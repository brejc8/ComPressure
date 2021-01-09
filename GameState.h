#pragma once
#include "Misc.h"
#include "Circuit.h"
#include <SDL.h>
#include <SDL_image.h>
#include <map>
#include <list>

#define PRESSURE_SCALAR 1024

class GameState
{
    SDL_Window* sdl_window;
    SDL_Renderer* sdl_renderer;
    SDL_Texture* sdl_texture;

    enum MouseState
    {
        MOUSE_STATE_NONE,
        MOUSE_STATE_PIPE,
        MOUSE_STATE_DELETING,
        MOUSE_STATE_PLACING_VALVE,
    } mouse_state = MOUSE_STATE_NONE;

    Direction direction = DIRECTION_N;

    int scale = 3;
    Circuit* current_circuit;
    XYPos mouse;
    
    XYPos pipe_start_grid_pos;
    bool pipe_start_ns;
    
    bool mouse_button_left;
    bool mouse_button_middle;
    bool mouse_button_right;

public:
    GameState(const char* filename);
    void save(const char* filename);
    ~GameState();
    SDL_Texture* loadTexture();

    void render();
    void advance();
    bool events();
private:
    void draw_char(XYPos& pos, char c);
};
