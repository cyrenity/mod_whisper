#ifndef __MOD_WHISPER_H__
#define __MOD_WHISPER_H__

#include <switch.h>
#include <netinet/tcp.h>
#include <libks/ks.h>

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

struct {
	char *asr_server_url;
	char *tts_server_url;
	int return_json;
	int auto_reload;
	switch_memory_pool_t *pool;
	ks_pool_t *ks_pool;
} globals;

typedef struct {
	char *text;
	char *voice;
	int samplerate;
	const char *channel_uuid;
	switch_memory_pool_t *pool;
	switch_buffer_t *audio_buffer;
	kws_t *ws;
} whisper_tts_t;

#endif
