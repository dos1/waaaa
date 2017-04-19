/*! \file empty.c
 *  \brief Empty gamestate.
 */
/*
 * Copyright (c) Sebastian Krzyszkowiak <dos@dosowisko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "../common.h"
#include <allegro5/allegro_color.h>
#include <libsuperderpy.h>
#include <fftw3.h>
#include <math.h>

#define SAMPLE_RATE 44100

#define FFT_SAMPLES 8192

#define BARS_NUM 8192/2
#define BARS_WIDTH 4
#define BARS_OFFSET 8

const int BALL_WIDTH = 3;
const int BALL_HEIGHT = 3;

// TODO: play with moving window
// TODO: mic volume / output fft normalization?
// TODO: bar offset with mic

struct GamestateResources {
		// This struct is for every resource allocated and used by your gamestate.
		// It gets created on load and then gets passed around to all other function calls.
		ALLEGRO_FONT *font;
		ALLEGRO_AUDIO_STREAM *audio;
		ALLEGRO_AUDIO_RECORDER *recorder;
		ALLEGRO_MIXER *mixer;
		float bars[BARS_NUM];
		float fft[SAMPLE_RATE / 2 + 1];
		float ringbuffer[SAMPLE_RATE];
		int ringpos;
		float fftbuffer[FFT_SAMPLES];

		ALLEGRO_BITMAP *pixelator, *blurer;
		ALLEGRO_SHADER *shader;

		int blink_counter;
		float rotation;
		int screamtime;
		bool inmenu;
		bool inmulti;
		float vx, vy, x, y;

		int shakin_dudi;
		int score1, score2;
		char level[80][45];

		struct Game *game;
		bool music_mode;
};

int Gamestate_ProgressCount = 1; // number of loading steps as reported by Gamestate_Load

void FFT(void *buffer, unsigned int samples, void* userdata);

void Gamestate_Logic(struct Game *game, struct GamestateResources* data) {
	// Called 60 times per second. Here you should do all your game logic.
	int end = data->ringpos - FFT_SAMPLES;
	if (end < 0) {
		end += SAMPLE_RATE;
	}
	int i = 0;
	for (int pos=end; pos != data->ringpos; pos++) {
		data->fftbuffer[i] = data->ringbuffer[pos];
		if (fabs(data->fftbuffer[i]) > 1) {
			printf("fftbuffer[%d] = ringbuffer[%d] = %f\n", i, pos, data->fftbuffer[i]);
		}
		i++;
		if (pos == SAMPLE_RATE-1) {
			pos = -1;
		}
	}
	FFT(data->fftbuffer, FFT_SAMPLES, data);

	for (int i=0; i<8192/2; i++) {
		data->bars[i] = 0;
		int width = 1;
		for (int j=i*width; j<i*width+width; j++) {
			data->bars[i] += data->fft[j+8*width];
		}
		data->bars[i] /= width;

	}
}

void Gamestate_Draw(struct Game *game, struct GamestateResources* data) {
	// Called as soon as possible, but no sooner than next Gamestate_Logic call.
	// Draw everything to the screen here.

	al_clear_to_color(al_map_rgb(0,0,0));

	al_set_target_bitmap(data->pixelator);

	al_clear_to_color(al_color_hsv(fabs(sin(data->rotation/360.0)) * 360, 0.75, 0.5 + sin(data->rotation/20.0)/20.0));
	al_draw_filled_rectangle(0, 180-data->screamtime, 320, 180, al_map_rgb(255,255,255)); // scream bar
	al_draw_filled_rectangle(0, 0, 320, 180, al_map_rgba(data->screamtime, data->screamtime, data->screamtime, data->screamtime));

	al_set_target_backbuffer(game->display);
	al_use_shader(data->shader);
	al_set_shader_int("scaleFactor", 2);
	al_draw_bitmap(data->pixelator, 0, 0, 0);
	al_use_shader(NULL);

	al_set_target_bitmap(data->pixelator);
	al_clear_to_color(al_map_rgba(0,0,0,0));

	// BAR DRAWING
	int width = 320/BARS_NUM;
	if (width==0) {
		width = 1;
	}
	width *= BARS_WIDTH;
		for (int i=BARS_OFFSET; i<BARS_NUM; i++) {
				int a = i-BARS_OFFSET;
				al_draw_filled_rectangle(a*width, (int)(178 - data->bars[i]*90), a*width+width, 180, al_map_rgb(255,255,255));
	}

    /*
    // WAVEFORM DRAWING
    width=1;
	for (int i=0; i<4096; i++) {
		al_draw_filled_rectangle(i*width, 180/2 - data->fftbuffer[i]*180/2, i*width+width, 180/2, al_map_rgb(255,255,0));
	}
	al_draw_textf(data->font, al_map_rgb(255,255,255), 10, 10, ALLEGRO_ALIGN_LEFT, "%d", data->ringpos);
		*/

	// FINAL DRAWING
	al_set_target_bitmap(data->blurer);
	al_clear_to_color(al_map_rgba(0,0,0,0));

	al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 0, 0, 320/4, 180/4, 0);

	al_set_target_backbuffer(game->display);

	float s = data->blink_counter / 5.0;
	ALLEGRO_COLOR tint = al_map_rgba(32*s,32*s,32*s,32*s);

	float rot = sin(data->rotation / 20.0) / 20.0;

	float scale = 1 - pow((fabs((320/2) - data->x) / (320/2.0)),2) * 0.1;

	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 40 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 40 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);

	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 200, 1.1*4*scale, 1.1*4*scale, rot, 0);


	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 120 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 120 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);

	float offset = data->blink_counter / 2.0 * (rand() / (float)INT_MAX);

	al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(0,192,192, 192), 320/2, 180*(3/4), 320/2 - 2*offset, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(192,0,0, 192), 320/2, 180*(3/4), 320/2 + 2*offset, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);

	al_use_shader(data->shader);
	al_set_shader_int("scaleFactor", 1);

	al_draw_scaled_rotated_bitmap(data->pixelator, 320/2, 180*(3/4), 320/2, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);

	al_use_shader(NULL);

}

