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
#include "fftw3.h"
#include <math.h>

const int WIDTH = 3;
const int HEIGHT = 3;


const ALLEGRO_AUDIO_DEPTH audio_depth = ALLEGRO_AUDIO_DEPTH_UINT8;
typedef uint8_t* audio_buffer_t;
const uint8_t sample_center = 128;
const int8_t min_sample_val = 0x80;
const int8_t max_sample_val = 0x7f;
const int sample_range = 0xff;
const int sample_size = 1;
/* How many samples do we want to process at one time?
	 Let's pick a multiple of the width of the screen so that the
	 visualization graph is easy to draw. */
const unsigned int samples_per_fragment = 4096;

/* Frequency is the quality of audio. (Samples per second.) Higher
	 quality consumes more memory. For speech, numbers as low as 8000
	 can be good enough. 44100 is often used for high quality recording. */
const unsigned int frequency = 44100;

/* The playback buffer specs don't need to match the recording sizes.
 * The values here are slightly large to help make playback more smooth.
 */
const unsigned int playback_fragment_count = 4;
const unsigned int playback_samples_per_fragment = 4096;
struct GamestateResources {
		// This struct is for every resource allocated and used by your gamestate.
		// It gets created on load and then gets passed around to all other function calls.
		ALLEGRO_FONT *font;
		int blink_counter;
int samples;
float rotation;
int screamtime;
bool inmenu;
bool inrecording;
bool inmulti;
    ALLEGRO_AUDIO_RECORDER *r;
struct Game *game;
    ALLEGRO_BITMAP *pixelator, *blurer;
ALLEGRO_SHADER *shader;
    float vx, vy, x, y;
fftw_complex *out;

ALLEGRO_SAMPLE_INSTANCE *point;
ALLEGRO_SAMPLE *point_sample;

ALLEGRO_AUDIO_STREAM *audio;

ALLEGRO_AUDIO_STREAM *recording;

uint8_t rec_buffer[4096*2];
int rec_buffer_pos;

int shakin_dudi;

int score1, score2;

  char level[80][45];

};
audio_buffer_t buffer = NULL;
int prev = 0;

int Gamestate_ProgressCount = 1; // number of loading steps as reported by Gamestate_Load
void loadlevel(struct Game *game, struct GamestateResources *data, char* name);

void nothing(void *buf, unsigned int samples, struct GamestateResources* data) {}

void handleaudio(uint8_t *buf, unsigned int samples, struct GamestateResources* data) {
	//PrintConsole(data->game, "samples %d", samples);

	if (data->inrecording && data->inmenu) {
		uint8_t max = 0;
		for (int i=0; i<samples; i++) {
			if (buf[i] > max) {
				max = buf[i];
			}
		}
		printf("max: %d\n", max);
		if (max >= 140) {
			data->screamtime+=20;
		} else {
			data->screamtime-=50;
		}
		if(data->screamtime > 255) {
			data->screamtime = 255;

			data->inmenu = false;
			al_play_sample_instance(data->point);
			data->inmulti = true;
			loadlevel(data->game, data, "levels/multi.lvl");



		}
		if (data->screamtime < 0) data->screamtime = 0;
		return;
	}
	if (!data->inmenu && !data->inrecording) return;

if (!data->inmenu) {
	data->screamtime-=50;
if (data->screamtime < 0) data->screamtime = 0;
}
  audio_buffer_t input = (audio_buffer_t) buf;

	if (samples > 4096) samples = 4096;

	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
	buffer = malloc(sizeof(uint8_t) * samples);
	memcpy(buffer, input, sizeof(uint8_t) * samples);

	data->samples = samples;
}


float *hanning(int N, short itype)
{
	  int half, i, idx, n;
		float *w;

		w = (float*) calloc(N, sizeof(float));
		memset(w, 0, N*sizeof(float));

		if(itype==1)    //periodic function
			  n = N-1;
		else
			  n = N;

		if(n%2==0)
		{
			  half = n/2;
				for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
					  w[i] = 0.5 * (1 - cos(2*ALLEGRO_PI*(i+1) / (n+1)));

				idx = half-1;
				for(i=half; i<n; i++) {
					  w[i] = w[idx];
						idx--;
				}
		}
		else
		{
			  half = (n+1)/2;
				for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
					  w[i] = 0.5 * (1 - cos(2*ALLEGRO_PI*(i+1) / (n+1)));

				idx = half-2;
				for(i=half; i<n; i++) {
					  w[i] = w[idx];
						idx--;
				}
		}

		if(itype==1)    //periodic function
		{
			  for(i=N-1; i>=1; i--)
					  w[i] = w[i-1];
				w[0] = 0.0;
		}
		return(w);
}

