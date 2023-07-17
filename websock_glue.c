#include "mod_whisper.h"
#include "websock_glue.h"
#include <libwebsockets.h>



// libwebsocket protocols
static struct lws_protocols WSBRIDGE_protocols[] = {
	{
		"WSBRIDGE",
		callback_ws,
		0,
	/* rx_buffer_size Docs:
	 *
	 * If you want atomic frames delivered to the callback, you should set this to the size of the biggest legal frame that you support. 
	 * If the frame size is exceeded, there is no error, but the buffer will spill to the user callback when full, which you can detect by using lws_remaining_packet_payload. 
	 *
	 * * */
		RX_BUFFER_SIZE,		
	},
	{ NULL, NULL, 0, 0 } /* end */
};

// thread for handling websocket connection
void *SWITCH_THREAD_FUNC ws_tts_thread_run(switch_thread_t *thread, void *obj) {
	//int n;
	whisper_tts_t *context = (whisper_tts_t *) obj;
	do {
		lws_service(context->lws_context, WS_TIMEOUT_MS);
	} while (context->started == WS_STATE_STARTED);
    return NULL;
}


void ws_tts_thread_launch(whisper_tts_t *tech_pvt, switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	tech_pvt->started = WS_STATE_STARTED;
	switch_thread_create(&thread, thd_attr, ws_tts_thread_run, tech_pvt, pool);
}

void ws_tts_close_connection(whisper_tts_t *tech_pvt) {
	whisper_tts_t *context = (whisper_tts_t *) tech_pvt;
	lws_cancel_service(context->lws_context);
	context->started = WS_STATE_DESTROY;
	lws_context_destroy(context->lws_context);
}

switch_status_t ws_tts_setup_connection(char * tts_server_uri, whisper_tts_t *tech_pvt, switch_memory_pool_t *pool) {
	whisper_tts_t *context = (whisper_tts_t *) tech_pvt;
	int logs = LLL_USER | LLL_ERR | LLL_WARN;
	const char *prot;

	context->lws_info.port = CONTEXT_PORT_NO_LISTEN;
	context->lws_info.protocols = WSBRIDGE_protocols;
	context->lws_info.gid = -1;
	context->lws_info.uid = -1;

	lws_set_log_level(logs, NULL);
	
	context->lws_context = lws_create_context(&context->lws_info);

	if (context->lws_context == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Creating libwebsocket context failed\n");
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	if (lws_parse_uri(tts_server_uri, 
		&prot, 
		&context->lws_ccinfo.address, 
		&context->lws_ccinfo.port, 
		&context->lws_ccinfo.path)) {
		/* XXX Error */
		return SWITCH_CAUSE_INVALID_URL;
	}

	if (!strcmp(prot, "ws")) {
		context->lws_ccinfo.ssl_connection = 0;
	} else {
		context->lws_ccinfo.ssl_connection = 2;
	}
	
    context->lws_ccinfo.context = context->lws_context;
    context->lws_ccinfo.host = lws_canonical_hostname(context->lws_context);
    context->lws_ccinfo.origin = "origin";
	context->lws_ccinfo.userdata = (whisper_tts_t *) context;
    context->lws_ccinfo.protocol = WSBRIDGE_protocols[0].name;

    context->wsi = lws_client_connect_via_info(&context->lws_ccinfo);

    if (context->wsi == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Websocket setup failed\n");
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	ws_tts_thread_launch(context, pool);

	while (!(context->wc_connected || context->wc_error)) {
		usleep(30000);
	}

	if (context->wc_error == TRUE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Websocket connect failed\n");
			return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	whisper_tts_t *context = (whisper_tts_t *)lws_wsi_user(wsi);

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WebSockets client established. [%p]\n", (void *)wsi);
			context->wc_connected = TRUE;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WS receiving data\n");

			if (lws_frame_is_binary(context->wsi)) {
				switch_buffer_write(context->audio_buffer, in, len);				
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "WebSockets RX: Frame not received in binary mode");
			}

			if (lws_is_final_fragment(context->wsi)) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)"seeya", 5);
				return -1;
			}
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Websocket connection error\n");
			context->wc_error = TRUE;
			return -1;
		    break;        
		case LWS_CALLBACK_CLIENT_CLOSED:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Websocket client connection closed.\n");
			context->started = WS_STATE_DESTROY;
			return -1;
		    break;    
        default:
            break;
    }
    return 0;
}

switch_status_t ws_send_text(kws_t *websocket, char *text) 
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "text: %s\n", text);
	
	if (kws_write_frame(websocket, WSOC_TEXT, text, strlen(text)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to send string");

		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t ws_send_json(kws_t *websocket, ks_json_t *json_object) 
{
	char *request_str = NULL;

	request_str = ks_json_print_unformatted(json_object);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending json string to websocket server %s\n", request_str);

	return ws_send_text(websocket, request_str);
}

switch_status_t whisper_get_final_transcription(whisper_t *context)
{
	int poll_result;
	kws_opcode_t oc;
	uint8_t *rdata;
	int rlen;
	ks_json_t *req = ks_json_create_object();

	ks_json_add_string_to_object(req, "eof", "true");

	if (ws_send_json(context->ws, req) != SWITCH_STATUS_SUCCESS) {
		ks_json_delete(&req);
		return SWITCH_STATUS_BREAK;
	}

	ks_json_delete(&req);

	while (1) {
		poll_result = kws_wait_sock(context->ws, 50, KS_POLL_READ | KS_POLL_ERROR);
		if (poll_result == KS_POLL_READ) { 
			break; 
		}
	}

	rlen = kws_read_frame(context->ws, &oc, &rdata);

	if (rlen < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Message length is not acceptable");
		return SWITCH_STATUS_BREAK;
	}

	context->result_text = switch_safe_strdup((const char *)rdata);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t whisper_get_speech_synthesis(whisper_tts_t *context)
{
	int poll_result;
	kws_opcode_t oc;
	uint8_t *rdata;
	int rlen;

	if (ws_send_text(context->ws, context->text) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_BREAK;
	}

	while (1) {
		poll_result = kws_wait_sock(context->ws, 50, KS_POLL_READ | KS_POLL_ERROR);
		if (poll_result == KS_POLL_READ) { 
			break; 
		}
	}

	rlen = kws_read_frame(context->ws, &oc, &rdata); 

	if (rlen < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Message length is not acceptable");
		return SWITCH_STATUS_BREAK;
	}

	//switch_buffer_write(context->audio_buffer, rdata, rlen);

	return SWITCH_STATUS_SUCCESS;
}
