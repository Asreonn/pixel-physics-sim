/*
 * render.c - SDL2-based rendering implementation
 */
#include "render.h"
#include "material.h"
#include "fire.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* Glow settings */
#define GLOW_RADIUS 3
#define GLOW_INTENSITY 40

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

uint32_t color_to_argb(Color c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* =============================================================================
 * Renderer Lifecycle
 * ============================================================================= */

Renderer* render_create(int width, int height, const char* title) {
    Renderer* renderer = calloc(1, sizeof(Renderer));
    if (!renderer) return NULL;
    
    renderer->width = width;
    renderer->height = height;
    renderer->overlay_mode = OVERLAY_NONE;
    renderer->show_fps = true;
    renderer->show_stats = true;
    
    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(renderer);
        return NULL;
    }
    
    /* Create window */
    renderer->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN
    );
    if (!renderer->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Create renderer */
    renderer->renderer = SDL_CreateRenderer(
        renderer->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Create texture for pixel buffer */
    renderer->texture = SDL_CreateTexture(
        renderer->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );
    if (!renderer->texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer->renderer);
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Allocate pixel buffer */
    renderer->pixels = calloc((size_t)width * height, sizeof(uint32_t));
    if (!renderer->pixels) {
        SDL_DestroyTexture(renderer->texture);
        SDL_DestroyRenderer(renderer->renderer);
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    return renderer;
}

void render_destroy(Renderer* renderer) {
    if (!renderer) return;
    
    free(renderer->pixels);
    if (renderer->texture) SDL_DestroyTexture(renderer->texture);
    if (renderer->renderer) SDL_DestroyRenderer(renderer->renderer);
    if (renderer->window) SDL_DestroyWindow(renderer->window);
    SDL_Quit();
    free(renderer);
}

/* =============================================================================
 * Frame Rendering
 * ============================================================================= */

void render_begin_frame(Renderer* renderer) {
    /* Clear pixel buffer to black */
    size_t pixel_count = (size_t)renderer->width * renderer->height;
    for (size_t i = 0; i < pixel_count; i++) {
        renderer->pixels[i] = 0xFF000000;  /* Opaque black */
    }
}

void render_world(Renderer* renderer, const World* world) {
    /* Render each cell */
    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            int idx = y * renderer->width + x;
            int world_idx = IDX(x, y);
            MaterialID mat = world->mat[world_idx];
            
            Color c;
            
            /* Special handling for animated materials */
            if (mat == MAT_FIRE) {
                /* Fire uses lifetime-based color animation */
                c = fire_get_color(world->lifetime[world_idx]);
            } else if (mat == MAT_SMOKE) {
                /* Smoke fades with age */
                c = material_color(mat, world->color_seed[world_idx]);
                int age = world->lifetime[world_idx];
                /* Reduce alpha as smoke ages */
                int alpha = 150 - (age / 2);
                if (alpha < 20) alpha = 20;
                c.a = (uint8_t)alpha;
            } else {
                c = world_get_cell_color(world, x, y);
            }
            
            renderer->pixels[idx] = color_to_argb(c);
        }
    }
}

/* Apply glow effect around fire cells */
static void render_apply_glow(Renderer* renderer, const World* world) {
    /* First pass: find all fire cells and add glow */
    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            MaterialID mat = world_get_mat(world, x, y);
            if (mat != MAT_FIRE) continue;
            
            /* Get fire intensity based on lifetime (younger = brighter) */
            int lifetime = world->lifetime[IDX(x, y)];
            int intensity = GLOW_INTENSITY - (lifetime / 4);
            if (intensity < 10) intensity = 10;
            
            /* Apply glow to surrounding pixels */
            for (int dy = -GLOW_RADIUS; dy <= GLOW_RADIUS; dy++) {
                for (int dx = -GLOW_RADIUS; dx <= GLOW_RADIUS; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    if (!IN_BOUNDS(nx, ny)) continue;
                    
                    /* Skip if target is also fire */
                    if (world_get_mat(world, nx, ny) == MAT_FIRE) continue;
                    
                    int dist = abs(dx) + abs(dy);  /* Manhattan distance */
                    int glow_amount = intensity / dist;
                    
                    int pix_idx = ny * renderer->width + nx;
                    uint32_t pixel = renderer->pixels[pix_idx];
                    
                    /* Extract RGB */
                    uint8_t r = (pixel >> 16) & 0xFF;
                    uint8_t g = (pixel >> 8) & 0xFF;
                    uint8_t b = pixel & 0xFF;
                    
                    /* Add orange/yellow glow */
                    r = (uint8_t)MIN(255, r + glow_amount);
                    g = (uint8_t)MIN(255, g + glow_amount / 2);
                    /* b stays same for orange tint */
                    
                    renderer->pixels[pix_idx] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                }
            }
        }
    }
}