void Gamestate_Logic(struct Game *game, struct GamestateResources* data) {
	// Called 60 times per second. Here you should do all your game logic.

//data->blink_counter++;

	if (!buffer) return;

	int sample_count = data->samples;
	const int R = sample_count / 320;
	int i, gain = 0;
	if (data->inmenu) {
	for (i = 0; i < sample_count; ++i) {
		buffer[i] = (buffer[i]/2.0) + (sample_center-sample_center/2.0);
	}
	}

	/* Calculate the volume, and display it regardless if we are actively
	 * recording to disk. */
	for (i = 0; i < sample_count; ++i) {
		 if (gain < abs(buffer[i] - sample_center))
			  gain = abs(buffer[i] - sample_center);
	}
	if (data->inmenu) {
		gain /= 8.0;
	}
//if (gain > 5) {
	data->blink_counter = gain / 4.0;
//}

data->rotation += data->blink_counter;

  /* Save raw bytes to disk. Assumes everything is written
		* succesfully. */
   /*if (fp && n < frequency / (float) samples_per_fragment *
			max_seconds_to_record) {
			al_fwrite(fp, input, sample_count * sample_size);
			++n;
	 }*/

   /* Draw a pathetic visualization. It draws exactly one fragment
		* per frame. This means the visualization is dependent on the
		* various parameters. A more thorough implementation would use this
		* event to copy the new data into a circular buffer that holds a
		* few seconds of audio. The graphics routine could then always
		* draw that last second of audio, which would cause the
		* visualization to appear constant across all different settings.
		*/
  for (i = 0; i < 320; ++i) {
		 int j, c = 0;

		 /* Take the average of R samples so it fits on the screen */
		 for (j = i * R; j < i * R + R && j < sample_count; ++j) {
			  c += buffer[j] - sample_center;
		 }
		 c /= R;

		 /* Draws a line from the previous sample point to the next */
		/* al_draw_line(i - 1, 128 + ((prev - min_sample_val) /
				(float) sample_range) * 256 - 128, i, 128 +
				((c - min_sample_val) / (float) sample_range) * 256 - 128,
				al_map_rgb(255,255,255), 1.2);
*/
		 prev = c;
	}
prev = 0;
  /* draw volume bar */
  /*al_draw_filled_rectangle((gain / (float) max_sample_val) * 320, 251,
		 0, 256, al_map_rgba(0, 255, 0, 128));*/
int N = data->samples;
  fftw_complex in[N], out[N];
	fftw_plan p1;

	float *window = hanning(N, 0);

	for (int i = 0; i < N; i++) {
		double multiplier = 0.5 * (1 - cos(2*ALLEGRO_PI*i/2047));

		in[i][0] = buffer[i] * (data->inmenu ? multiplier : window[i]) * (1.0/256.0);
		  in[i][1] = 0;
	}

	free(window);

	p1 = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	fftw_execute(p1);
float max = 0;

for (int i = 1; i <= 320; i++) {
out[i][0] = out[i][0] * out[i][0] + out[i][1] * out[i][1];
}
for (int i = 5; i <= 320; i++) {
/*
if(i*2<320)	  out[i*2][0] -= out[i][0];
if(i*3<320)		out[i*3][0] -= out[i][0];
if(i*4<320)		out[i*4][0] -= out[i][0];
if(i*5<320)		out[i*5][0] -= out[i][0];
if(i*6<320)		out[i*6][0] -= out[i][0];

if (out[i][0] < 0) out[i][0] = 0;*/

/*	al_draw_line(i - 1, 128 + ((prev - min_sample_val) /
		 (float) sample_range) * 256 - 128, i, 128 +
		 ((out[i][0] - min_sample_val) / (float) sample_range) * 256 - 128,
		 al_map_rgb(255,255,255), 1.2);*/

//if ((out[i][0] * out[i][0] + out[i][1] * out[i][1]) > max) {
//	max = (out[i][0] * out[i][0] + out[i][1] * out[i][1]);
//}
  }
max=768;

//al_draw_filled_rectangle(0, 180, 320, 180/2, al_map_rgb(255,255,255));


for (int i = 8; i <= 100; i++) {
/*	al_draw_line(i - 1, 128 + ((prev - min_sample_val) /
	 (float) sample_range) * 256 - 128, i, 128 +
	 ((out[i][0] - min_sample_val) / (float) sample_range) * 256 - 128,
	 al_map_rgb(255,255,255), 1.2);*/
if (out[i][0] != 0) out[i][0] = log(fabs(out[i][0]));
out[i][0] -= 0.5;
out[i][0] *= 2;
//if (out[i][0] > 8) out[i][0] = 8;

/*out[i+320][0] = log(out[i+320][0]);
out[i+320][0] -= 0.5;
out[i+320][0] *= 2;
*/
//out[i][0] += out[i+320][0];
if (out[i][0] > 8) out[i][0] = 8;

}

  //for (int i = 0; i < N; i++)
      //cout << out[i][0] << " + j" << out[i][1] << endl; // <<<



  int oldx = data->x;
	int oldy = data->y;


	data->x += data->vx;
	data->y += data->vy;

	if (data->vx > 0) {
		data->vx -= 0.005;
	} else if (data->vx < 0) {
		data->vx += 0.005;
	}
	data->vy += 0.05;

	if (data->y > 180-HEIGHT-5) {
		data->vy = -data->vy / 3;
		data->y = 178-5;
	}

	if (data->x < 0) {
		data->vx = -data->vx;
		data->x = 0;
	}
	if (data->x > 320) {
		data->vx = -data->vx;
		data->x = 320;
	}
	if (data->y == 180-HEIGHT-5) {
		if (data->vx > 0) {
			data->vx -= 0.01;
		} else if (data->vx < 0) {
			data->vx += 0.01;
		}
	}


	float x = 0; float width = 8;

	for (int i = 8; i <= 50; i++) {
	width -= 0.02;

	int pos = 180 - out[i][0] * 10;
	int prev = 180 - out[i-1][0] * 10;
	int next = 180 - out[i+1][0] * 10;
	//PrintConsole(game, "data->y %f, pos %d", data->y, pos);

	if (data->y - HEIGHT >= pos) {

		//PrintConsole(game, "i %d pos data->y %f pos %d out[i][0] %f", i, data->y, pos, out[i][0]);

		if (x-1 == data->x) {
			// left
			data->vy = (pos - data->y) / 25;
			data->vx += -1;
			PrintConsole(game, "left bump %d", pos);
			data->y = pos;
		}
		else if (x + width == data->x) {
			// right
			data->vy = (pos - data->y) / 25;
			data->vx += 1;
			data->y = pos;
			PrintConsole(game, "right bump %d", pos);
		}
		else if ((x <= data->x) && (x + width >= data->x)) {
			data->vy = (pos - data->y) / 25 - data->vy * 0.5;
			data->vx += ((rand() / (float)INT_MAX) - 0.5) * 2;
			data->y = pos;
			PrintConsole(game, "center bump %d", pos);

			if ((prev < pos) && (next > pos)) {
				data->vx += -1;
			}
			if ((prev > pos) && (next < pos)) {
				data->vy += 1;
			}
		}

	}


//	al_draw_filled_rectangle(x, 180 - data->out[i][0] * 10, x+width, 180, al_map_rgb(255,255,255));
	  x+= width;
	}


	// collision with level

	int oldsx = (int)(oldx / 4);
	int oldsy = (int)(oldy / 4);

	int sx = (int)(data->x /4);
	int sy = (int)(data->y /4);

	int tx = oldsx; int ty = oldsy;

	int colx = sx; int coly = sy;

	while ((tx != sx) && (ty != sy)) {
		if (data->level[tx][ty] == '0') {
			colx = tx;
			coly = ty;
			break;
		}

		if (tx != sx) { if (sx > tx) tx++; else tx--; }
		if (ty != sy) { if (sy > ty) ty++; else ty--; }
	}

	if ((data->level[colx][coly] == 'X') || (data->level[colx][coly] == 'Y')) {
		data->vx = 0;
		data->vy = 0;
		data->x = 320/2;
		data->y = 120;

		data->shakin_dudi=120;

		if (data->level[colx][coly] == 'X') {
			data->score1 ++;
		} else {
			data->score2 ++;
		}

		al_play_sample_instance(data->point);

	}

	if (data->level[colx][coly] == 'O') {
		if (data->x != colx * 4) {
			data->blink_counter=3;
		}
		if (data->y != coly * 4) {
			data->blink_counter=3;
		}
		  data->x = oldsx * 4;
			data->y = oldsy * 4;
/*		if (data->vx > 0) {
			data->x = -4;
		} else {
			data->x = +4;
		}
		if (data->vy > 0) {
			data->y = -4;
		} else {
			data->y = +4;
		}*/
		PrintConsole(game, "collision %d %d", (int)(data->x / 4), (int)(data->y / 4));

		data->vx = -data->vx * 0.5;
		data->vy = -data->vy * 0.5;

		if (fabs(data->vx) < 0.2) {
			data->vx *= 10;
		}
		if (fabs(data->vy) < 0.2) {
			data->vy *= 10;
		}

		if ((data->x == oldx) && (data->y == oldy)) {
			// we're stuck! RANDOMMMMM and hope for the best
			data->vx = ((rand() / (float)INT_MAX) - 0.5) * 5;
			data->vy = ((rand() / (float)INT_MAX) - 0.5) * 5;
		}
	}


	data->vx += sin(data->rotation / 20.0) / 100.0;

	fftw_destroy_plan(p1);

	if (data->shakin_dudi) {
		data->blink_counter = (rand() / (float)INT_MAX) * 15;
		data->rotation+=data->blink_counter;
		data->shakin_dudi--;
	}


	  for (int i=0; i< N; i++) {
			if (data->out[i][0] > out[i][0]) {
				data->out[i][0]-=0.666;
			} else {
				data->out[i][0] = out[i][0];
			}
			data->out[i][1] = out[i][1];
		}

}



