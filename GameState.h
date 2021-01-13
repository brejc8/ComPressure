#pragma once
#include "Misc.h"
#include "Circuit.h"
#include "Level.h"
#include <SDL.h>
#include <SDL_image.h>
#include <map>
#include <list>

class GameState
{
    SDL_Window* sdl_window;
    SDL_Renderer* sdl_renderer;
    SDL_Texture* sdl_texture;

    Rand rand = 3;
    enum MouseState
    {
        MOUSE_STATE_NONE,
        MOUSE_STATE_PIPE,
        MOUSE_STATE_DELETING,
        MOUSE_STATE_PLACING_VALVE,
        MOUSE_STATE_PLACING_SOURCE,
        MOUSE_STATE_PLACING_SUBCIRCUIT,
    } mouse_state = MOUSE_STATE_NONE;

    enum PanelState
    {
        PANEL_STATE_LEVEL_SELECT,
        PANEL_STATE_EDITOR,
        PANEL_STATE_MONITOR
    } panel_state = PANEL_STATE_LEVEL_SELECT;

    Direction direction = DIRECTION_N;

    const int scale = 3;
    const XYPos grid_offset = XYPos(32 * scale, 32 * scale);
    const XYPos panel_offset = XYPos((16 + 32 * 11) * scale, (16 + 8 + 32) * scale);

    LevelSet* level_set;
    Level* current_level;

    Circuit* current_circuit = NULL;
    unsigned current_level_index = 0;
    unsigned placing_subcircuit_level;
    unsigned selected_monitor = 0;
    unsigned frame_index = 0;
    XYPos mouse;
    
    XYPos pipe_start_grid_pos;
    bool pipe_start_ns;

public:
    GameState(const char* filename);
    void save(const char* filename);
    ~GameState();
    SDL_Texture* loadTexture();

    void render();
    void advance();
    void set_level(unsigned level_index);
    void mouse_click_in_grid();
    void mouse_click_in_panel();
    bool events();
private:
    void draw_char(XYPos& pos, char c);
};
