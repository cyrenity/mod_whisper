#ifndef __WEBSOCK_GLUE_H__
#define __WEBSOCK_GLUE_H__

#include "mod_whisper.h"
#include <libwebsockets.h>


void *SWITCH_THREAD_FUNC ws_tts_thread_run(switch_thread_t *thread, void *obj);
void ws_tts_thread_launch(whisper_tts_t *tech_pvt, switch_memory_pool_t *pool);
switch_status_t ws_tts_setup_connection(char * tts_server_uri, whisper_tts_t *tech_pvt, switch_memory_pool_t *pool);
void ws_tts_close_connection(whisper_tts_t *tech_pvt);

switch_status_t ws_asr_setup_connection(char * asr_server_uri, whisper_t *tech_pvt, switch_memory_pool_t *pool);
void *SWITCH_THREAD_FUNC ws_asr_thread_run(switch_thread_t *thread, void *obj);
void ws_asr_thread_launch(whisper_t *tech_pvt, switch_memory_pool_t *pool);
void ws_asr_close_connection(whisper_t *tech_pvt);

switch_status_t ws_send_binary(struct lws *websocket, void *data, int rlen); 

switch_status_t ws_send_text(struct lws *websocket, char *text) ;
switch_status_t ws_send_json(struct lws *websocket, ks_json_t *json_object) ;
switch_status_t whisper_get_final_transcription(whisper_t *context);
void whisper_fire_event(whisper_t *context, char * event_subclass);
switch_status_t whisper_get_speech_synthesis(whisper_tts_t *context);

#endif
