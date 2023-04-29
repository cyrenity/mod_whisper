#include "websock_glue.h"


static switch_status_t ws_send_text(kws_t *websocket, char *text) 
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "text: %s\n", text);
	
	if (kws_write_frame(websocket, WSOC_TEXT, text, strlen(text)) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to send string");

		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t ws_send_json(kws_t *websocket, ks_json_t *json_object) 
{
	char *request_str = NULL;

	request_str = ks_json_print_unformatted(json_object);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending json string to websocket server %s\n", request_str);

	return ws_send_text(websocket, request_str);
}

static switch_status_t whisper_get_final_transcription(whisper_t *context)
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




static switch_status_t whisper_get_speech_synthesis(whisper_tts_t *context)
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

	switch_buffer_write(context->audio_buffer, rdata, rlen);

	return SWITCH_STATUS_SUCCESS;
}