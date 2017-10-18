/*! \file common.c
 *  \brief Common stuff that can be used by all gamestates.
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

#include "common.h"
#include <libsuperderpy.h>
#include <math.h>

// remember to free the returned buffer
float *CreateHanningWindow(int N, bool periodic) {
	int half, i, idx, n;
	float *w;

	w = (float*) calloc(N, sizeof(float));
	memset(w, 0, N*sizeof(float));

	if (periodic) {
		n = N-1;
	} else {
		n = N;
	}

	if (n%2==0) {
		half = n/2;
		for (i=0; i<half; i++) {
			w[i] = 0.5 * (1 - cos(2*ALLEGRO_PI*(i+1) / (n+1)));
		}

		idx = half-1;
		for (i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	} else {
		half = (n+1)/2;
		for (i=0; i<half; i++) {
			w[i] = 0.5 * (1 - cos(2*ALLEGRO_PI*(i+1) / (n+1)));
		}

		idx = half-2;
		for (i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}

	if (periodic) {
		for (i=N-1; i>=1; i--) {
			w[i] = w[i-1];
		}
		w[0] = 0.0;
	}
	return w;
}

bool GlobalEventHandler(struct Game *game, ALLEGRO_EVENT *ev) {
	if ((ev->type==ALLEGRO_EVENT_KEY_DOWN) && (ev->keyboard.keycode == ALLEGRO_KEY_F)) {
		game->config.fullscreen = !game->config.fullscreen;
		if (game->config.fullscreen) {
			SetConfigOption(game, "SuperDerpy", "fullscreen", "1");
			al_hide_mouse_cursor(game->display);
		} else {
			SetConfigOption(game, "SuperDerpy", "fullscreen", "0");
			al_show_mouse_cursor(game->display);
		}
		al_set_display_flag(game->display, ALLEGRO_FULLSCREEN_WINDOW, game->config.fullscreen);
		SetupViewport(game, game->viewport_config);
		PrintConsole(game, "Fullscreen toggled");
	}

	return false;
}

struct CommonResources* CreateGameData(struct Game *game) {
	struct CommonResources *data = calloc(1, sizeof(struct CommonResources));
	return data;
}

void DestroyGameData(struct Game *game) {
	free(game->data);
}

