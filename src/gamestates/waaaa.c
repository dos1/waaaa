/*! \file empty.c
 *  \brief Empty gamestate.
 */
/*
 * Copyright (c) Sebastian Krzyszkowiak <dos@dosowisko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define ALLEGRO_UNSTABLE

#include "../common.h"
#include <allegro5/allegro_color.h>
#include <fftw3.h>
#include <libsuperderpy.h>
#include <math.h>

#define SAMPLE_RATE 44100 // FIXME: should match the main mixer

#define FFT_SAMPLES 8192

#define BARS_NUM (8192 / 2)
#define BARS_WIDTH 4
#define BARS_OFFSET 8

#define MAX_MAX_LIMIT 0.042

static const int BALL_WIDTH = 3;
static const int BALL_HEIGHT = 3;

// TODO: play with moving window
// TODO: mic volume / output fft normalization?
// TODO: bar offset with mic

struct GamestateResources {
	// This struct is for every resource allocated and used by your gamestate.
	// It gets created on load and then gets passed around to all other function calls.
	ALLEGRO_FONT* font;
	ALLEGRO_AUDIO_STREAM* audio;
	ALLEGRO_AUDIO_RECORDER* recorder;
	ALLEGRO_MIXER* mixer;
	ALLEGRO_BITMAP *crt, *crtbg;
	ALLEGRO_BITMAP* screen;
	ALLEGRO_BITMAP* stage;
	float bars[BARS_NUM];
	float fft[SAMPLE_RATE / 2 + 1];
	float ringbuffer[SAMPLE_RATE];
	int ringpos;
	float fftbuffer[FFT_SAMPLES];
	float max_max;

	ALLEGRO_BITMAP *pixelator, *blurer, *background;
	ALLEGRO_SHADER* shader;

	ALLEGRO_SAMPLE_INSTANCE* point;
	ALLEGRO_SAMPLE* point_sample;

	ALLEGRO_MUTEX* mutex;

	int distortion;
	float rotation;
	int screamtime;
	bool inmenu;
	bool inmulti;
	float vx, vy, x, y;

	int shakin_dudi;
	int score1, score2;
	char level[80][45];

	int yoffset;

	struct Game* game;
	bool music_mode;

	bool demo_mode;
	int blink_counter;

	bool use_shaders;
	char* current_level;
};

int Gamestate_ProgressCount = 1; // number of loading steps as reported by Gamestate_Load

void FFT(void* buffer, unsigned int samples, void* userdata);
void LoadLevel(struct Game* game, struct GamestateResources* data, char* name);

void Gamestate_Logic(struct Game* game, struct GamestateResources* data, double delta) {
	al_lock_mutex(data->mutex); // we don't want to get the ringbuffer rewritten as we access it
	int end = data->ringpos - FFT_SAMPLES;
	if (end < 0) {
		end += SAMPLE_RATE;
	}
	int i = 0;
	for (int pos = end; pos != data->ringpos; pos++) {
		data->fftbuffer[i] = data->ringbuffer[pos];
		if (fabsf(data->fftbuffer[i]) > 1) {
			printf("fftbuffer[%d] = ringbuffer[%d] = %f\n", i, pos, data->fftbuffer[i]);
		}
		i++;
		if (pos == SAMPLE_RATE - 1) {
			pos = -1;
		}
	}

	FFT(data->fftbuffer, FFT_SAMPLES, data);
	al_unlock_mutex(data->mutex);
}

void Gamestate_Tick(struct Game* game, struct GamestateResources* data) {
	// Called 60 times per second.

	float gain = 0;
	for (int i = 0; i < BARS_NUM; i++) {
		data->bars[i] = 0;
		int width = 1;
		for (int j = i * width; j < i * width + width; j++) {
			data->bars[i] += data->fft[j + 8 * width];
			if (gain < data->fft[j + 8 * width]) {
				gain = data->fft[j + 8 * width];
			}
		}
		data->bars[i] /= width;
	}
	data->distortion = gain * 2;
	data->rotation += data->distortion * 3;

	//PrintConsole(game, "gain %f", gain);

	// COLLISION HANDLING (sucks)

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

	if (data->y > 180 - BALL_HEIGHT - 5) {
		data->vy = -data->vy / 3;
		data->y = 178 - 5;
	}

	if (data->x < 0) {
		data->vx = -data->vx;
		data->x = 0;
	}
	if (data->x > 319) {
		data->vx = -data->vx;
		data->x = 319;
	}
	if (data->y == 180 - BALL_HEIGHT - 5) {
		if (data->vx > 0) {
			data->vx -= 0.01;
		} else if (data->vx < 0) {
			data->vx += 0.01;
		}
	}

	float x = 0;
	float width = BARS_WIDTH;

	for (int i = BARS_OFFSET; i <= 320 / BARS_WIDTH + BARS_OFFSET; i++) {
		//width -= 0.02;
		if (data->bars[i] != data->bars[i]) { // NaN
			break;
		}
		int pos = 176 - data->bars[i] * 64;
		int prev = 176 - data->bars[i - 1] * 64;
		int next = 176 - data->bars[i + 1] * 64;
		//PrintConsole(game, "data->y %f, pos %d", data->y, pos);

		if (data->y - BALL_HEIGHT >= pos) {
			//PrintConsole(game, "i %d pos data->y %f pos %d out[i][0] %f", i, data->y, pos, out[i][0]);

			if (x - 1 == data->x) {
				// left
				data->vy = (pos - data->y) / 15;
				data->vx += -2;
				PrintConsole(game, "left bump %d", pos);
				data->y = pos;
			} else if (x + width == data->x) {
				// right
				data->vy = (pos - data->y) / 15;
				data->vx += 2;
				data->y = pos;
				PrintConsole(game, "right bump %d", pos);
			} else if ((x <= data->x) && (x + width >= data->x)) {
				data->vy = (pos - data->y) / 15 - data->vy * 0.5;
				data->vx += ((rand() / (float)RAND_MAX) - 0.5) * 2;
				data->y = pos;
				//PrintConsole(game, "center bump %d", pos);

				if ((prev < pos) && (next > pos)) {
					data->vx += -2;
				}
				if ((prev > pos) && (next < pos)) {
					data->vy += 2;
				}
			}
		}

		//	al_draw_filled_rectangle(x, 180 - data->out[i][0] * 10, x+width, 180, al_map_rgb(255,255,255));
		x += width;
	}

	// collision with level

	int oldsx = oldx / 4;
	int oldsy = oldy / 4;

	int sx = (int)(data->x / 4);
	int sy = (int)(data->y / 4);

	int tx = oldsx;
	int ty = oldsy;

	int colx = sx;
	int coly = sy;

	while ((tx != sx) && (ty != sy)) {
		if (data->level[tx][ty] == '0') {
			colx = tx;
			coly = ty;
			break;
		}

		if (tx != sx) {
			if (sx > tx) {
				tx++;
			} else {
				tx--;
			}
		}
		if (ty != sy) {
			if (sy > ty) {
				ty++;
			} else {
				ty--;
			}
		}
	}

	if ((data->level[colx][coly] == 'X') || (data->level[colx][coly] == 'Y')) {
		data->vx = 0;
		data->vy = 0;
		data->x = 320 / 2;
		data->y = 120;

		data->shakin_dudi = 120;

		if (data->level[colx][coly] == 'X') {
			data->score1++;
		} else {
			data->score2++;
		}

		al_play_sample_instance(data->point);
	}

	if (data->level[colx][coly] == 'O') {
		if (data->x != colx * 4) {
			data->distortion = 3;
		}
		if (data->y != coly * 4) {
			data->distortion = 3;
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
			data->vx = ((rand() / (float)RAND_MAX) - 0.5) * 5;
			data->vy = ((rand() / (float)RAND_MAX) - 0.5) * 5;
		}
	}

	if (data->level[colx][coly] == '!') {
		data->x = 320 / 2;
		data->y = 120;
	}

	data->vx += sin(data->rotation / 20.0) / 75.0;

	if (data->shakin_dudi) {
		data->distortion = (rand() / (float)RAND_MAX) * 15;
		data->rotation += data->distortion;
		data->shakin_dudi--;
	}

	if (!data->music_mode) {
		if (data->max_max <= MAX_MAX_LIMIT + 0.001) {
			if (!data->demo_mode) {
				// start demo mode
				PrintConsole(game, "starting demo");
				data->demo_mode = true;
				LoadLevel(game, data, "levels/menu.lvl");
			}
		} else {
			if (data->demo_mode) {
				// stop demo mode
				PrintConsole(game, "out of demo at %f", data->max_max);
				data->demo_mode = false;
				LoadLevel(game, data, "levels/multi.lvl");
				data->score1 = 0;
				data->score2 = 0;
			}
		}
	}

	data->blink_counter++;
	if (data->blink_counter >= 60) {
		data->blink_counter = 0;
	}
}

void LoadLevel(struct Game* game, struct GamestateResources* data, char* name) {
	ALLEGRO_FILE* file = al_fopen(GetDataFilePath(game, name), "r");

	//	data->score1 = 0;
	//	data->score2 = 0;

	char buf;

	int x = 0, y = 0;
	while (al_fread(file, &buf, sizeof(char))) {
		data->level[x][y] = buf;
		if (buf != '\n') {
			x++;
			if (x == 80) {
				x = 0;
				y++;
			}
			if (y == 45) {
				break;
			}
		}
	}

	/*	data->x = 320/2;
	data->y = 120;
	data->vx = 0;
	data->vy = 0;*/

	al_fclose(file);

	data->current_level = name;

	// drawing
	al_set_target_bitmap(data->stage);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));
	al_hold_bitmap_drawing(true);
	for (int x = 0; x < 80; x++) {
		for (int y = 0; y < 45; y++) {
			int color = 32;
			if (!data->use_shaders) {
				color = 50;
			}
			int color2 = 64;
			if (!data->use_shaders) {
				color2 = 95;
			}
			if (data->level[x][y] == 'O') {
				al_draw_filled_rectangle(x * 4, y * 4, x * 4 + 4, y * 4 + 4, al_map_rgb(255, 255, 255));
			}
			if (data->level[x][y] == 'X') {
				al_draw_filled_rectangle(x * 4, y * 4, x * 4 + 4, y * 4 + 4, al_map_rgba(0, 0, color, color));
			}
			if (data->level[x][y] == 'Y') {
				al_draw_filled_rectangle(x * 4, y * 4, x * 4 + 4, y * 4 + 4, al_map_rgba(color, 0, 0, color));
			}
			if (data->level[x][y] == 'a') {
				al_draw_filled_rectangle(x * 4, y * 4, x * 4 + 4, y * 4 + 4, al_map_rgba(color2, color2, color2, color2));
			}
		}
	}
	al_hold_bitmap_drawing(false);
	SetFramebufferAsTarget(game);
}