void Gamestate_Draw(struct Game *game, struct GamestateResources* data) {
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

if (!data->out) { return; }

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

al_draw_filled_rectangle(x, 180 - data->out[i][0] * 10, x+width, 180, al_map_rgb(255,255,255));
  x+= width;
}

al_draw_filled_rectangle(0, 180-5, 320, 180, al_map_rgb(255,255,255));

if (!data->inmenu){
	al_draw_filled_rectangle(data->x - WIDTH, data->y - HEIGHT, data->x + WIDTH, data->y + HEIGHT,
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



void Gamestate_ProcessEvent(struct Game *game, struct GamestateResources* data, ALLEGRO_EVENT *ev) {
	// Called for each event in Allegro event queue.
	// Here you can handle user input, expiring timers etc.
	if ((ev->type==ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE)) {

		if (!data->inmenu) {
			data->inmenu = true;
			data->inmulti = false;
			loadlevel(game, data, "levels/menu.lvl");
		} else
		UnloadCurrentGamestate(game); // mark this gamestate to be stopped and unloaded
		// When there are no active gamestates, the engine will quit.
	}

	if ((ev->type==ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_SPACE)) {
		data->inmenu = false;
		data->inmulti = true;
		loadlevel(game, data, "levels/multi.lvl");

	}

	if (ev->type == ALLEGRO_EVENT_AUDIO_RECORDER_FRAGMENT) {
		       /* We received an incoming fragment from the microphone. In this
						* example, the recorder is constantly recording even when we aren't
						* saving to disk. The display is updated every time a new fragment
						* comes in, because it makes things more simple. If the fragments
						* are coming in faster than we can update the screen, then it will be
						* a problem.
						*/

		       ALLEGRO_AUDIO_RECORDER_EVENT *re = al_get_audio_recorder_event(ev);
data->inrecording = true;
handleaudio(re->buffer, re->samples, data);
data->inrecording = false;
//					al_flip_display();
	      }

}

void loadlevel(struct Game *game, struct GamestateResources *data, char* name) {
	ALLEGRO_FILE *file = al_fopen(GetDataFilePath(game, name), "r");

	data->score1 = 0;
	data->score2 = 0;

	char buf;

	int x = 0, y = 0;
	while (al_fread(file, &buf, sizeof(char))) {
		data->level[x][y] = buf;
		if (buf!='\n') {
			x++;
			if (x==80) {
				x=0;
				y++;
			}
			if (y==45) {
				break;
			}
		}
	}

	data->x = 320/2;
	data->y = 120;
	data->vx = 0;
	data->vy = 0;

	al_fclose(file);

}

void* Gamestate_Load(struct Game *game, void (*progress)(struct Game*)) {
	// Called once, when the gamestate library is being loaded.
	// Good place for allocating memory, loading bitmaps etc.
	struct GamestateResources *data = malloc(sizeof(struct GamestateResources));
	data->font = al_create_builtin_font();

	data->out = calloc(4096,sizeof(fftw_complex));

	data->point_sample = al_load_sample( GetDataFilePath(game, "point.flac") );
	data->point = al_create_sample_instance(data->point_sample);
	al_set_sample_instance_gain(data->point, 1.5);
	al_attach_sample_instance_to_mixer(data->point, game->audio.fx);
	al_set_sample_instance_playmode(data->point, ALLEGRO_PLAYMODE_ONCE);


	data->game = game;
	progress(game); // report that we progressed with the loading, so the engine can draw a progress bar

	data->pixelator = al_create_bitmap(320, 180);
	data->blurer = al_create_bitmap(320/4, 180/4);


	data->r = al_create_audio_recorder(1000, samples_per_fragment, frequency,
	                                   audio_depth, ALLEGRO_CHANNEL_CONF_1);

	data->shader = al_create_shader(ALLEGRO_SHADER_GLSL);
	PrintConsole(game, "VERTEX: %d", al_attach_shader_source_file(data->shader, ALLEGRO_VERTEX_SHADER, GetDataFilePath(game, "vertex.glsl")));
	PrintConsole(game, "%s", al_get_shader_log(data->shader));
	PrintConsole(game, "PIXEL: %d", al_attach_shader_source_file(data->shader, ALLEGRO_PIXEL_SHADER, GetDataFilePath(game, "pixel.glsl")));
	PrintConsole(game, "%s", al_get_shader_log(data->shader));
	al_build_shader(data->shader);

	loadlevel(game, data, "levels/menu.lvl");


	al_register_event_source(game->_priv.event_queue, al_get_audio_recorder_event_source(data->r));

al_start_audio_recorder(data->r);


data->audio = al_load_audio_stream(GetDataFilePath(game, "waaaa.flac"), 4, 4096);
//al_get_audio_stream_fragment(data->audio);

al_register_event_source(game->_priv.event_queue, al_get_audio_stream_event_source(data->audio));
al_set_audio_stream_playing(data->audio, true);
al_set_audio_stream_playmode(data->audio, ALLEGRO_PLAYMODE_LOOP);
al_attach_audio_stream_to_mixer(data->audio, game->audio.music);


al_set_mixer_postprocess_callback(game->audio.music, *handleaudio, data);

    //(ALLEGRO_MIXER *mixer,
   //void (*pp_callback)(void *buf, unsigned int samples, void *data),
   //void *pp_callback_userdata)

//data->out = NULL;
  return data;
}

void Gamestate_Unload(struct Game *game, struct GamestateResources* data) {
	// Called when the gamestate library is being unloaded.
	// Good place for freeing all allocated memory and resources.
	al_destroy_font(data->font);
	al_destroy_audio_stream(data->audio);
	al_destroy_audio_recorder(data->r);
	free(data);
}

void Gamestate_Start(struct Game *game, struct GamestateResources* data) {
	// Called when this gamestate gets control. Good place for initializing state,
	// playing music etc.
	data->screamtime = 0;
	data->inmulti = false;
	data->inmenu = true;
	data->inrecording = false;
	data->blink_counter = 0;
data->shakin_dudi = 0;
  data->vx = 0;
	data->vy = 0;
	data->x = 320/2;
	data->y = 120;

	data->score1 = 0;
	data->score2 = 0;
}

void Gamestate_Stop(struct Game *game, struct GamestateResources* data) {
	// Called when gamestate gets stopped. Stop timers, music etc. here.
	al_set_audio_stream_playing(data->audio, false);
	al_stop_audio_recorder(data->r);
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
