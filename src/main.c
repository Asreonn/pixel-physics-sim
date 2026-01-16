/*
 * main.c - Entry point for Pixel-Cell Physics Simulator
 * 
 * Milestone A: Sand simulation with fixed timestep
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core/types.h"
#include "materials/material.h"
#include "world/world.h"
#include "engine/simulation.h"
#include "engine/render.h"
#include "engine/input.h"

/* =============================================================================
 * Main Entry Point
 * ============================================================================= */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    printf("Pixel-Cell Physics Simulator - Full Simulation\n");
    printf("=================================================\n");
    printf("Controls:\n");
    printf("  Left Click   - Paint material\n");
    printf("  Right Click  - Erase\n");
    printf("  Mouse Wheel  - Change brush size\n");
    printf("  1 = Sand, 2 = Stone, 3 = Water, 4 = Wood\n");
    printf("  5 = Soil, 6 = Fire, 7 = Smoke, 8 = Empty\n");
    printf("  9 = Ice, 0 = Steam, - = Ash, = = Acid\n");
    printf("  Space        - Pause/Unpause\n");
    printf("  Period (.)   - Step one tick (when paused)\n");
    printf("  C            - Clear world\n");
    printf("  Tab          - Cycle debug overlay (incl. Temperature)\n");
    printf("  Escape       - Quit\n");
    printf("=================================================\n");
    
    /* Initialize material system */
    material_init();
    
    /* Create world */
    World* world = world_create(GRID_WIDTH, GRID_HEIGHT);
    if (!world) {
        fprintf(stderr, "Failed to create world\n");
        return 1;
    }
    
    /* Create simulation */
    Simulation* sim = simulation_create(TICK_HZ);
    if (!sim) {
        fprintf(stderr, "Failed to create simulation\n");
        world_destroy(world);
        return 1;
    }
    
    /* Create renderer */
    Renderer* renderer = render_create(WINDOW_WIDTH, WINDOW_HEIGHT, "Pixel Simulator - Sand (Phase A)");
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        simulation_destroy(sim);
        world_destroy(world);
        return 1;
    }
    
    /* Create input handler */
    Input* input = input_create();
    if (!input) {
        fprintf(stderr, "Failed to create input handler\n");
        render_destroy(renderer);
        simulation_destroy(sim);
        world_destroy(world);
        return 1;
    }
    
    /* Create initial ground/walls for testing */
    /* Bottom wall */
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = GRID_HEIGHT - 10; y < GRID_HEIGHT; y++) {
            world_set_mat(world, x, y, MAT_STONE);
        }
    }
    
    /* Left wall */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < 10; x++) {
            world_set_mat(world, x, y, MAT_STONE);
        }
    }
    
    /* Right wall */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = GRID_WIDTH - 10; x < GRID_WIDTH; x++) {
            world_set_mat(world, x, y, MAT_STONE);
        }
    }
    
    /* Platform in the middle */
    for (int x = 150; x < 350; x++) {
        for (int y = 350; y < 360; y++) {
            world_set_mat(world, x, y, MAT_STONE);
        }
    }
    
    /* Activate all chunks initially */
    for (int cy = 0; cy < CHUNKS_Y; cy++) {
        for (int cx = 0; cx < CHUNKS_X; cx++) {
            world_activate_chunk(world, cx, cy);
        }
    }
    world_update_chunk_activation(world);
    
    /* Main loop timing */
    uint64_t last_time = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();
    
    uint64_t frame_count = 0;
    double fps_timer = 0.0;
    
    printf("Starting main loop...\n");
    
    /* Main loop */
    while (!input->quit_requested) {
        /* Calculate delta time */
        uint64_t current_time = SDL_GetPerformanceCounter();
        double delta_time = (double)(current_time - last_time) / (double)freq;
        last_time = current_time;
        
        /* Cap delta time to prevent spiral of death */
        if (delta_time > 0.1) {
            delta_time = 0.1;
        }
        
        /* Update input */
        input_update(input);
        
        /* Apply input to world */
        input_apply(input, world, sim, renderer);
        
        /* Update simulation */
        simulation_update(sim, world, delta_time);
        
        /* Render */
        render_begin_frame(renderer);
        render_world(renderer, world);
        render_overlay(renderer, world);
        render_ui(renderer, world, sim->tick_time_ms, sim->tick_count, sim->paused);
        render_end_frame(renderer);
        
        /* Update FPS counter */
        render_update_fps(renderer, delta_time);
        
        /* FPS display */
        fps_timer += delta_time;
        frame_count++;
        if (fps_timer >= 1.0) {
            printf("FPS: %.1f | Ticks: %llu | Cells: %u | Chunks: %u | %s [%d] | %s\n",
                   (double)frame_count / fps_timer,
                   (unsigned long long)sim->tick_count,
                   world->cells_updated,
                   world->active_chunks,
                   input_get_material_name(input),
                   input->brush_size,
                   sim->paused ? "PAUSED" : "RUNNING");
            printf("  Profile: powder=%.0fus fluid=%.0fus fire=%.0fus gas=%.0fus total=%.0fus\n",
                   sim->profile_powder_us,
                   sim->profile_fluid_us,
                   sim->profile_fire_us,
                   sim->profile_gas_us,
                   sim->profile_total_us);
            fps_timer = 0.0;
            frame_count = 0;
        }
    }
    
    printf("Shutting down...\n");
    
    /* Cleanup */
    input_destroy(input);
    render_destroy(renderer);
    simulation_destroy(sim);
    world_destroy(world);
    
    printf("Done.\n");
    return 0;
}