void Gamestate_Draw(struct Game* game, struct GamestateResources* data) {
	// Called as soon as possible, but no sooner than next Gamestate_Logic call.
	// Draw everything to the screen here.

	if (data->use_shaders) {
		al_set_target_bitmap(data->pixelator);
		al_clear_to_color(al_color_hsv(fabs(sin(data->rotation / 360.0)) * 360, 0.75, 0.5 + sin(data->rotation / 20.0) / 20.0));

		SetFramebufferAsTarget(game);
		al_use_shader(data->shader);
		al_set_shader_int("scaleFactor", 2);

		al_draw_bitmap(data->pixelator, 0, 0, 0);

		al_use_shader(NULL);
	} else {
		ALLEGRO_TRANSFORM trans;
		al_identity_transform(&trans);
		al_set_clipping_rectangle(0, 0, al_get_display_width(game->display), al_get_display_height(game->display));
		al_use_transform(&trans);

		al_clear_to_color(al_color_hsv(fabs(sin(data->rotation / 360.0)) * 360, 0.75, 0.5 + sin(data->rotation / 20.0) / 20.0));

		al_set_separate_blender(ALLEGRO_DEST_MINUS_SRC, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA, ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);
		al_hold_bitmap_drawing(true);
		for (int i = 0; i < al_get_display_width(game->display); i += al_get_bitmap_width(data->crtbg)) {
			for (int j = 0; j < al_get_display_height(game->display); j += al_get_bitmap_height(data->crtbg)) {
				al_draw_bitmap(data->crtbg, i, j, 0);
			}
		}
		al_hold_bitmap_drawing(false);
		al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);

		ResetClippingRectangle();
	}

	al_set_target_bitmap(data->pixelator);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));

	// LEVEL DRAWING
	al_draw_bitmap(data->stage, 0, 0, 0);

	// BAR DRAWING
	al_hold_bitmap_drawing(true);
	int width = 320 / BARS_NUM;
	if (width == 0) {
		width = 1;
	}
	width *= BARS_WIDTH;
	for (int i = BARS_OFFSET; i < BARS_NUM; i++) {
		int a = i - BARS_OFFSET;
		al_draw_filled_rectangle(a * width, (int)(176 - data->bars[i] * 64), a * width + width, 180, al_map_rgb(255, 255, 255));
		if (a * width > game->viewport.width) {
			break;
		}
	}
	al_hold_bitmap_drawing(false);

	/*
	// WAVEFORM DRAWING
	width=1;
	for (int i=0; i<4096; i++) {
		al_draw_filled_rectangle(i*width, 180/2 - data->fftbuffer[i]*180/2, i*width+width, 180/2, al_map_rgb(255,255,0));
	}
	al_draw_textf(data->font, al_map_rgb(255,255,255), 10, 10, ALLEGRO_ALIGN_LEFT, "%d", data->ringpos);
	*/

	// BALL DRAWING
	//	if (!data->inmenu){
	if (!data->demo_mode) {
		al_draw_textf(data->font, al_map_rgb(255, 255, 255), 320 / 2, 72, ALLEGRO_ALIGN_CENTER, data->shakin_dudi ? (((data->shakin_dudi / 10) % 2) ? "" : "SCORE!") : "WAAAA");
	}

	al_draw_filled_rectangle(data->x - BALL_WIDTH, data->y - BALL_HEIGHT, data->x + BALL_WIDTH, data->y + BALL_HEIGHT,
		//al_color_hsv(fabs(sin((data->rotation/360.0)+ALLEGRO_PI/4.0)) * 360, 1, 1));

		al_map_rgb(255, 255, 0));
	//	}
	// UI DRAWING
	//	if (data->inmulti) {
	if (!data->demo_mode) {
		al_draw_textf(data->font, al_map_rgb(255, 255, 255), 320 / 2, 82, ALLEGRO_ALIGN_CENTER, "%d:%d", data->score1, data->score2);
	}
	if (data->demo_mode && !data->music_mode) {
		if (data->blink_counter < 50) {
			al_draw_text(data->font, al_map_rgb(255, 255, 255), 320 / 2, 126, ALLEGRO_ALIGN_CENTER, "GRAB A MICROPHONE AND PLAY!");
		}
	}
	//	}

	// FINAL DRAWING
	al_set_target_bitmap(data->blurer);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));

	al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 0, 0, 320 / 4, 180 / 4, 0);

	float s = data->distortion / 5.0;
	ALLEGRO_COLOR tint = al_map_rgba(32 * s, 32 * s, 32 * s, 32 * s);

	float rot = sin(data->rotation / 20.0) / 20.0;

	float scale = 1 - pow((fabs((320 / 2) - data->x) / (320 / 2.0)), 2) * 0.1;

	al_set_target_bitmap(data->background);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));

	al_hold_bitmap_drawing(true);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2, 180 / 2 - 40, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 + 2, 180 / 2 - 40, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 - 2, 180 / 2 - 40, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 - 2, 180 / 2 - 40 - 3, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 + 2, 180 / 2 - 40 - 3, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);

	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 + 2, 180 / 2 - 200, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);

	int yoffset = data->yoffset;

	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2, 180 / 2 - 120 + yoffset, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 + 2, 180 / 2 - 120 + yoffset, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 - 2, 180 / 2 - 120 + yoffset, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 - 2, 180 / 2 - 120 + yoffset - 3, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->blurer, tint, 320 / 4 / 2, (180 / 4) * (3 / 4), 320 / 2 + 2, 180 / 2 - 120 + yoffset - 3, 1.1 * 4 * scale, 1.1 * 4 * scale, rot, 0);
	al_hold_bitmap_drawing(false);

	SetFramebufferAsTarget(game);
	al_draw_bitmap(data->background, 0, 0, 0);

	float offset = data->distortion / 2.0 * (rand() / (float)RAND_MAX);

	al_hold_bitmap_drawing(true);
	al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(0, 192, 192, 192), 320 / 2, 180 * (3 / 4), 320 / 2 - 2 * offset, 180 / 2 - 120 + yoffset, 1.1 * scale, 1.1 * scale, rot, 0);
	al_draw_tinted_scaled_rotated_bitmap(data->pixelator, al_map_rgba(192, 0, 0, 192), 320 / 2, 180 * (3 / 4), 320 / 2 + 2 * offset, 180 / 2 - 120 + yoffset, 1.1 * scale, 1.1 * scale, rot, 0);
	al_hold_bitmap_drawing(false);

	if (data->use_shaders) {
		al_use_shader(data->shader);
		al_set_shader_int("scaleFactor", 1);
		al_draw_scaled_rotated_bitmap(data->pixelator, 320 / 2, 180 * (3 / 4), 320 / 2, 180 / 2 - 120, 1.1 * scale, 1.1 * scale, rot, 0);
		al_use_shader(NULL);
	} else {
		al_set_target_bitmap(data->screen);
		al_clear_to_color(al_map_rgba(0, 0, 0, 0));

		al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 0, 0, al_get_bitmap_width(data->screen), al_get_bitmap_height(data->screen), 0);
		al_hold_bitmap_drawing(true);
		for (int i = 0; i < al_get_display_width(game->display); i += al_get_bitmap_width(data->crt) * 2) {
			for (int j = 0; j < al_get_display_height(game->display); j += al_get_bitmap_height(data->crt)) {
				al_draw_scaled_bitmap(data->crt, 0, 0, 500, 500, i, j, 1000, 500, 0);
				//al_draw_bitmap(data->crt, i, j, 0);
			}
		}
		al_hold_bitmap_drawing(false);

		al_set_blender(ALLEGRO_ADD, ALLEGRO_ZERO, ALLEGRO_ALPHA); // now as a mask
		al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 0, 0, al_get_bitmap_width(data->screen), al_get_bitmap_height(data->screen), 0);

		al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);

		//	al_use_shader(NULL);

		SetFramebufferAsTarget(game);
		//al_clear_to_color(al_map_rgb(0,255,0));
		//al_draw_bitmap(data->screen, 0, 0,0);
		//al_draw_scaled_bitmap(data->screen, 0 ,0, al_get_bitmap_width(data->screen), al_get_bitmap_height(data->screen), 0, 0, 320, 180, 0);
		al_draw_scaled_rotated_bitmap(data->screen, al_get_bitmap_width(data->screen) / 2, al_get_bitmap_height(data->screen) * (3 / 4), 320 / 2, 180 / 2 - 120 + yoffset, 320 / (float)al_get_bitmap_width(data->screen) * 1.1 * scale, 180 / (float)al_get_bitmap_height(data->screen) * 1.1 * scale, rot, 0);
	}

	al_set_target_bitmap(data->pixelator);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));
	al_draw_text(data->font, al_map_rgb(255, 255, 255), 319, 180 - 9, ALLEGRO_ALIGN_RIGHT, "ALPHAAAA BUILD");
	SetFramebufferAsTarget(game);
	al_draw_scaled_bitmap(data->pixelator, 0, 0, 320, 180, 320 / 2, 180 / 2, 320 / 2, 180 / 2, 0);
}

