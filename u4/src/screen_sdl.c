/*
 * $Id$
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <SDL/SDL.h>

#include "u4.h"
#include "screen.h"
#include "map.h"
#include "context.h"
#include "savegame.h"

SDL_Surface *screen;
SDL_Surface *tiles, *charset;

int screenLoadTiles();
int screenLoadCharSet();
int screenLoadTileSet(SDL_Surface **surface, int width, int height, int n, const char *filename);

void screenInit() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
	exit(1);
    }
    atexit(SDL_Quit);

    screen = SDL_SetVideoMode(320 * SCALE, 200 * SCALE, 16, SDL_SWSURFACE);
    if (!screen) {
	fprintf(stderr, "Unable to set video: %s\n", SDL_GetError());
	exit(1);
    }
    SDL_WM_SetCaption("GNU Ultima IV", NULL);

    if (!screenLoadTiles() ||
        !screenLoadCharSet()) {
        fprintf(stderr, "Unable to load data files\n");
        exit(1);
    }

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL); 
}

int screenLoadTiles() {
    return screenLoadTileSet(&tiles, TILE_WIDTH, TILE_HEIGHT, N_TILES, "./ultima4/shapes.ega");
}

int screenLoadCharSet() {
    return screenLoadTileSet(&charset, CHAR_WIDTH, CHAR_HEIGHT, N_CHARS, "./ultima4/charset.ega");
}

int screenLoadTileSet(SDL_Surface **surface, int width, int height, int n, const char *filename) {
    FILE *in;
    int scale = SCALE;
    int x, y, xs, ys;
    Uint8 *p;

    in = fopen(filename, "r");
    if (!in)
        return 0;

    (*surface) = SDL_CreateRGBSurface(SDL_HWSURFACE, width * scale, height * n * scale, 8, 0, 0, 0, 0);
    if (!(*surface)) {
        fclose(in);
        return 0;
    }

    #define setpalentry(i, red, green, blue) \
        (*surface)->format->palette->colors[i].r = red; \
        (*surface)->format->palette->colors[i].g = green; \
        (*surface)->format->palette->colors[i].b = blue;
    setpalentry(0, 0x00, 0x00, 0x00);
    setpalentry(1, 0x00, 0x00, 0x80);
    setpalentry(2, 0x00, 0x80, 0x00);
    setpalentry(3, 0x00, 0x80, 0x80);
    setpalentry(4, 0x80, 0x00, 0x00);
    setpalentry(5, 0x00, 0x80, 0x80);
    setpalentry(6, 0x80, 0x80, 0x00);
    setpalentry(7, 0xc3, 0xc3, 0xc3);
    setpalentry(8, 0xa0, 0xa0, 0xa0);
    setpalentry(9, 0x00, 0x00, 0xFF);
    setpalentry(10, 0x00, 0xFF, 0x00);
    setpalentry(11, 0xFF, 0x00, 0x00);
    setpalentry(12, 0xFF, 0x00, 0x00);
    setpalentry(13, 0xFF, 0x00, 0xFF);
    setpalentry(14, 0xFF, 0xFF, 0x00);
    setpalentry(15, 0xFF, 0xFF, 0xFF);
    #undef setpalentry

    p = (*surface)->pixels;
    for (y = 0; y < height * n; y++) {
        for (x = 0; x < width; x += 2) {
            int temp = getc(in);
            for (xs = 0; xs < scale; xs++) {
                *p = temp >> 4;
                p++;
            }
            for (xs = 0; xs < scale; xs++) {
                *p = temp & 0x0f;
                p++;
            }
        }
        p += ((*surface)->pitch) - (width * scale);
        for (ys = 1; ys < scale; ys++) {
            for (x = 0; x < width; x++) {
                for (xs = 0; xs < scale; xs++) {
                    *p = *(p - (*surface)->pitch);
                    p++;
                }
            }
            p += ((*surface)->pitch) - (width * scale);
        }
    }

    fclose(in);

    return 1;
}


void screenDrawBorders() {
    SDL_Rect r;

#define do_rect(X, Y, W, H, red, green, blue) \
    r.x = (X) * SCALE; \
    r.y = (Y) * SCALE; \
    r.w = (W) * SCALE; \
    r.h = (H) * SCALE; \
    SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, red, green, blue));

    do_rect(0, 0, CHAR_WIDTH * 40, CHAR_HEIGHT, 0, 0, 255);
    do_rect(CHAR_WIDTH, CHAR_HEIGHT - 1, CHAR_WIDTH * 38, 1, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 11, 0, CHAR_WIDTH * 2, CHAR_HEIGHT, 0, 0, 0);
    do_rect(0, CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT * 23, 0, 0, 255);
    do_rect(CHAR_WIDTH - 1, CHAR_HEIGHT, 1, CHAR_HEIGHT * 22, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH, CHAR_HEIGHT * 23, CHAR_WIDTH * 22, CHAR_HEIGHT, 0, 0, 255);
    do_rect(CHAR_WIDTH, CHAR_HEIGHT * 23, CHAR_WIDTH * 22, 1, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 23, 0, CHAR_WIDTH, CHAR_HEIGHT * 24, 0, 0, 255);
    do_rect(CHAR_WIDTH * 23, CHAR_HEIGHT, 1, CHAR_HEIGHT * 22, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 24 - 1, CHAR_HEIGHT, 1, CHAR_HEIGHT * 23, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 39, CHAR_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT * 11, 0, 0, 255);
    do_rect(CHAR_WIDTH * 39, CHAR_HEIGHT, 1, CHAR_HEIGHT * 11, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 24 - 1, CHAR_HEIGHT * 9, CHAR_WIDTH * 16 + 1, CHAR_HEIGHT, 0, 0, 255);
    do_rect(CHAR_WIDTH * 24, CHAR_HEIGHT * 9, CHAR_WIDTH * 15, 1, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 24, CHAR_HEIGHT * 10 - 1, CHAR_WIDTH * 15, 1, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 24 - 1, CHAR_HEIGHT * 11, CHAR_WIDTH * 16 + 1, CHAR_HEIGHT, 0, 0, 255);
    do_rect(CHAR_WIDTH * 24, CHAR_HEIGHT * 11, CHAR_WIDTH * 15, 1, 0xC3, 0xC3, 0xC3);
    do_rect(CHAR_WIDTH * 24, CHAR_HEIGHT * 12 - 1, CHAR_WIDTH * 16, 1, 0xC3, 0xC3, 0xC3);

    /* corners */
    do_rect(0, 0, CHAR_WIDTH + 1, 1, 0, 0, 0);
    do_rect(0, 1, CHAR_WIDTH - 2, 1, 0, 0, 0);
    do_rect(0, 2, CHAR_WIDTH - 4, 1, 0, 0, 0);
    do_rect(0, 3, CHAR_WIDTH - 5, 1, 0, 0, 0);
    do_rect(0, 4, CHAR_WIDTH - 6, 2, 0, 0, 0);
    do_rect(0, 6, CHAR_WIDTH - 7, 3, 0, 0, 0);
    
    do_rect(0, (CHAR_HEIGHT * 24) - 9, CHAR_WIDTH - 7, 3, 0, 0, 0);
    do_rect(0, (CHAR_HEIGHT * 24) - 6, CHAR_WIDTH - 6, 2, 0, 0, 0);
    do_rect(0, (CHAR_HEIGHT * 24) - 4, CHAR_WIDTH - 5, 1, 0, 0, 0);
    do_rect(0, (CHAR_HEIGHT * 24) - 3, CHAR_WIDTH - 4, 1, 0, 0, 0);
    do_rect(0, (CHAR_HEIGHT * 24) - 2, CHAR_WIDTH - 2, 1, 0, 0, 0);
    do_rect(0, (CHAR_HEIGHT * 24) - 1, CHAR_WIDTH + 1, 1, 0, 0, 0);
    
    do_rect(CHAR_WIDTH * 39, 0, CHAR_WIDTH + 1, 1, 0, 0, 0);
    do_rect(CHAR_WIDTH * 39 + 2, 1, CHAR_WIDTH - 2, 1, 0, 0, 0);
    do_rect(CHAR_WIDTH * 39 + 4, 2, CHAR_WIDTH - 4, 1, 0, 0, 0);
    do_rect(CHAR_WIDTH * 39 + 5, 3, CHAR_WIDTH - 5, 1, 0, 0, 0);
    do_rect(CHAR_WIDTH * 39 + 6, 4, CHAR_WIDTH - 6, 2, 0, 0, 0);
    do_rect(CHAR_WIDTH * 39 + 7, 6, CHAR_WIDTH - 7, 3, 0, 0, 0);
    
    screenShowChar(16, 10, 0);
    screenShowChar(17, 13, 0);

    SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void screenShowChar(int chr, int x, int y) {
    SDL_Rect src, dest;

    src.x = 0;
    src.y = chr * (charset->h / N_CHARS);
    src.w = charset->w;
    src.h = charset->h / N_CHARS;
    dest.x = x * charset->w;
    dest.y = y * (charset->h / N_CHARS);
    dest.w = charset->w;
    dest.h = charset->h / N_CHARS;
    SDL_BlitSurface(charset, &src, screen, &dest);
}

