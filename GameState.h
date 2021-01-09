#pragma once
#include "Misc.h"
#include <SDL.h>
#include <SDL_image.h>
#include <map>
#include <list>

class GridSquare
{
public:
    enum State
    {
        STATE_PIPE,
        STATE_VALVE,
    } state = STATE_PIPE;

    enum Connections
    {
        CONNECTIONS_NONE,
        CONNECTIONS_NW,
        CONNECTIONS_NE,
        CONNECTIONS_NS,
        CONNECTIONS_EW,
        CONNECTIONS_ES,
        CONNECTIONS_WS,
        CONNECTIONS_NWE,
        CONNECTIONS_NES,
        CONNECTIONS_NWS,
        CONNECTIONS_EWS,
        CONNECTIONS_NS_WE,
        CONNECTIONS_NW_ES,
        CONNECTIONS_NE_WS,
        CONNECTIONS_ALL,
    } connections = CONNECTIONS_NONE;

    Direction direction = DIRECTION_N;

    GridSquare()
    {
    }
};

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
    GridSquare squares[9][9];
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