void render_overlay(Renderer* renderer, const World* world) {
    /* Always apply glow effect for fire (subtle, not debug) */
    render_apply_glow(renderer, world);
    
    switch (renderer->overlay_mode) {
        case OVERLAY_CHUNKS:
            /* Draw chunk boundaries and highlight active chunks */
            for (int cy = 0; cy < CHUNKS_Y; cy++) {
                for (int cx = 0; cx < CHUNKS_X; cx++) {
                    bool active = world_is_chunk_active(world, cx, cy);
                    
                    /* Draw chunk boundary */
                    int x0 = cx * CHUNK_SIZE;
                    int y0 = cy * CHUNK_SIZE;
                    int x1 = MIN(x0 + CHUNK_SIZE, renderer->width);
                    int y1 = MIN(y0 + CHUNK_SIZE, renderer->height);
                    
                    /* Tint active chunks green */
                    if (active) {
                        for (int y = y0; y < y1; y++) {
                            for (int x = x0; x < x1; x++) {
                                int idx = y * renderer->width + x;
                                uint32_t pixel = renderer->pixels[idx];
                                uint8_t r = (pixel >> 16) & 0xFF;
                                uint8_t g = (pixel >> 8) & 0xFF;
                                uint8_t b = pixel & 0xFF;
                                
                                /* Add green tint */
                                g = (uint8_t)MIN(255, g + 40);
                                renderer->pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
                            }
                        }
                    }
                    
                    /* Draw boundary lines (red) */
                    for (int x = x0; x < x1; x++) {
                        if (y0 < renderer->height) {
                            renderer->pixels[y0 * renderer->width + x] = 0xFFFF0000;
                        }
                    }
                    for (int y = y0; y < y1; y++) {
                        if (x0 < renderer->width) {
                            renderer->pixels[y * renderer->width + x0] = 0xFFFF0000;
                        }
                    }
                }
            }
            break;
            
        case OVERLAY_UPDATED:
            /* Highlight cells updated this tick */
            for (int y = 0; y < world->height; y++) {
                for (int x = 0; x < world->width; x++) {
                    if (world_has_flag(world, x, y, FLAG_UPDATED)) {
                        int idx = y * renderer->width + x;
                        /* Tint yellow */
                        renderer->pixels[idx] = 0xFFFFFF00;
                    }
                }
            }
            break;
            
        case OVERLAY_TEMPERATURE:
            /* Temperature heatmap overlay */
            for (int y = 0; y < world->height; y++) {
                for (int x = 0; x < world->width; x++) {
                    int idx = y * renderer->width + x;
                    float temp = world->temp[IDX(x, y)];
                    
                    /* Map temperature to color:
                     * < 0: Blue (cold)
                     * 0-20: Green-ish (ambient)
                     * 20-100: Yellow (warm)
                     * > 100: Red-Orange (hot)
                     * > 500: White (very hot)
                     */
                    uint8_t r, g, b;
                    
                    if (temp < 0) {
                        /* Cold: blue */
                        float cold = CLAMP(-temp / 50.0f, 0.0f, 1.0f);
                        r = 0;
                        g = (uint8_t)(100 * (1 - cold));
                        b = (uint8_t)(150 + 105 * cold);
                    } else if (temp < 20) {
                        /* Ambient: dark green */
                        r = 0;
                        g = (uint8_t)(50 + temp * 2);
                        b = 0;
                    } else if (temp < 100) {
                        /* Warm: yellow */
                        float warm = (temp - 20) / 80.0f;
                        r = (uint8_t)(255 * warm);
                        g = (uint8_t)(100 + 155 * warm);
                        b = 0;
                    } else if (temp < 500) {
                        /* Hot: orange to red */
                        float hot = (temp - 100) / 400.0f;
                        r = 255;
                        g = (uint8_t)(200 * (1 - hot));
                        b = 0;
                    } else {
                        /* Very hot: white */
                        float vhot = CLAMP((temp - 500) / 500.0f, 0.0f, 1.0f);
                        r = 255;
                        g = (uint8_t)(200 + 55 * vhot);
                        b = (uint8_t)(200 * vhot);
                    }
                    
                    /* Blend with existing pixel (50% overlay) */
                    uint32_t pixel = renderer->pixels[idx];
                    uint8_t orig_r = (pixel >> 16) & 0xFF;
                    uint8_t orig_g = (pixel >> 8) & 0xFF;
                    uint8_t orig_b = pixel & 0xFF;
                    
                    r = (uint8_t)((r + orig_r) / 2);
                    g = (uint8_t)((g + orig_g) / 2);
                    b = (uint8_t)((b + orig_b) / 2);
                    
                    renderer->pixels[idx] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                }
            }
            break;
            
        default:
            break;
    }
}

void render_ui(Renderer* renderer, const World* world, double tick_time_ms, uint64_t tick_count, bool paused) {
    /* UI rendering would typically use SDL_ttf for text
     * For now, we just update internal stats
     * A full implementation would draw text overlays */
    (void)renderer;
    (void)world;
    (void)tick_time_ms;
    (void)tick_count;
    (void)paused;
    
    /* In a full implementation:
     * - Draw FPS counter
     * - Draw tick count
     * - Draw cells updated
     * - Draw active chunks
     * - Draw current brush/material
     * - Draw pause indicator
     */
}

void render_end_frame(Renderer* renderer) {
    /* Update texture from pixel buffer */
    SDL_UpdateTexture(
        renderer->texture, NULL,
        renderer->pixels,
        renderer->width * sizeof(uint32_t)
    );
    
    /* Clear and draw */
    SDL_RenderClear(renderer->renderer);
    SDL_RenderCopy(renderer->renderer, renderer->texture, NULL, NULL);
    SDL_RenderPresent(renderer->renderer);
}

/* =============================================================================
 * Overlay and Display Toggles
 * ============================================================================= */

void render_cycle_overlay(Renderer* renderer) {
    renderer->overlay_mode = (OverlayMode)((renderer->overlay_mode + 1) % OVERLAY_COUNT);
}

void render_toggle_fps(Renderer* renderer) {
    renderer->show_fps = !renderer->show_fps;
}

void render_toggle_stats(Renderer* renderer) {
    renderer->show_stats = !renderer->show_stats;
}

void render_update_fps(Renderer* renderer, double delta_time) {
    renderer->frame_time_ms = delta_time * 1000.0;
    if (delta_time > 0) {
        renderer->fps = 1.0 / delta_time;
    }
}