static void MixerPostprocess(void* buffer, unsigned int samples, void* userdata) {
	// REMEMBER: don't use any drawing code inside this function
	// PrintConsole etc. NOT ALLOWED!
	float* buf = buffer;
	struct GamestateResources* data = userdata;

	// critical section: don't use ringbuffer behind our back, as it's getting rewritten
	al_lock_mutex(data->mutex);

	int end = data->ringpos + samples;
	if (end >= SAMPLE_RATE) {
		end -= SAMPLE_RATE;
	}

	// TODO: in case of mono signal, detect silent channel and ignore

	int i = 0;
	for (int pos = data->ringpos; pos != end; pos++) {
		data->ringbuffer[pos] = (buf[i] + buf[i + 1]) / 2;
		i += 2;
		if (pos == SAMPLE_RATE - 1) {
			pos = -1;
		}
	}
	data->ringpos = end;

	al_unlock_mutex(data->mutex);
}

void FFT(void* buffer, unsigned int samples, void* userdata) {
	float* buf = buffer;
	struct GamestateResources* data = userdata;
	double* in;
	fftw_complex* out;
	in = (double*)fftw_malloc(sizeof(double) * samples);
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (samples / 2 + 1));

	float* window = CreateHanningWindow(samples, false);
	float min = 0, max = 0;
	for (unsigned int i = 0; i < samples; i++) {
		if (buf[i] > max) {
			max = buf[i];
		}
		if (buf[i] < min) {
			min = buf[i];
		}
	}
	min *= -1;
	if (min > max) {
		max = min;
	}
	if (max > data->max_max) {
		data->max_max = max;
	}

	for (unsigned int i = 0; i < samples; i++) {
		in[i] = (buf[i] * window[i]) / data->max_max;
		//buf[i] = 0;
		//PrintConsole(data->game, "%d: %f", i, buf[i]);
	}
	//PrintConsole(data->game, "samples: %d, min: %f, max: %f, max_max: %f", samples, min, max, data->max_max);
	//fflush(stdout);
	free(window);

	if (max < data->max_max) {
		data->max_max -= (data->max_max - max) / 1024.0;
	}
	if (data->max_max < MAX_MAX_LIMIT) {
		data->max_max = MAX_MAX_LIMIT; // reboot develop setting
	}

	fftw_plan p = fftw_plan_dft_r2c_1d(samples, in, out, FFTW_ESTIMATE);
	fftw_execute(p);

	for (unsigned int i = 0; i < (samples / 2 + 1); i++) {
		out[i][0] *= 1. / samples;
		out[i][1] *= 1. / samples;

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

void Gamestate_ProcessEvent(struct Game* game, struct GamestateResources* data, ALLEGRO_EVENT* ev) {
	// Called for each event in Allegro event queue.
	// Here you can handle user input, expiring timers etc.
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_ESCAPE)) {
		UnloadCurrentGamestate(game); // mark this gamestate to be stopped and unloaded
		// When there are no active gamestates, the engine will quit.
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_SPACE)) {
		data->x = 320 / 2;
		data->y = 120;
		data->vx = 0;
		data->vy = 0;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_R)) {
		data->score1 = 0;
		data->score2 = 0;
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_N)) {
		if (data->yoffset == 0) {
			LoadLevel(game, data, "levels/border.lvl");
			data->yoffset = 10;
		} else {
			LoadLevel(game, data, "levels/multi.lvl");
			data->yoffset = 0;
		}
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_TAB)) {
		SwitchCurrentGamestate(game, "cinema");
	}
	if ((ev->type == ALLEGRO_EVENT_KEY_DOWN) && ((ev->keyboard.keycode == ALLEGRO_KEY_S) || (ev->keyboard.keycode == ALLEGRO_KEY_BACK))) {
		data->use_shaders = !data->use_shaders;
		PrintConsole(game, "use_shaders: %d", data->use_shaders);
		LoadLevel(game, data, data->current_level);
	}

	if (ev->type == ALLEGRO_EVENT_AUDIO_RECORDER_FRAGMENT) {
		ALLEGRO_AUDIO_RECORDER_EVENT* re = al_get_audio_recorder_event(ev);
		MixerPostprocess(re->buffer, re->samples, data);
	}

	if (ev->type == ALLEGRO_EVENT_DISPLAY_RESIZE) {
		al_destroy_bitmap(data->screen);

		int flags = al_get_new_bitmap_flags();
		al_set_new_bitmap_flags(flags | ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);
		data->screen = CreateNotPreservedBitmap(al_get_display_width(game->display), al_get_display_height(game->display));
		al_set_new_bitmap_flags(flags);
	}
}

