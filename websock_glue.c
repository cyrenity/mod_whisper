#include "mod_whisper.h"
#include "websock_glue.h"
#include <libwebsockets.h>

// libwebsocket protocols
static struct lws_protocols ws_tts_protocols[] = {
	{
		"WSBRIDGE",
		callback_ws_tts,
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
static struct lws_protocols ws_asr_protocols[] = {
	{
		"WSBRIDGE",
		callback_ws_asr,
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

//TTS Functions
int callback_ws_tts(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Websocket connection error\n");
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
switch_status_t ws_tts_setup_connection(char * tts_server_uri, whisper_tts_t *tech_pvt, switch_memory_pool_t *pool) {
	whisper_tts_t *context = (whisper_tts_t *) tech_pvt;
	int logs = LLL_USER | LLL_ERR | LLL_WARN;
	const char *prot;

	context->lws_info.port = CONTEXT_PORT_NO_LISTEN;
	context->lws_info.protocols = ws_tts_protocols;
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
    context->lws_ccinfo.protocol = ws_tts_protocols[0].name;

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

// thread for handling websocket connection
void *SWITCH_THREAD_FUNC ws_tts_thread_run(switch_thread_t *thread, void *obj) {
	whisper_tts_t *context = (whisper_tts_t *) obj;
	do {
		lws_service(context->lws_context, WS_TIMEOUT_MS);
	} while (context->started == WS_STATE_STARTED);
    return NULL;
}

void ws_tts_close_connection(whisper_tts_t *tech_pvt) {
	whisper_tts_t *context = (whisper_tts_t *) tech_pvt;
	lws_cancel_service(context->lws_context);
	context->started = WS_STATE_DESTROY;
	lws_context_destroy(context->lws_context);
}

//ASR Functions
int callback_ws_asr(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	whisper_t *context = (whisper_t *)lws_wsi_user(wsi);

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WebSockets client established. [%p]\n", (void *)wsi);
			context->wc_connected = TRUE;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "WS receiving data\n");
			if (!lws_frame_is_binary(context->wsi)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Text: %s \n", (char *)in);
				context->result_text = switch_safe_strdup((const char *)in); 
			}

			if (lws_is_final_fragment(context->wsi)) {
				lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)"seeya", 5);
				return -1;
			}
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Websocket connection error\n");
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

switch_status_t ws_asr_setup_connection(char * asr_server_uri, whisper_t *tech_pvt, switch_memory_pool_t *pool) {
	whisper_t *context = (whisper_t *) tech_pvt;
	int logs = LLL_USER | LLL_ERR | LLL_WARN ;
	const char *prot;

	memset(&context->lws_info, 0, sizeof(context->lws_info));
	memset(&context->lws_ccinfo, 0, sizeof(context->lws_ccinfo));
	
	context->lws_info.port = CONTEXT_PORT_NO_LISTEN;
	context->lws_info.protocols = ws_asr_protocols;
	context->lws_info.gid = -1;
	context->lws_info.uid = -1;

	lws_set_log_level(logs, NULL);

	context->lws_context = lws_create_context(&context->lws_info);

	if (context->lws_context == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Creating libwebsocket context failed\n");
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	if (lws_parse_uri(asr_server_uri, 
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
	context->lws_ccinfo.userdata = (whisper_t *) context;
    context->lws_ccinfo.protocol = ws_asr_protocols[0].name;

    context->wsi = lws_client_connect_via_info(&context->lws_ccinfo);

    if (context->wsi == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Websocket setup failed\n");
			return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	ws_asr_thread_launch(context, pool);

	while (!(context->wc_connected || context->wc_error)) {
		switch_sleep(10000);
	}	

	if (context->wc_error == TRUE) {
			ws_asr_close_connection(context);
			return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

void ws_asr_thread_launch(whisper_t *tech_pvt, switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	tech_pvt->started = WS_STATE_STARTED;
	switch_thread_create(&thread, thd_attr, ws_asr_thread_run, tech_pvt, pool);
}

// thread for handling websocket connection
void *SWITCH_THREAD_FUNC ws_asr_thread_run(switch_thread_t *thread, void *obj) {
	//int n;
	whisper_t *context = (whisper_t *) obj;

	do {
		lws_service(context->lws_context, WS_TIMEOUT_MS);
	} while (context->started == WS_STATE_STARTED);

	return NULL;
}

void ws_asr_close_connection(whisper_t *tech_pvt) {
	whisper_t *context = (whisper_t *) tech_pvt;
	lws_cancel_service(context->lws_context);
	context->started = WS_STATE_DESTROY;
	lws_context_destroy(context->lws_context);
}



switch_status_t ws_send_binary(struct lws *websocket, void *data, int rlen) 
{	
	
	unsigned char buffer[LWS_PRE + rlen];
	unsigned char *p = &buffer[LWS_PRE];

	memcpy(p, data, rlen);

	if (lws_write(websocket, p, rlen, LWS_WRITE_BINARY) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to write message\n");
		return SWITCH_STATUS_BREAK;
	}		
	return SWITCH_STATUS_SUCCESS;
}


switch_status_t ws_send_text(struct lws *websocket, char *text) 
{
	unsigned char buffer[LWS_SEND_BUFFER_PRE_PADDING + strlen(text) + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buffer[LWS_SEND_BUFFER_PRE_PADDING];

	memcpy(p, text, strlen(text));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "text: %s\n", text);
	
	if (lws_write(websocket, p, strlen(text), LWS_WRITE_TEXT) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to write message\n");
		return SWITCH_STATUS_BREAK;
	}		
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t ws_send_json(struct lws *websocket, ks_json_t *json_object) 
{
	char *request_str = NULL;

	request_str = ks_json_print_unformatted(json_object);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending json string to websocket server %s\n", request_str);

	return ws_send_text(websocket, request_str);
}

switch_status_t whisper_get_final_transcription(whisper_t *context)
{
	ks_json_t *req = ks_json_create_object();

	ks_json_add_string_to_object(req, "eof", "true");

	if (ws_send_json(context->wsi, req) != SWITCH_STATUS_SUCCESS) {
		ks_json_delete(&req);
		return SWITCH_STATUS_BREAK;
	}

	ks_json_delete(&req);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t whisper_get_speech_synthesis(whisper_tts_t *context)
{
	int poll_result;
	kws_opcode_t oc;
	uint8_t *rdata;
	int rlen;

	if (ws_send_text(context->wsi, context->text) != SWITCH_STATUS_SUCCESS) {
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

	return SWITCH_STATUS_SUCCESS;
}