void Gamestate_Draw_Old(struct Game *game, struct GamestateResources* data) {
	// Called as soon as possible, but no sooner than next Gamestate_Logic call.
	// Draw everything to the screen here.
	al_clear_to_color(al_map_rgb(0,0,0));

	al_set_target_bitmap(data->pixelator);

	al_clear_to_color(al_color_hsv(fabs(sin(data->rotation/360.0)) * 360, 0.75, 0.5 + sin(data->rotation/20.0)/20.0));
	//al_draw_filled_rectangle(0, 180-data->screamtime, 320, 180, al_map_rgb(255,255,255));
	al_draw_filled_rectangle(0, 0, 320, 180, al_map_rgba(data->screamtime, data->screamtime, data->screamtime, data->screamtime));

al_set_target_backbuffer(game->display);
al_use_shader(data->shader);
al_set_shader_int("scaleFactor", 2);
al_draw_bitmap(data->pixelator, 0, 0, 0);
al_use_shader(NULL);

al_set_target_bitmap(data->pixelator);
al_clear_to_color(al_map_rgba(0,0,0,0));


	if (data->blink_counter < 50) {
		//al_draw_text(data->font, al_map_rgb(255,255,255), game->viewport.width / 2, game->viewport.height / 2,
			//           ALLEGRO_ALIGN_CENTRE, "Nothing to see here, move along!");
	}

//if (!data->out) { return; }

for (int x=0; x<80; x++) {
	for (int y=0; y<45; y++) {
		if (data->level[x][y]=='O') {
			al_draw_filled_rectangle(x*4, y*4, x*4+4, y*4+4, al_map_rgb(255,255,255));
		}
		if (data->level[x][y]=='X') {
			al_draw_filled_rectangle(x*4, y*4, x*4+4, y*4+4, al_map_rgba(0,0,32,32));
		}
		if (data->level[x][y]=='Y') {
			al_draw_filled_rectangle(x*4, y*4, x*4+4, y*4+4, al_map_rgba(32,0,0,32));
		}
	}
}
{

int x = data->x / 4; int y = data->y / 4;
 if (!data->inmenu)
  al_draw_filled_rectangle(x*4, y*4, x*4+4, y*4+4, al_map_rgba(16,16,16,16));
}
float x = 0; float width = 8;

int OFF = 0;
if (data->inmenu) OFF = 15;

for (int i = 8 + OFF; i <= 50 + OFF; i++) {
width -= 0.02;

// HERE DRAWING
//al_draw_filled_rectangle(x, 180 - data->out[i][0] * 10, x+width, 180, al_map_rgb(255,255,255));
	x+= width;
}

al_draw_filled_rectangle(0, 180-5, 320, 180, al_map_rgb(255,255,255));

if (!data->inmenu){
	al_draw_filled_rectangle(data->x - BALL_WIDTH, data->y - BALL_HEIGHT, data->x + BALL_WIDTH, data->y + BALL_HEIGHT,
													 //al_color_hsv(fabs(sin((data->rotation/360.0)+ALLEGRO_PI/4.0)) * 360, 1, 1));

													 al_map_rgb(255,255,0));
}
if (data->inmulti) {
	al_draw_textf(data->font, al_map_rgb(255,255,255), 320/2, 72, ALLEGRO_ALIGN_CENTER, data->shakin_dudi ? (((data->shakin_dudi / 10) % 2) ? "" : "SCORE!") : "WAAAA");
	al_draw_textf(data->font, al_map_rgb(255,255,255), 320/2, 82, ALLEGRO_ALIGN_CENTER, "%d:%d", data->score1, data->score2);
}

	al_set_target_bitmap(data->blurer);
	al_clear_to_color(al_map_rgba(0,0,0,0));

	al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 0, 0, 320/4, 180/4, 0);


