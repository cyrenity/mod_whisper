#ifndef __MOD_WHISPER_H__
#define __MOD_WHISPER_H__

#include <switch.h>
#include <netinet/tcp.h>
#include <libks/ks.h>
#include <libwebsockets.h>

#define AUDIO_BLOCK_SIZE 3200
#define SPEECH_BUFFER_SIZE 49152
#define SPEECH_BUFFER_SIZE_MAX 4194304

typedef enum {
	ASRFLAG_READY = (1 << 0),
	ASRFLAG_INPUT_TIMERS = (1 << 1),
	ASRFLAG_START_OF_SPEECH = (1 << 2),
	ASRFLAG_RETURNED_START_OF_SPEECH = (1 << 3),
	ASRFLAG_NOINPUT_TIMEOUT = (1 << 4),
	ASRFLAG_RESULT = (1 << 5),
	ASRFLAG_RETURNED_RESULT = (1 << 6),
	ASRFLAG_TIMEOUT = (1 << 7)
} whisper_flag_t;

typedef struct {
	uint32_t flags;
	char *result_text;
	double result_confidence;
	uint32_t thresh;
	uint32_t silence_ms;
	uint32_t voice_ms;
	int no_input_timeout;
	int speech_timeout;
	switch_bool_t start_input_timers;
	switch_time_t no_input_time;
	switch_time_t speech_time;
	char *grammar;
	char *channel_uuid;
	switch_vad_t *vad;
	switch_buffer_t *audio_buffer;
	switch_mutex_t *mutex;
	kws_t *ws;
	int partial;
} whisper_t;



typedef struct {
	char *text;
	char *voice;
	int samplerate;
	const char *channel_uuid;
	switch_memory_pool_t *pool;
	switch_buffer_t *audio_buffer;
	kws_t *ws;
	/* thread related members */
	switch_mutex_t *wsi_mutex;
	int started;
	switch_bool_t wc_connected;
	struct lws *wsi_wsbridge;
	struct lws_context *lws_context;
	struct lws_context_creation_info lws_info;
	struct lws_client_connect_info lws_ccinfo;
} whisper_tts_t;

// static void wsbridge_thread_launch(whisper_tts_t *context);
// static void *SWITCH_THREAD_FUNC wsbridge_thread_run(switch_thread_t *thread, void *obj);

// static int wsbridge_callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
// 								void *user, void *in, size_t len);
 

 
#define WSBRIDGE_STATE_STARTED 0
#define WSBRIDGE_STATE_DESTROY 1
#define WS_TIMEOUT_MS 50  /* same as ptime on the RTP side , lws_service()*/
#endif
