#include "rom.h"
#include "assert.h"
#include "ppu.h"
#include "stdio.h"
#include "SDL/SDL.h"
#include "SDL/SDL_framerate.h"
#include "GL/glew.h"


typedef struct {
    SDL_Surface* screen;
    GLuint tex;
    FPSmanager fpsmanager;
    bool pendingResize;
    int pendingResizeWidth;
    int pendingResizeHeight;
} Video;

static Video v;
static Ppu* p;

void flush_events() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_VIDEORESIZE:
                v.pendingResize = true;
                v.pendingResizeWidth = event.resize.w;
                v.pendingResizeHeight = event.resize.h;
                break;
            case SDL_QUIT:
                exit(0);
        }
    }
}

void rom_cycle(uint8_t cycles) {
    flush_events();
    for (int i = 0; i < 3 * cycles; ++i) {
        Ppu_step(p);
    }
}

uint8_t rom_ppustatus() {
    return Ppu_readStatus(p);
}

void rom_ppuctrl(uint8_t b) {
    Ppu_writeControl(p, b);
}

void rom_ppumask(uint8_t b) {
    Ppu_writeMask(p, b);
}

void rom_ppuaddr(uint8_t b) {
    Ppu_writeAddress(p, b);
}

void rom_setppudata(uint8_t b) {
    Ppu_writeData(p, b);
}

void rom_oamaddr(uint8_t b) {
    Ppu_writeOamAddress(p, b);
}

void rom_setoamdata(uint8_t b) {
    Ppu_writeOamData(p, b);
}

void rom_setppuscroll(uint8_t b) {
    Ppu_writeScroll(p, b);
}

void reshape_video(int width, int height) {
    int x_offset = 0;
    int y_offset = 0;

    double dWidth = width;
    double dHeight = height;
    double r = dHeight / dWidth;

    if (r > 0.9375) { // Height taller than ratio
        int h = 0.9375 * dWidth;
        y_offset = (height - h) / 2;
        height = h;
    } else if (r < 0.9375) { // Width wider
        double scrW, scrH;
        if (p->overscanEnabled) {
            scrW = 240.0;
            scrH = 224.0;
        } else {
            scrW = 256.0;
            scrH = 240.0;
        }

        int w = (scrH / scrW) * dHeight;
        x_offset = (width - w) / 2;
        width = w;
    }

    glViewport(x_offset, y_offset, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

void init_video() {
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
        exit(1);
    }

    v.screen = SDL_SetVideoMode(512, 480, 32, SDL_OPENGL|SDL_RESIZABLE);

    if (v.screen == NULL) {
        fprintf(stderr, "Unable to set SDL video mode: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_WM_SetCaption("jamulator", NULL);

    if (glewInit() != 0) {
        fprintf(stderr, "Unable to init glew\n");
        exit(1);
    }

    glEnable(GL_TEXTURE_2D);
    reshape_video(v.screen->w, v.screen->h);
    v.pendingResize = false;

    glGenTextures(1, &v.tex);

    SDL_initFramerate(&v.fpsmanager);
    SDL_setFramerate(&v.fpsmanager, 70);
}


void render() {
    if (v.pendingResize) {
        reshape_video(v.pendingResizeWidth, v.pendingResizeHeight);
        v.pendingResize = false;
    }
    uint8_t* slice = malloc(p->framebufferSize * 3);
    for (int i = 0; i < p->framebufferSize; ++i) {
        slice[i*3+0] = (p->framebuffer[i] >> 16) & 0xff;
        slice[i*3+1] = (p->framebuffer[i] >> 8) & 0xff;
        slice[i*3+2] = p->framebuffer[i] & 0xff;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, v.tex);

    int w = p->overscanEnabled ? 240 : 256;
    int h = p->overscanEnabled ? 224 : 240;
    glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, slice);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0);
    glVertex3f(-1.0, -1.0, 0.0);
    glTexCoord2f(1.0, 1.0);
    glVertex3f(1.0, -1.0, 0.0);
    glTexCoord2f(1.0, 0.0);
    glVertex3f(1.0, 1.0, 0.0);
    glTexCoord2f(0.0, 0.0);
    glVertex3f(-1.0, 1.0, 0.0);
    glEnd();

    if (v.screen != NULL) {
        SDL_GL_SwapBuffers();
        SDL_framerateDelay(&v.fpsmanager);
    }
    free(slice);
}

int main() {
    p = Ppu_new();
    p->render = &render;
    Nametable_setMirroring(&p->nametables, rom_mirroring);
    assert(rom_chr_bank_count == 1);
    rom_read_chr(p->vram);
    init_video();
    rom_start();
    Ppu_dispose(p);
}
