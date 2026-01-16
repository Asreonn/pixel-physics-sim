/*
 * render.h - SDL2-based rendering system
 */
#ifndef RENDER_H
#define RENDER_H

#include "core/types.h"
#include "world/world.h"
#include <SDL2/SDL.h>

/* =============================================================================
 * Debug Overlay Modes
 * ============================================================================= */

typedef enum {
    OVERLAY_NONE = 0,
    OVERLAY_CHUNKS,       /* Show active chunks */
    OVERLAY_UPDATED,      /* Show cells updated this tick */
    OVERLAY_MATERIAL,     /* Normal material view */
    OVERLAY_TEMPERATURE,  /* Temperature heatmap (future) */
    OVERLAY_PRESSURE,     /* Pressure heatmap (future) */
    OVERLAY_VELOCITY,     /* Velocity vectors (future) */
    OVERLAY_COUNT
} OverlayMode;

/* =============================================================================
 * Renderer State
 * ============================================================================= */

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;    /* Texture for pixel-level rendering */
    
    uint32_t* pixels;        /* Pixel buffer (ARGB format) */
    int width;
    int height;
    
    /* Debug overlay */
    OverlayMode overlay_mode;
    bool show_fps;
    bool show_stats;
    
    /* Performance */
    double frame_time_ms;
    double fps;
    
} Renderer;

/* =============================================================================
 * Renderer Functions
 * ============================================================================= */

/* Create renderer and window */
Renderer* render_create(int width, int height, const char* title);

/* Destroy renderer */
void render_destroy(Renderer* renderer);

/* Begin frame (clear buffers) */
void render_begin_frame(Renderer* renderer);

/* Render the world */
void render_world(Renderer* renderer, const World* world);

/* Render debug overlay */
void render_overlay(Renderer* renderer, const World* world);

/* Render UI/HUD elements */
void render_ui(Renderer* renderer, const World* world, double tick_time_ms, uint64_t tick_count, bool paused);

/* End frame (present to screen) */
void render_end_frame(Renderer* renderer);

/* Cycle to next overlay mode */
void render_cycle_overlay(Renderer* renderer);

/* Toggle FPS display */
void render_toggle_fps(Renderer* renderer);

/* Toggle stats display */
void render_toggle_stats(Renderer* renderer);

/* Update FPS counter */
void render_update_fps(Renderer* renderer, double delta_time);

/* Convert Color to uint32 ARGB */
uint32_t color_to_argb(Color c);

#endif /* RENDER_H */
