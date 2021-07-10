#include <spa/utils/defs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <pipewire/pipewire.h>
#include <pipewire/main-loop.h>
#include <spa/param/audio/format-utils.h>
#include <spa/debug/buffer.h>
#include <spa/debug/mem.h>

#include "main.h"
#include "../../shared.h"

#define M_PI_M2 ( M_PI + M_PI )

#define DEFAULT_RATE            44100
#define DEFAULT_CHANNELS        2
#define DEFAULT_VOLUME          0.7

static int n = 0;

struct pwdata {
	struct pw_main_loop *loop;
	struct pw_stream *stream;
	struct audio_data *audio;
	double accumulator;
};

static void on_process(void *userdata)
{
	struct pwdata *pwdata = userdata;
	struct audio_data *audio = pwdata->audio;
	struct pw_buffer *b;
	struct spa_buffer *buf;
	int i, n_frames, stride;
	int16_t *audio_buffer;

	if ((b = pw_stream_dequeue_buffer(pwdata->stream)) == NULL) {
		pw_log_warn("out of buffers: %m");
		return;
	}

	// list of SPA buffers 
	buf = b->buffer;

	// Test the first buffer of the first SPA buffer
	if ((audio_buffer = buf[0].datas[0].data) == NULL)
		return;

	// math
	stride = audio->channels * sizeof(int16_t);
	n_frames = buf[0].datas[0].chunk[0].size/stride;

	// assume that the data is located in the first SPA and the
	// first data chunk
	audio_buffer = buf[0].datas[0].data;

	// audio conversion magic
	switch(audio->channels) {
		case 1:
			for(i=0; i<n_frames; i++) {
				audio->audio_out_l[n] = audio_buffer[i];
				n = (n+1) % audio->inputsize;
			}
			break;
		case 2:
			for(i=0; i<n_frames; i++) {
				audio->audio_out_l[n] = *audio_buffer++;
				audio->audio_out_r[n] = *audio_buffer++;
				n = (n+1) % audio->inputsize;
			}
			break;
	}

	// inform pipewire that we got the thing
	pw_stream_queue_buffer(pwdata->stream, b);

	// getfo
	if(audio->terminate)
		pw_main_loop_quit(pwdata->loop);
}

static const struct pw_stream_events stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.process = on_process,
};

EXP_FUNC void* xavaInput(void *audiodata) {
	struct pwdata pwdata;
	const struct spa_pod *params[1];
	uint16_t buffer[1024];
	struct spa_pod_builder b;

	pwdata.audio = audiodata;

	b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

	pw_init(NULL, NULL);

	xavaSpam("Compiled with libpipewire %s",
			pw_get_headers_version());
	xavaSpam("Linked with libpipewire %s",
			pw_get_library_version());

	pwdata.loop = pw_main_loop_new(NULL);
	pwdata.stream = pw_stream_new_simple(
			pw_main_loop_get_loop(pwdata.loop),
			PACKAGE,
			pw_properties_new(
				PW_KEY_MEDIA_TYPE, "Audio",
				PW_KEY_MEDIA_CATEGORY, "Monitor",
				PW_KEY_MEDIA_ROLE, "DSP",
				NULL),
			&stream_events,
			&pwdata);

	params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
			&SPA_AUDIO_INFO_RAW_INIT(
				.format = SPA_AUDIO_FORMAT_S16,
				.channels = pwdata.audio->channels,
				.rate = DEFAULT_RATE ));

	pw_stream_connect(pwdata.stream, PW_DIRECTION_INPUT, PW_ID_ANY,
			PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
			PW_STREAM_FLAG_RT_PROCESS, params, 1);

	pw_main_loop_run(pwdata.loop);

	pw_stream_destroy(pwdata.stream);
	pw_main_loop_destroy(pwdata.loop);

	return 0;
}

EXP_FUNC void xavaInputHandleConfiguration(void *data1, void *data2) {
	dictionary *ini = (dictionary*) data1;
	struct audio_data *audio = (struct audio_data*) data2; 
	audio->rate = 44100;

	// the current implementation is dumb, so this'll just get ignored
	audio->source = (char *)iniparser_getstring(ini, "input:source", "Loopback");
}