void* Gamestate_Load(struct Game* game, void (*progress)(struct Game*)) {
	// Called once, when the gamestate library is being loaded.
	// Good place for allocating memory, loading bitmaps etc.
	struct GamestateResources* data = calloc(1, sizeof(struct GamestateResources));
	data->font = al_create_builtin_font();

	int flags = al_get_new_bitmap_flags();
	al_set_new_bitmap_flags(flags ^ ALLEGRO_MAG_LINEAR);

	progress(game); // report that we progressed with the loading, so the engine can draw a progress bar

	for (int i = 0; i < SAMPLE_RATE; i++) {
		data->ringbuffer[i] = 0;
	}

	data->music_mode = false;
	data->mutex = al_create_mutex();

	data->mixer = al_create_mixer(SAMPLE_RATE, ALLEGRO_AUDIO_DEPTH_FLOAT32, ALLEGRO_CHANNEL_CONF_2);
	al_attach_mixer_to_mixer(data->mixer, game->audio.music);

	data->audio = al_load_audio_stream(GetDataFilePath(game, "waaaa.flac"), 4, 4096);
	//al_register_event_source(game->_priv.event_queue, al_get_audio_stream_event_source(data->audio));
	al_set_audio_stream_playing(data->audio, true);
	al_set_audio_stream_playmode(data->audio, ALLEGRO_PLAYMODE_LOOP);
	al_attach_audio_stream_to_mixer(data->audio, data->mixer);
	al_set_audio_stream_gain(data->audio, 1);

	data->recorder = al_create_audio_recorder(128, 1024, SAMPLE_RATE,
		ALLEGRO_AUDIO_DEPTH_FLOAT32, ALLEGRO_CHANNEL_CONF_2);

	if (!data->recorder) {
		data->music_mode = true;
		PrintConsole(game, "ERROR: audio recorder failed!");
	}

	if (data->music_mode) {
		al_set_mixer_postprocess_callback(data->mixer, MixerPostprocess, data);
	} else {
		al_register_event_source(game->event_queue, al_get_audio_recorder_event_source(data->recorder));
	}

	data->pixelator = CreateNotPreservedBitmap(320, 180);
	data->stage = CreateNotPreservedBitmap(320, 180);
	data->background = CreateNotPreservedBitmap(320, 180);
	data->blurer = CreateNotPreservedBitmap(320 / 4, 180 / 4);

	data->point_sample = al_load_sample(GetDataFilePath(game, "point.flac"));
	data->point = al_create_sample_instance(data->point_sample);
	al_set_sample_instance_gain(data->point, 1.5);
	al_attach_sample_instance_to_mixer(data->point, game->audio.fx);
	al_set_sample_instance_playmode(data->point, ALLEGRO_PLAYMODE_ONCE);

	al_set_new_bitmap_flags(flags | ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);
	data->screen = CreateNotPreservedBitmap(al_get_display_width(game->display), al_get_display_height(game->display));

	data->crt = al_create_bitmap(500, 500);
	ALLEGRO_BITMAP* crt = al_load_bitmap(GetDataFilePath(game, "crt.png"));
	al_set_target_bitmap(data->crt);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));
	al_hold_bitmap_drawing(true);
	for (int i = 0; i < al_get_display_width(game->display); i += al_get_bitmap_width(crt)) {
		for (int j = 0; j < al_get_display_height(game->display); j += al_get_bitmap_height(crt)) {
			al_draw_bitmap(crt, i, j, 0);
		}
	}
	al_hold_bitmap_drawing(false);
	al_destroy_bitmap(crt);

	data->crtbg = al_create_bitmap(500, 500);
	crt = al_load_bitmap(GetDataFilePath(game, "crtbg.png"));
	al_set_target_bitmap(data->crtbg);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));
	al_hold_bitmap_drawing(true);
	for (int i = 0; i < al_get_display_width(game->display); i += al_get_bitmap_width(crt)) {
		for (int j = 0; j < al_get_display_height(game->display); j += al_get_bitmap_height(crt)) {
			al_draw_bitmap(crt, i, j, 0);
		}
	}
	al_hold_bitmap_drawing(false);
	al_destroy_bitmap(crt);

	al_set_new_bitmap_flags(flags);

	data->game = game;

	data->shader = CreateShader(game, GetDataFilePath(game, "vertex.glsl"), GetDataFilePath(game, "pixel.glsl"));

	return data;
}

