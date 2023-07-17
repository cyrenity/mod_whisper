#ifndef __WEBSOCK_GLUE_H__
#define __WEBSOCK_GLUE_H__


void *SWITCH_THREAD_FUNC ws_tts_thread_run(switch_thread_t *thread, void *obj);
void ws_tts_thread_launch(whisper_tts_t *tech_pvt, switch_memory_pool_t *pool);
switch_status_t ws_tts_setup_connection(char * tts_server_uri, whisper_tts_t *tech_pvt, switch_memory_pool_t *pool);
void ws_tts_close_connection(whisper_tts_t *tech_pvt);

switch_status_t ws_send_text(kws_t *websocket, char *text) ;
switch_status_t ws_send_json(kws_t *websocket, ks_json_t *json_object) ;
switch_status_t whisper_get_final_transcription(whisper_t *context);
switch_status_t whisper_get_speech_synthesis(whisper_tts_t *context);

#endif