void screenShowTile(int tile, int x, int y) {
    SDL_Rect src, dest;

    src.x = 0;
    src.y = tile * (tiles->h / N_TILES);
    src.w = tiles->w;
    src.h = tiles->h / N_TILES;
    dest.x = x * tiles->w + (BORDER_WIDTH * SCALE);
    dest.y = y * (tiles->h / N_TILES) + (BORDER_HEIGHT * SCALE);
    dest.w = tiles->w;
    dest.h = tiles->h / N_TILES;
    SDL_BlitSurface(tiles, &src, screen, &dest);
}

void screenMessage(char *fmt, ...) {
    char buffer[1024];
    int i;

    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    // scroll the text area, if necessary
    if (c->line == 12) {
        SDL_Rect src, dest;
        
        src.x = TEXT_AREA_X * charset->w;
        src.y = (TEXT_AREA_Y + 1) * (charset->h / N_CHARS);
        src.w = TEXT_AREA_W * charset->w;
        src.h = (TEXT_AREA_H - 1) * charset->h / N_CHARS;

        dest.x = TEXT_AREA_X * charset->w;
        dest.y = TEXT_AREA_Y * (charset->h / N_CHARS);
        dest.w = TEXT_AREA_W * charset->w;
        dest.h = (TEXT_AREA_H - 1) * charset->h / N_CHARS;

        SDL_BlitSurface(screen, &src, screen, &dest);

        dest.x = TEXT_AREA_X * charset->w;
        dest.y = (TEXT_AREA_Y + TEXT_AREA_H - 1) * (charset->h / N_CHARS);
        dest.w = TEXT_AREA_W * charset->w;
        dest.h = 1 * charset->h / N_CHARS;

        SDL_FillRect(screen, &dest, SDL_MapRGB(screen->format, 0, 0, 0));
        SDL_UpdateRect(screen, 0, 0, 0, 0);

        c->line--;
    }

    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '\n' || c->col == 16) {
            if (buffer[i] == '\n')
                i++;
            c->line++;
            c->col = 0;
            screenMessage(buffer + i);
            return;
        }
	screenShowChar(buffer[i], TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
        c->col++;
    }
}

void screenForceRedraw() {
    SDL_UpdateRect(screen, 0, 0, 0, 0);
}