void Gamestate_Unload(struct Game* game, struct GamestateResources* data) {
	// Called when the gamestate library is being unloaded.
	// Good place for freeing all allocated memory and resources.
	al_destroy_font(data->font);
	al_destroy_audio_stream(data->audio);
	al_destroy_mixer(data->mixer);
	if (data->recorder) {
		al_destroy_audio_recorder(data->recorder);
	}
	al_destroy_bitmap(data->crt);
	al_destroy_bitmap(data->screen);
	al_destroy_bitmap(data->stage);

	al_destroy_bitmap(data->pixelator);
	al_destroy_bitmap(data->blurer);
	al_destroy_bitmap(data->background);
	DestroyShader(game, data->shader);
	al_destroy_sample_instance(data->point);
	al_destroy_sample(data->point_sample);

	al_destroy_mutex(data->mutex);
	free(data);
}

void Gamestate_Start(struct Game* game, struct GamestateResources* data) {
	// Called when this gamestate gets control. Good place for initializing state,
	// playing music etc.
	data->use_shaders = true;
	data->ringpos = 0;
	data->max_max = MAX_MAX_LIMIT;
	data->demo_mode = true;
	if (data->recorder) {
		al_start_audio_recorder(data->recorder);
	}
	LoadLevel(game, data, "levels/menu.lvl");
	data->x = 320 / 2;
	data->y = 120;
	data->vx = 0;
	data->vy = 0;
	data->blink_counter = 0;

	data->distortion = 0;
	data->rotation = 0;
	data->screamtime = 0;
	data->inmenu = true;
	data->inmulti = false;
	data->shakin_dudi = 0;
	data->score1 = 0;
	data->score2 = 0;

	data->yoffset = 0;
}

void Gamestate_Stop(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets stopped. Stop timers, music etc. here.
	if (data->recorder) {
		al_stop_audio_recorder(data->recorder);
	}
}

void Gamestate_Pause(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets paused (so only Draw is being called, no Logic not ProcessEvent)
	// Pause your timers here.
}

void Gamestate_Resume(struct Game* game, struct GamestateResources* data) {
	// Called when gamestate gets resumed. Resume your timers here.
}

// Ignore this for now.
// TODO: Check, comment, refine and/or remove:
void Gamestate_Reload(struct Game* game, struct GamestateResources* data) {
	data->pixelator = CreateNotPreservedBitmap(320, 180);
	data->stage = CreateNotPreservedBitmap(320, 180);
	data->background = CreateNotPreservedBitmap(320, 180);
	data->blurer = CreateNotPreservedBitmap(320 / 4, 180 / 4);

	int flags = al_get_new_bitmap_flags();
	al_set_new_bitmap_flags(flags | ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);
	data->screen = CreateNotPreservedBitmap(al_get_display_width(game->display), al_get_display_height(game->display));
	al_set_new_bitmap_flags(flags);

	LoadLevel(game, data, data->current_level);
}