al_set_target_backbuffer(game->display);
//al_draw_bitmap(data->blurer, 0, 0, 0);

float s = data->blink_counter / 5.0;
ALLEGRO_COLOR tint = al_map_rgba(32*s,32*s,32*s,32*s);

float rot = sin(data->rotation / 20.0) / 20.0;

float scale = 1 - pow((fabs((320/2) - data->x) / (320/2.0)),2) * 0.1;

al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 40, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 40 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 40 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);

al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 200, 1.1*4*scale, 1.1*4*scale, rot, 0);


al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 120, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 - 2, 180/2 - 120 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320/4/2, (180/4)*(3/4), 320/2 + 2, 180/2 - 120 - 3, 1.1*4*scale, 1.1*4*scale, rot, 0);

float offset = data->blink_counter / 2.0 * (rand() / (float)INT_MAX);

al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(0,192,192, 192), 320/2, 180*(3/4), 320/2 - 2*offset, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);
al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(192,0,0, 192), 320/2, 180*(3/4), 320/2 + 2*offset, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);

al_use_shader(data->shader);
al_set_shader_int("scaleFactor", 1);

al_draw_scaled_rotated_bitmap(data->pixelator, 320/2, 180*(3/4), 320/2, 180/2 - 120, 1.1*scale, 1.1*scale, rot, 0);

al_use_shader(NULL);

}


float max_max = 0;

void MixerPostprocess(void *buffer, unsigned int samples, void* userdata) {
	// REMEMBER: don't use any drawing code inside this function
	// PrintConsole etc. NOT ALLOWED!
	float *buf = buffer;
	struct GamestateResources *data = userdata;

	int end = data->ringpos + samples;
	if (end >= SAMPLE_RATE) {
		end -= SAMPLE_RATE;
	}

	// TODO: in case of mono signal, detect silent channel and ignore

	int i = 0;
	for (int pos=data->ringpos; pos != end; pos++) {
		data->ringbuffer[pos] = (buf[i] + buf[i+1]) / 2;
		i+=2;
		if (pos == SAMPLE_RATE-1) {
			pos=-1;
		}
	}
	data->ringpos = end;
}

void FFT(void *buffer, unsigned int samples, void* userdata) {
	float *buf = buffer;
	struct GamestateResources *data = userdata;
	double *in; fftw_complex *out;
	in = (double*) fftw_malloc(sizeof(double) * samples);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (samples/2 + 1));

	float *window = CreateHanningWindow(samples, false);
	float min = 0, max = 0;
	for (int i = 0; i < samples; i++) {
		if (buf[i] > max) max = buf[i];
		if (buf[i] < min) min = buf[i];
	}
	min *= -1;
	if (min > max) max = min;
	if (max > max_max) max_max = max;

	for (int i = 0; i < samples; i++) {
		in[i] = (buf[i] * window[i]) / max_max;
		//buf[i] = 0;
		//PrintConsole(data->game, "%d: %f", i, buf[i]);
	}
	printf("samples: %d, min: %f, max: %f, max_max: %f\n", samples, min, max, max_max);
	fflush(stdout);
	free(window);

	if (max < max_max) {
		max_max -= (max_max - max) / 1024.0;
	}

	fftw_plan p = fftw_plan_dft_r2c_1d(samples, in, out, FFTW_ESTIMATE);
	fftw_execute(p);

	int bar_width = (samples / 2 + 1) / BARS_NUM;
	if (bar_width == 0) {
		bar_width = 1;
	}

	for (int i = 0; i < (samples / 2 + 1); i++) {
		out[i][0] *= 1./samples;
		out[i][1] *= 1./samples;

		double val = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
		if (data->music_mode) {
			val = sqrt(sqrt(val)) * 2;
		} else {
			val *= 100;
			if (val > 1) {
				val = 1;
			}
		}
		data->fft[i] = val;
	}

	fftw_destroy_plan(p);
	fftw_free(in);
	fftw_free(out);
}

