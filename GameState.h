#pragma once
#include "Misc.h"
#include "Circuit.h"
#include "Level.h"
#include "Compress.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <map>
#include <list>
#include <set>

struct ServerResp
{
    SaveObject* resp = NULL;
    bool done = false;
    bool error = false;
    SDL_SpinLock working = 0;
};

class GameState
{
public:
    SDL_Window* sdl_window;
    SDL_Renderer* sdl_renderer;
    SDL_Texture* sdl_texture;
    SDL_Texture* sdl_tutorial_texture;
    SDL_Texture* sdl_levels_texture;
    TTF_Font *font;

    Rand rand = 3;
    enum MouseState
    {
        MOUSE_STATE_NONE,
        MOUSE_STATE_PIPE,
        MOUSE_STATE_PIPE_DRAGGING,
        MOUSE_STATE_DELETING,
        MOUSE_STATE_PLACING_VALVE,
        MOUSE_STATE_PLACING_SOURCE,
        MOUSE_STATE_PLACING_SUBCIRCUIT,
        MOUSE_STATE_PLACING_SIGN,
        MOUSE_STATE_SPEED_SLIDER,
        MOUSE_STATE_AREA_SELECT,
        MOUSE_STATE_DRAGGING_SIGN,
        MOUSE_STATE_ENTERING_TEXT_INTO_SIGN,
        MOUSE_STATE_PASTING_CLIPBOARD,
        MOUSE_STATE_ANIMATING

    } mouse_state = MOUSE_STATE_NONE;

    enum PanelState
    {
        PANEL_STATE_LEVEL_SELECT,
        PANEL_STATE_EDITOR,
        PANEL_STATE_MONITOR,
        PANEL_STATE_TEST,
        PANEL_STATE_SCORES

    } panel_state = PANEL_STATE_LEVEL_SELECT;

    DirFlip dir_flip;

    bool full_screen = false;
    int scale = 0;
    XYPos screen_offset = XYPos(0, 0);
    XYPos grid_offset = XYPos(32 * scale, 32 * scale);
    XYPos panel_offset = XYPos((8 + 32 * 11) * scale, (8 + 8 + 32) * scale);
    
    uint64_t steam_id = 0;
    const char* steam_username = "Charles Chavvington";
    std::set<uint64_t> friends;

    ServerResp scores_from_server;
    ServerResp design_from_server;

    LevelSet* level_set;
    LevelSet* edited_level_set;
    bool free_level_set_on_return = false;
    Level* current_level;

    Circuit* current_circuit = NULL;
    unsigned current_level_index = 0;
    unsigned placing_subcircuit_level;
    
    std::vector<CircuitElement*> inspection_stack;
    bool current_circuit_is_inspected_subcircuit = false;
    bool current_level_set_is_inspected = false;
    bool current_circuit_is_read_only = false;

    char* last_clip = NULL;
    LevelSet* clipboard_level_set = NULL;
    unsigned clipboard_level_index;

    LevelSet** deletable_level_set = NULL;

    bool skip_to_next_subtest = false;
    int skip_to_subtest_index = -1;

    unsigned frame_index = 0;
    unsigned game_speed = 20;

    unsigned slider_pos;
    Direction slider_direction;
    unsigned slider_max;
    unsigned* slider_value_tgt;
    unsigned slider_value_max;
    
    std::set<XYPos> selected_elements;
    XYPos select_area_pos;
    Clipboard clipboard;

    bool show_debug = false;
    unsigned debug_last_time = 0;
    unsigned debug_frames = 0;
    unsigned debug_simticks = 0;

    unsigned debug_last_second_frames = 0;
    unsigned debug_last_second_simticks = 0;
    unsigned minutes_played = 0;

    bool show_help = false;
    int show_help_page = 0;
    int level_win_animation = 0;
    int push_in_animation = 0;
    XYPos push_in_animation_grid;
    bool show_hint = false;
    bool flash_editor_menu = true;
    bool flash_steam_inlet = true;
    bool flash_valve = true;
    unsigned level_screen = 0;

    std::string tooltip_string;

//    bool requesting_help = false;

    bool show_confirm = false;
    XYPos confirm_box_pos;
    bool confirm_delete = false;
    int confirm_save_index = 0;


    bool show_dialogue = false;
    bool show_dialogue_hint = false;
    int dialogue_index = 0;
    int next_dialogue_level = 0;
    int highest_level = 0;

    bool show_main_menu = false;
    bool display_about = false;
    unsigned sound_volume = 50;
    unsigned music_volume = 50;
    Mix_Chunk *vent_steam_wav;
    Mix_Chunk *move_steam_wav;
    Mix_Music *music;
    
    XYPos mouse;
    
    bool keyboard_shift = false;
    bool keyboard_ctrl = false;
    
    XYPos pipe_start_grid_pos;
    bool pipe_start_ns;
    std::list<XYPos> pipe_drag_list;
    bool pipe_dragged = false;
    bool first_deletion = false;
    
    Sign dragged_sign;
    bool dragged_sign_motion;

    GameState(const char* filename);
    SaveObject* save(bool lite = false);
    void save(std::ostream& outfile, bool lite = false);
    void save(const char* filename, bool lite = false);
    void post_to_server(SaveObject* send, bool sync);
    void fetch_from_server(SaveObject* send, ServerResp* resp);
    void save_to_server(bool sync = false);
    void score_submit(unsigned level_index, bool sync = false);
    void score_fetch(unsigned level);
    void design_fetch(uint64_t design_steam_id, unsigned level_index);

    ~GameState();
    SDL_Texture* loadTexture(const char* filename);

    void audio();
    XYPos get_text_size(std::string& text);
    void render_texture(SDL_Rect& src_rect, SDL_Rect& dst_rect);
    void render_texture_custom(SDL_Texture* texture, SDL_Rect& src_rect, SDL_Rect& dst_rect);
    void render_number_2digit_err(XYPos pos, unsigned value, unsigned scale_mul = 1, unsigned bg_colour = 9, unsigned fg_colour = 0);
    void render_number_2digit(XYPos pos, unsigned value, unsigned scale_mul = 1, unsigned bg_colour = 9, unsigned fg_colour = 0);
    void render_number_pressure(XYPos pos, Pressure value, unsigned scale_mul = 1, unsigned bg_colour = 9, unsigned fg_colour = 0);
    void render_number_long(XYPos pos, unsigned value, unsigned scale_mul = 1);
    void render_box(XYPos pos, XYPos size, unsigned colour);
    void render_button(XYPos pos, XYPos content, unsigned colour, const char* tooltip = NULL);
    void render_tooltip();

    void render_text_wrapped(XYPos tl, const char* string, int width);
    void render_text(XYPos tl, const char* string, SDL_Color color = {0xff,0xff,0xff});
    void update_scale(int ewscale);
    void render(bool saving = false);
    void advance();
    void set_level(unsigned level_index);
    void mouse_click_in_grid(unsigned clicks);
    void mouse_click_in_panel();
    void mouse_motion();
    bool events();
    void watch_slider(unsigned slider_pos_, Direction slider_direction_, unsigned slider_max_, unsigned* slider_value_tgt_, unsigned slider_value_max_ = 0);
    void set_current_circuit_read_only();
    void check_clipboard();
    void deal_with_scores();
    void deal_with_design_fetch();

    void set_steam_user(uint64_t id, const char* name)
    {
        steam_id = id;
        steam_username = strdup(name);
    }
    void add_friend(uint64_t id)
    {
        friends.insert(id);
    }
};