void Gamestate_ProcessEvent(struct Game *game, struct GamestateResources* data, ALLEGRO_EVENT *ev) {
	// Called for each event in Allegro event queue.
	// Here you can handle user input, expiring timers etc.
	if ((ev->type==ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE)) {
		UnloadCurrentGamestate(game); // mark this gamestate to be stopped and unloaded
		// When there are no active gamestates, the engine will quit.
	}

	if (ev->type == ALLEGRO_EVENT_AUDIO_RECORDER_FRAGMENT) {
		ALLEGRO_AUDIO_RECORDER_EVENT *re = al_get_audio_recorder_event(ev);
		MixerPostprocess(re->buffer, re->samples, data);
	}
}

void* Gamestate_Load(struct Game *game, void (*progress)(struct Game*)) {
	// Called once, when the gamestate library is being loaded.
	// Good place for allocating memory, loading bitmaps etc.
	struct GamestateResources *data = malloc(sizeof(struct GamestateResources));
	data->font = al_create_builtin_font();
	progress(game); // report that we progressed with the loading, so the engine can draw a progress bar

	for (int i=0; i<SAMPLE_RATE; i++) {
		data->ringbuffer[i] = 0;
	}

		data->music_mode = false;

	data->mixer = al_create_mixer(SAMPLE_RATE, ALLEGRO_AUDIO_DEPTH_FLOAT32, ALLEGRO_CHANNEL_CONF_2);
	al_attach_mixer_to_mixer(data->mixer, game->audio.music);

	data->audio = al_load_audio_stream(GetDataFilePath(game, "waaaa.flac"), 4, 4096);
	//al_register_event_source(game->_priv.event_queue, al_get_audio_stream_event_source(data->audio));
	al_set_audio_stream_playing(data->audio, true);
	al_set_audio_stream_playmode(data->audio, ALLEGRO_PLAYMODE_LOOP);
	al_attach_audio_stream_to_mixer(data->audio, data->mixer);
	al_set_audio_stream_gain(data->audio, 1.188);

	if (data->music_mode) {
		al_set_mixer_postprocess_callback(data->mixer, *MixerPostprocess, data);
	}

	data->recorder = al_create_audio_recorder(128, 1024, SAMPLE_RATE,
	                                   ALLEGRO_AUDIO_DEPTH_FLOAT32, ALLEGRO_CHANNEL_CONF_2);

	if (!data->music_mode) {
		al_register_event_source(game->_priv.event_queue, al_get_audio_recorder_event_source(data->recorder));
	}

	data->pixelator = al_create_bitmap(320, 180);
	data->blurer = al_create_bitmap(320/4, 180/4);

	data->shader = al_create_shader(ALLEGRO_SHADER_GLSL);
	PrintConsole(game, "VERTEX: %d", al_attach_shader_source_file(data->shader, ALLEGRO_VERTEX_SHADER, GetDataFilePath(game, "vertex.glsl")));
	PrintConsole(game, "%s", al_get_shader_log(data->shader));
	PrintConsole(game, "PIXEL: %d", al_attach_shader_source_file(data->shader, ALLEGRO_PIXEL_SHADER, GetDataFilePath(game, "pixel.glsl")));
	PrintConsole(game, "%s", al_get_shader_log(data->shader));
	al_build_shader(data->shader);

	data->game = game;

	return data;
}

void Gamestate_Unload(struct Game *game, struct GamestateResources* data) {
	// Called when the gamestate library is being unloaded.
	// Good place for freeing all allocated memory and resources.
	al_destroy_font(data->font);
	al_destroy_audio_stream(data->audio);
	al_destroy_mixer(data->mixer);
	al_destroy_audio_recorder(data->recorder);
	free(data);
}

void Gamestate_Start(struct Game *game, struct GamestateResources* data) {
	// Called when this gamestate gets control. Good place for initializing state,
	// playing music etc.
	data->ringpos = 0;
	al_start_audio_recorder(data->recorder);
}

void Gamestate_Stop(struct Game *game, struct GamestateResources* data) {
	// Called when gamestate gets stopped. Stop timers, music etc. here.
	al_stop_audio_recorder(data->recorder);
}

void Gamestate_Pause(struct Game *game, struct GamestateResources* data) {
	// Called when gamestate gets paused (so only Draw is being called, no Logic not ProcessEvent)
	// Pause your timers here.
}

void Gamestate_Resume(struct Game *game, struct GamestateResources* data) {
	// Called when gamestate gets resumed. Resume your timers here.
}

// Ignore this for now.
// TODO: Check, comment, refine and/or remove:
void Gamestate_Reload(struct Game *game, struct GamestateResources* data) {}
