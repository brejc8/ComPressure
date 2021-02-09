#define _USE_MATH_DEFINES
#include <cmath>

#include "GameState.h"
#include "SaveState.h"
#include "Misc.h"

#include <cassert>
#include <SDL.h>
#include <SDL_mixer.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <limits>
#include <math.h>


GameState::GameState(const char* filename)
{
    std::ifstream loadfile(filename);
    if (loadfile.fail() || loadfile.eof())
    {
        level_set = new LevelSet();
        current_level_index = 0;
        scale = 3;
    }
    else
    {
        SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();
        level_set = new LevelSet(omap->get_item("levels"));
        
        current_level_index = omap->get_num("current_level_index");
        if (current_level_index >= LEVEL_COUNT)
            current_level_index = 0;
        game_speed = omap->get_num("game_speed");
        show_debug = omap->get_num("show_debug");
        show_help = omap->get_num("show_help");
        show_help_page = omap->get_num("show_help_page");
        
        sound_volume = omap->get_num("sound_volume");
        music_volume = omap->get_num("music_volume");
        scale = omap->get_num("scale");
        if (scale < 1)
            scale = 3;
        full_screen = omap->get_num("full_screen");

        delete omap;
    }
    set_level(current_level_index);

    sdl_window = SDL_CreateWindow( "ComPressure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640*scale, 360*scale, SDL_WINDOW_RESIZABLE);
    SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
	sdl_texture = loadTexture("texture.png");
	sdl_tutorial_texture = loadTexture("tutorial.png");
	sdl_levels_texture = loadTexture("levels.png");
	sdl_font_texture = loadTexture("font.png");
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
    scale = 0;
    
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    Mix_AllocateChannels(16);
    
    vent_steam_wav = Mix_LoadWAV("vent_steam.wav");
    Mix_PlayChannel(0, vent_steam_wav, -1);
    Mix_Volume(0, 0);
    
    move_steam_wav = Mix_LoadWAV("move_steam.wav");
    Mix_PlayChannel(1, move_steam_wav, -1);
    Mix_Volume(1, 0);
    
    Mix_VolumeMusic(music_volume);
    music = Mix_LoadMUS("music.ogg");
    Mix_PlayMusic(music, -1);
    
    top_level_allowed = level_set->top_playable();
    
    
}
    

void GameState::save(const char*  filename)
{
    std::ofstream outfile (filename);
    outfile << std::setprecision(std::numeric_limits<double>::digits);
    SaveObjectMap omap;
    
    omap.add_item("levels", level_set->save());
    omap.add_num("current_level_index", current_level_index);
    omap.add_num("game_speed", game_speed);
    omap.add_num("show_debug", show_debug);
    omap.add_num("show_help", show_help);
    omap.add_num("show_help_page", show_help_page);
    omap.add_num("scale", scale);
    omap.add_num("full_screen", full_screen);
    omap.add_num("sound_volume", sound_volume);
    omap.add_num("music_volume", music_volume);

    omap.save(outfile);
}


GameState::~GameState()
{
    Mix_FreeChunk(vent_steam_wav);
    Mix_FreeChunk(move_steam_wav);
    Mix_FreeMusic(music);

	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyTexture(sdl_tutorial_texture);
	SDL_DestroyTexture(sdl_levels_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);
    delete level_set;
}

extern const char embedded_data_binary_texture_png_start;
extern const char embedded_data_binary_texture_png_end;




SDL_Texture* GameState::loadTexture(const char* filename)
{
    SDL_Surface* loadedSurface = IMG_Load(filename);
//    SDL_Surface* loadedSurface = IMG_Load_RW(SDL_RWFromMem((void*)&embedded_data_binary_texture_png_start,
//                                    &embedded_data_binary_texture_png_end - &embedded_data_binary_texture_png_start),1);
	assert(loadedSurface);
    SDL_Texture* newTexture = SDL_CreateTextureFromSurface(sdl_renderer, loadedSurface);
	assert(newTexture);
	SDL_FreeSurface(loadedSurface);
	return newTexture;
}


void GameState::advance()
{
    unsigned period = 200;
    unsigned time = SDL_GetTicks();

    if (SDL_TICKS_PASSED(time, debug_last_time + period))
    {
        float mul = 1000 / float(time - debug_last_time);
        debug_last_second_simticks = round(debug_simticks * mul);
        debug_last_second_frames = round(debug_frames * mul);
        debug_simticks = 0;
        debug_frames = 0;
        debug_last_time = SDL_GetTicks();
    }

    int count = pow(1.2, game_speed) * 2;
    if (monitor_state != MONITOR_STATE_PAUSE)
    {
        while (count)
        {
            int subcount = count < 100 ? count : 100;
            current_level->advance(subcount, monitor_state);
            count -= subcount;
            debug_simticks += subcount;
            if (SDL_TICKS_PASSED(SDL_GetTicks(), time + 100))
            {
                game_speed--;
                break;
            }
        }
        {
            SimPoint& simp = current_level->tests[current_level->test_index].sim_points[current_level->sim_point_index];
            for (int p = 0; p < 4; p++)
            {
                test_value[p] = simp.values[p];
                test_drive[p] = simp.force/3;
            }
            test_drive[current_level->tests[current_level->test_index].tested_direction] = 0;
            test_pressure_histroy_index = 0;
            test_pressure_histroy_sample_downcounter = 0;
            for (int i = 0; i < 192; i++)
            {
                test_pressure_histroy[i].values[0]=0;
                test_pressure_histroy[i].values[1]=0;
                test_pressure_histroy[i].values[2]=0;
                test_pressure_histroy[i].values[3]=0;
                test_pressure_histroy[i].set = false;
            }
        }



    }
    else
    {
        while (count)
        {
            int subcount = count < 100 ? count : 100;
            for (int i = 0; i < subcount; i++)
            {
                for (int p = 0; p < 4; p++)
                {
                    test_pressures[p].apply(test_value[p], test_drive[p] * 3);
                    test_pressures[p].pre();
                }
                PressureAdjacent adj(test_pressures[0], test_pressures[1],test_pressures[2],test_pressures[3]);

                current_circuit->sim_pre(adj);
                current_circuit->sim_post(adj);

                for (int p = 0; p < 4; p++)
                {
                    test_pressures[p].post();
                }

                if (test_pressure_histroy_sample_downcounter == 0 )
                {
                    test_pressure_histroy_sample_downcounter = 100; // test_pressure_histroy_sample_frequency;

                    for (int p = 0; p < 4; p++)
                        test_pressure_histroy[test_pressure_histroy_index].values[p] = pressure_as_percent(test_pressures[p].value);
                    test_pressure_histroy[test_pressure_histroy_index].set = true;

                    test_pressure_histroy_index = (test_pressure_histroy_index + 1) % 192;
                    
                }
                test_pressure_histroy_sample_downcounter--;
                
            }
            count -= subcount;
            debug_simticks += subcount;
            if (SDL_TICKS_PASSED(SDL_GetTicks(), time + 100))
            {
                game_speed--;
                break;
            }
        }
    }
}

void GameState::audio()
{
    Mix_Volume(0, pressure_as_percent(current_circuit->last_vented*40) * sound_volume / 100 / 5);
    Mix_Volume(1, pressure_as_percent(current_circuit->last_moved*1) * sound_volume / 100/ 2);
    Mix_VolumeMusic(music_volume);
}

void GameState::render_number_2digit(XYPos pos, unsigned value, unsigned scale_mul, unsigned bg_colour, unsigned fg_colour)
{
    int myscale = scale * scale_mul;
    {
        SDL_Rect src_rect = {503, 80 + int(bg_colour), 1, 1};
        SDL_Rect dst_rect = {pos.x, pos.y, 9 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    if (value == 100)
    {
        SDL_Rect src_rect = {40 + int(fg_colour/4) * 64, 160 + int(fg_colour%4) * 5, 9, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 9 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    else
    {
        SDL_Rect src_rect = {0 + int(fg_colour/4) * 64 + (int(value) / 10) * 4, 160 + int(fg_colour%4) * 5, 4, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        src_rect.w = 5;
        src_rect.x = 0 + (value % 10) * 4 + int(fg_colour/4) * 64;
        dst_rect.w = 5 * myscale;
        dst_rect.x += 4 * myscale;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

}


void GameState::render_number_pressure(XYPos pos, Pressure value, unsigned scale_mul, unsigned bg_colour, unsigned fg_colour)
{
    int myscale = scale * scale_mul;
    unsigned p = (value * 100 + PRESSURE_SCALAR / 2) / PRESSURE_SCALAR;
    render_number_2digit(pos, p / 100, scale_mul, bg_colour, fg_colour);
    pos.x += 9 * myscale;
    {
        SDL_Rect src_rect = {49 + int(fg_colour/4) * 64, 160 + int(fg_colour%4) * 5, 1, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 1 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    pos.x += 1 * myscale;
    render_number_2digit(pos, p % 100, scale_mul, bg_colour, fg_colour);
    
}


void GameState::render_number_long(XYPos pos, unsigned value, unsigned scale_mul)
{
    int myscale = scale * scale_mul;
    unsigned digits[10];
    
    int i = 10;
    while (value)
    {
        i--;
        digits[i] = value % 10;
        value /= 10;
    }
    
    SDL_Rect src_rect = {0, 160, 4, 5};
    SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
    while (i < 10)
    {
        src_rect.x = 0 + digits[i] * 4;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.x += 4 * myscale;
        i++;

    }
}

void GameState::render_box(XYPos pos, XYPos size, unsigned colour)
{
    int color_table = 256 + colour * 32;
    SDL_Rect src_rect = {color_table, 80, 16, 16};
    SDL_Rect dst_rect = {pos.x, pos.y, 16 * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Top Left

    src_rect = {color_table + 16, 80, 1, 16};
    dst_rect = {pos.x + 16 * scale, pos.y, (size.x - 32) * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Top

    src_rect = {color_table + 16, 80, 16, 16};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y, 16 * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Top Right
    
    src_rect = {color_table, 80 + 16, 16, 1};
    dst_rect = {pos.x, pos.y + 16 * scale, 16 * scale, (size.y - 32) * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Left

    src_rect = {color_table + 16, 80 + 16, 1, 1};
    dst_rect = {pos.x + 16 * scale, pos.y + 16 * scale, (size.x - 32) * scale, (size.y - 32) * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Middle

    src_rect = {color_table + 16, 80 + 16, 16, 1};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y + 16 * scale, 16 * scale, (size.y - 32) * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Right

    src_rect = {color_table, 80 + 16, 16, 16};
    dst_rect = {pos.x, pos.y + (size.y - 16) * scale, 16 * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Bottom Left

    src_rect = {color_table + 16, 80 + 16, 1, 16};
    dst_rect = {pos.x + 16 * scale, pos.y + (size.y - 16) * scale, (size.x - 32) * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Bottom

    src_rect = {color_table + 16, 80 + 16, 16, 16};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y + (size.y - 16) * scale, 16 * scale, 16 * scale};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);     //  Bottom Right
}


void GameState::render_text(XYPos tl, const char* string)
{
    XYPos pos;
    while (int c = (unsigned char)*string)
    {
        if (c == '\n')
        {
            pos.y++;
            pos.x = 0;
        }
        else
        {
            c -= 0x20;
            if (c >= 128)
                c-= 32;
            SDL_Rect src_rect = {(c % 16) * 7, (c / 16) * 9, 7, 9};
            SDL_Rect dst_rect = {(tl.x + pos.x * 7) * scale, (tl.y + pos.y * 9) * scale, 7 * scale, 9 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_font_texture, &src_rect, &dst_rect);
            pos.x++;
        }
        string++;
    }
}

void GameState::render()
{
    SDL_RenderClear(sdl_renderer);
    XYPos window_size;
    SDL_GetWindowSize(sdl_window, &window_size.x, &window_size.y);
    {
        int sx = window_size.x / 640;
        int sy = window_size.y / 360;
        int newscale = std::min(sy, sx);
        if (newscale < 1)
            newscale = 1;
        if (scale != newscale)
        {
            scale = newscale;
            grid_offset = XYPos(32 * scale, 32 * scale);
            panel_offset = XYPos((8 + 32 * 11) * scale, (8 + 8 + 32) * scale);
        }
    }
    
    XYPos pos;
    frame_index++;
    debug_frames++;
    
    if (top_level_allowed != level_set->top_playable())
    {
        top_level_allowed = level_set->top_playable();
        level_win_animation = 100;
    
    }
    
    {
        SDL_Rect src_rect = {320, 300, 320, 180};       // Background
        SDL_Rect dst_rect = {0, 0, 640*scale, 360*scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.x = 640*scale;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.x = 0;
        dst_rect.y = 360*scale;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.x = 640*scale;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    {                                               // Input pipe background
        render_box(XYPos((4 * 32 - 8) * scale + grid_offset.x, (-32 - 16) * scale + grid_offset.y), XYPos(48, 48), 0);
        render_box(XYPos((9 * 32 - 0) * scale + grid_offset.x, (4 * 32 - 8) * scale + grid_offset.y), XYPos(48, 48), 1);
        render_box(XYPos((4 * 32 - 8) * scale + grid_offset.x, (9 * 32 - 0) * scale + grid_offset.y), XYPos(48, 48), 2);
        render_box(XYPos((-32 - 16) * scale + grid_offset.x, (4 * 32 - 8) * scale + grid_offset.y), XYPos(48, 48), 3);
    }

    {                                               // Input pipes
        SDL_Rect src_rect = {160, 0, 32, 32};       // W
        SDL_Rect dst_rect = {-32 * scale + grid_offset.x, (4 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        src_rect = {224, 0, 32, 32};                // E
        dst_rect = {(9 * 32) * scale + grid_offset.x, (4 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        src_rect = {192, 0, 32, 32};                // N
        dst_rect = {(4 * 32) * scale + grid_offset.x, -32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        src_rect = {128, 0, 32, 32};                // S
        dst_rect = {(4 * 32) * scale + grid_offset.x, (9 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        SDL_Rect src_rect = {352, 144, 32, 32};
        if (pos.y > 0)
            src_rect.y += 32;
        if (pos.y == 8)
            src_rect.y += 32;
        if (pos.x > 0)
            src_rect.x += 32;
        if (pos.x == 8)
            src_rect.x += 32;
        if (current_level->blocked[pos.y][pos.x])
            src_rect = {384, 80, 32, 32};
            
        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    
    for (pos.y = 0; pos.y < 9; pos.y++)                                         // Draw elements
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        {
            SDL_Rect src_rect = current_circuit->elements[pos.y][pos.x]->getimage_bg();
            if (src_rect.w)
            {
                int xoffset = (32 - src_rect.w) / 2;
                int yoffset = (32 - src_rect.h) / 2;
                SDL_Rect dst_rect = {(pos.x * 32  + xoffset) * scale + grid_offset.x, (pos.y * 32 + yoffset) * scale + grid_offset.y, src_rect.w * scale, src_rect.h * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }


        XYPos src_pos = current_circuit->elements[pos.y][pos.x]->getimage();
        if (src_pos != XYPos(0,0))
        {
            SDL_Rect src_rect = {src_pos.x, src_pos.y, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        src_pos = current_circuit->elements[pos.y][pos.x]->getimage_fg();
        if (src_pos != XYPos(0,0))
        {
            SDL_Rect src_rect =  {src_pos.x, src_pos.y, 24, 24};
            SDL_Rect dst_rect = {(pos.x * 32 + 4) * scale + grid_offset.x, (pos.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        bool selected = false;
        selected = selected_elements.find(pos) != selected_elements.end();
        
        if (mouse_state == MOUSE_STATE_AREA_SELECT)
        {
            XYPos tl = ((mouse - grid_offset) / scale) / 32;
            XYPos br = select_area_pos / 32;

            if (tl.x > br.x)
            {
                int t = tl.x;
                tl.x = br.x;
                br.x = t;
            }
            if (tl.y > br.y)
            {
                int t = tl.y;
                tl.y = br.y;
                br.y = t;
            }
            if (pos.x >= tl.x && pos.x <= br.x && pos.y >= tl.y && pos.y <= br.y && !current_circuit->elements[pos.y][pos.x]->is_empty())
                selected = true;
        }
        
        if (selected)                     //selected
        {
            SDL_Rect src_rect =  {256, 176, 32, 32};
            SDL_Rect dst_rect = {(pos.x * 32) * scale + grid_offset.x, (pos.y * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

    }

    if (mouse_state == MOUSE_STATE_PIPE && (frame_index % 20 < 10))
    {
        Connections con;
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
        if (pipe_start_ns)
        {
            mouse_rel.x -= 16;
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_level->blocked[pipe_start_grid_pos.y - 1][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y - 1;


            if (mouse_rel.y >= 0)   //north - southwards
            {
                    if (mouse_rel.y > abs(mouse_rel.x) + 16)     // south
                        con = CONNECTIONS_NS;
                    else if (mouse_rel.x >= 0)               // east
                        con = CONNECTIONS_NE;
                    else                                    // west
                        con = CONNECTIONS_NW;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale + grid_offset.x, pipe_start_grid_pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            else                                        //south - northwards
            {
                    if (-mouse_rel.y > abs(mouse_rel.x) + 16)    // north
                        con = CONNECTIONS_NS;
                    else if (mouse_rel.x >= 0)               // east
                        con = CONNECTIONS_ES;
                    else                                    // west
                        con = CONNECTIONS_WS;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale + grid_offset.x, (pipe_start_grid_pos.y - 1) * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
//             SDL_Rect src_rect = {503, 80, 1, 1};
//             SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 + 16 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
//             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        else
        {
            mouse_rel.y -= 16;
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x - 1]))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
                mouse_rel.x = -mouse_rel.x - 1;
            if (mouse_rel.x >= 0)   //west - eastwards
            {
                    if (mouse_rel.x > abs(mouse_rel.y) + 16)     // west
                        con = CONNECTIONS_EW;
                    else if (mouse_rel.y >= 0)               // south
                        con = CONNECTIONS_WS;
                    else                                    // north
                        con = CONNECTIONS_NW;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale + grid_offset.x, pipe_start_grid_pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            else                                        //east - westwards
            {
                    if (-mouse_rel.x > abs(mouse_rel.y) + 16)    // east
                        con = CONNECTIONS_EW;
                    else if (mouse_rel.y >= 0)               // south
                        con = CONNECTIONS_ES;
                    else                                    // north
                        con = CONNECTIONS_NE;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {(pipe_start_grid_pos.x - 1) * 32 * scale + grid_offset.x, pipe_start_grid_pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
//             SDL_Rect src_rect = {503, 80, 1, 1};
//             SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 + 16 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
//             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset)/ scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            SDL_Rect src_rect = {direction * 32, 4 * 32, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            SDL_Rect src_rect = {128 + direction * 32, 0, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            XYPos pos = level_set->levels[placing_subcircuit_level]->getimage(direction);
            if (pos != XYPos(0,0))
            {
                SDL_Rect src_rect = {pos.x, pos.y, 32, 32};
                SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }


            pos = level_set->levels[placing_subcircuit_level]->getimage_fg(direction);
            if (pos != XYPos(0,0))
            {
                SDL_Rect src_rect = {pos.x, pos.y, 24, 24};
                SDL_Rect dst_rect = {(mouse_grid.x * 32 + 4) * scale + grid_offset.x, (mouse_grid.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }

        }
    }
    else if (mouse_state == MOUSE_STATE_AREA_SELECT)
    {
        XYPos mouse_pos = ((mouse - grid_offset) / scale);
        
        XYPos tl = mouse_pos;
        XYPos br = select_area_pos;
        
        if (tl.x > br.x)
        {
            int t = tl.x;
            tl.x = br.x;
            br.x = t;
        }
        if (tl.y > br.y)
        {
            int t = tl.y;
            tl.y = br.y;
            br.y = t;
        }
        XYPos size = br - tl;
        
        {
            SDL_Rect src_rect = {503, 80, 1, 1};
            {
                SDL_Rect dst_rect = {tl.x * scale + grid_offset.x, tl.y * scale + grid_offset.y, size.x * scale, 1 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            {
                SDL_Rect dst_rect = {tl.x * scale + grid_offset.x, (tl.y + size.y) * scale + grid_offset.y, (size.x + 1) * scale, 1 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            {
                SDL_Rect dst_rect = {tl.x * scale + grid_offset.x, tl.y * scale + grid_offset.y, 1 * scale, size.y * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            {
                SDL_Rect dst_rect = {(tl.x + size.x) * scale + grid_offset.x, tl.y * scale + grid_offset.y, 1 * scale, (size.y + 1) * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }
    }

    for (pos.y = 0; pos.y < 10; pos.y++)                        // Print pressure numbers
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (!current_circuit->connections_ns[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ns[pos.y][pos.x].vented);
        unsigned value = pressure_as_percent(current_circuit->connections_ns[pos.y][pos.x].value);
        
        if (vented > 20)
        {
            SDL_Rect src_rect = {16*int(rand & 3) + 256, 160, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  + 7 + int(rand % 3)) * scale + grid_offset.x, (pos.y * 32 - 9  + int(rand % 3)) * scale + grid_offset.y, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        render_number_2digit(XYPos((pos.x * 32  + 11) * scale + grid_offset.x, (pos.y * 32 - 3) * scale + grid_offset.y), value, 1, 6);
    }
    for (pos.y = 0; pos.y < 9; pos.y++)                         // Show venting pipes
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        if (!current_circuit->connections_ew[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ew[pos.y][pos.x].vented);
        unsigned value = pressure_as_percent(current_circuit->connections_ew[pos.y][pos.x].value);
        if (vented > 20)
        {
            SDL_Rect src_rect = {16*int(rand & 3) + 256, 160, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  - 9) * scale + int(rand % 3 * scale) + grid_offset.x, (pos.y * 32 + 7 ) * scale + int(rand % 3 * scale) + grid_offset.y, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        render_number_2digit(XYPos((pos.x * 32  - 5) * scale + grid_offset.x, (pos.y * 32 + 13) * scale + grid_offset.y), value, 1, 6);
    }
    
    
    {                                               // Panel background
        {
            int panel_colour = 0;
            switch (panel_state)
            {
                case PANEL_STATE_LEVEL_SELECT:
                    panel_colour = 1;
                    break;
                case PANEL_STATE_EDITOR:
                    panel_colour = 0;
                    break;
                case PANEL_STATE_MONITOR:
                    panel_colour = 2;
                    break;
                case PANEL_STATE_TEST:
                    panel_colour = 4;
                    break;
                default:
                    assert(0);
            }
            
            render_box(XYPos((32 * 11) * scale, 0), XYPos(8*32 + 16, 11*32 + 8), panel_colour);
        }
        {                                                                                               // Top Menu
            SDL_Rect src_rect = {256, 112, 32, 32};
            SDL_Rect dst_rect = {(8 + 32 * 11) * scale, (8) * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        if (level_set->is_playable(1)){
            SDL_Rect src_rect = {256+32, 112, 32, 32};
            SDL_Rect dst_rect = {(8 + 32 * 12) * scale, (8) * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        if (level_set->is_playable(3)){
            SDL_Rect src_rect = {256+64, 112, 32, 32};
            SDL_Rect dst_rect = {(8 + 32 * 13) * scale, (8) * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        if (level_set->is_playable(5)){
            SDL_Rect src_rect = {256+96, 112, 32, 32};
            SDL_Rect dst_rect = {(8 + 32 * 14) * scale, (8) * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {
            SDL_Rect src_rect = {256+128, 112, 96, 32};
            SDL_Rect dst_rect = {(8 + 32 * 15) * scale, (8) * scale, 96 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {                                                                                               // Speed slider
            SDL_Rect src_rect = {624, 16, 16, 16};
            SDL_Rect dst_rect = {(8 + 32 * 11 + 32 * 5 + int(game_speed)) * scale, (8 + 8) * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {                                                                                               // Current Score
            render_number_2digit(XYPos((8 + 32 * 11 + 32 * 4 + 3) * scale, (8 + 8) * scale), pressure_as_percent(current_level->best_score), 3);
        }
        {                                                                                               // Help Button
            render_box(XYPos((8 + 32 * 11 + 7 * 32) * scale, (8) * scale), XYPos(32, 32), 0);
            SDL_Rect src_rect = {640-96, 208, 32, 32};
            SDL_Rect dst_rect = {(8 + 32 * 11 + 7 * 32) * scale, (8) * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

    }

    
    
    if (panel_state == PANEL_STATE_LEVEL_SELECT)
    {
        unsigned level_index = 0;
        
        for (pos.y = 0; pos.y < 8; pos.y++)
        for (pos.x = 0; pos.x < 8; pos.x++)
        {
            if (level_index >= LEVEL_COUNT)
                break;
            if (!level_set->is_playable(level_index))
                break;
            SDL_Rect src_rect = {256, 80, 32, 32};
            if (level_index == current_level_index)
                src_rect = {288, 80, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale + panel_offset.x, pos.y * 32 * scale + panel_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            
            
            XYPos level_pos = level_set->levels[level_index]->getimage_fg(DIRECTION_N);
            src_rect = {level_pos.x, level_pos.y, 24, 24};
            dst_rect = {(pos.x * 32 + 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            
            unsigned score = pressure_as_percent(level_set->levels[level_index]->best_score);

            render_number_2digit(XYPos((pos.x * 32 + 32 - 9 - 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), score);
            level_index++;
        }

        {
            SDL_Rect src_rect = {show_hint * 256, int(current_level_index) * 128, 256, 128};
            SDL_Rect dst_rect = {panel_offset.x, panel_offset.y + 176 * scale, 256 * scale, 128 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_levels_texture, &src_rect, &dst_rect);
        }




    } else if (panel_state == PANEL_STATE_EDITOR)
    {
        pos = XYPos(0,0);
        for (int i = 0; i < 4; i++)
        {
            SDL_Rect src_rect = {256, 80, 32, 32};
            SDL_Rect dst_rect = {panel_offset.x + i * 32 * scale, panel_offset.y, 32 * scale, 32 * scale};
            if (i == 0 && mouse_state == MOUSE_STATE_PLACING_VALVE)
                src_rect.x = 256 + 32;
            if (i == 1 && mouse_state == MOUSE_STATE_PLACING_SOURCE)
                src_rect.x = 256 + 32;
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        for (int i = 0; i < 2; i++)
        {
            SDL_Rect src_rect = {544 + direction * 24, 160 + i * 24, 24, 24};
            SDL_Rect dst_rect = {(i * 32 + 4) * scale + panel_offset.x, (4) * scale + panel_offset.y, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {
            SDL_Rect src_rect = {640-64, 208, 64, 32};
            SDL_Rect dst_rect = {64 * scale + panel_offset.x, panel_offset.y, 64 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        unsigned level_index = 0;

        for (pos.y = 0; pos.y < 8; pos.y++)
        for (pos.x = 0; pos.x < 8; pos.x++)
        {
            if (level_index >= LEVEL_COUNT)
                break;
            if (!level_set->is_playable(level_index))
                break;
            SDL_Rect src_rect = {256, 80, 32, 32};
            if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT && level_index == placing_subcircuit_level)
                src_rect.x = 256 + 32;

            SDL_Rect dst_rect = {pos.x * 32 * scale + panel_offset.x, (32 + 8 + pos.y * 32) * scale + panel_offset.y, 32 * scale, 32 * scale};
            if (level_index != current_level_index && !level_set->levels[level_index]->circuit->contains_subcircuit_level(current_level_index, level_set))
                 SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            XYPos level_pos = level_set->levels[level_index]->getimage_fg(direction);
            src_rect = {level_pos.x, level_pos.y, 24, 24};
            dst_rect = {(pos.x * 32 + 4) * scale + panel_offset.x, (32 + 8 + pos.y * 32 + 4) * scale + panel_offset.y, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            level_index++;
        }


        XYPos panel_pos = ((mouse - panel_offset) / scale);                 // Tooltip
        if (panel_pos.y >= 0 && panel_pos.x >= 0)
        {
            XYPos panel_grid_pos = panel_pos / 32;
            if (panel_grid_pos.y == 0 &&  panel_grid_pos.x < 4)
            {
                SDL_Rect src_rect = {496, 124 + panel_grid_pos.x * 12, 28, 12};
                SDL_Rect dst_rect = {(panel_pos.x - 28)* scale + panel_offset.x, panel_pos.y * scale + panel_offset.y, 28 * scale, 12 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            
        }
//     } else if (panel_state == PANEL_STATE_MONITOR && 0)
//     {
//         int mon_offset = 0;
//         for (unsigned mon_index = 0; mon_index < 4; mon_index++)
//         {
//             int color_table = 256 + mon_index * 48;
//             unsigned graph_size;
//             if (mon_index == selected_monitor)
//             {
//                 render_box(XYPos(panel_offset.x, mon_offset * scale + panel_offset.y), XYPos(7*32, 112), mon_index);
// 
//                 {
//                     SDL_Rect src_rect = {503, 86, 1, 1};
//                     SDL_Rect dst_rect = {5 * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 200 * scale, 101 * scale};
//                     SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
// 
//                 }
//                 for (int x = 0; x < 10; x++)
//                 {
//                     SDL_Rect src_rect = {524, 80, 13, 101};
//                     SDL_Rect dst_rect = {(x * 20 + 6) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 13 * scale, 101 * scale};
//                     if (x != 0)
//                     {
//                         src_rect.w = 5;
//                         dst_rect.w = 5 * scale;
//                     }
//                     SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                 }
//             }
//             else
//             {
//                 render_box(XYPos(panel_offset.x, mon_offset * scale + panel_offset.y), XYPos(7*32, 48), mon_index);
//                 {
//                     SDL_Rect src_rect = {503, 86, 1, 1};
//                     SDL_Rect dst_rect = {5 * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 200 * scale, 35 * scale};
//                     SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
// 
//                 }
//                 for (int x = 0; x < 10; x++)
//                 {
//                     SDL_Rect src_rect = {504, 80, 13, 35};
//                     SDL_Rect dst_rect = {(x * 20 + 6) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 13 * scale, 35 * scale};
//                     if (x != 0)
//                     {
//                         src_rect.w = 5;
//                         dst_rect.w = 5 * scale;
//                     }
//                     SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                 }
//             }
//                 {
//                     SDL_Rect src_rect;
//                     switch (mon_index)
//                     {
//                         case 0:
//                             src_rect = {256+80, 0+32, 16, 16}; break;
//                         case 1:
//                             src_rect = {352+80, 0+32, 16, 16}; break;
//                         case 2:
//                             src_rect = {448+80, 0+32, 16, 16}; break;
//                         case 3:
//                             src_rect = {544+80, 0+32, 16, 16}; break;
//                         default:
//                             assert (0);
//                     }
//                         
//                     SDL_Rect dst_rect = {(200 + 5) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 16 * scale, 16 * scale};
//                     SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                 
//                 }
//             if (current_level->sim_point_count)
//             {
//                 unsigned width = 200 / current_level->sim_point_count;
//                 assert (width);
//                 for (int x = 0; x < current_level->sim_point_count; x++)
//                 {
//                     bool current_point = (x == current_level->sim_point_index);
// 
//                     IOValue& io_val = current_level->sim_points[x].get(mon_index);
//                     if (io_val.in_value < 0)
//                         continue;
// 
//                     int rec_val = io_val.recorded;
//                     if (current_point)
//                     {
//                         switch (mon_index)
//                         {
//                         case 0:
//                             rec_val = pressure_as_percent(current_level->N.value);
//                             break;
//                         case 1:
//                             rec_val = pressure_as_percent(current_level->E.value);
//                             break;
//                         case 2:
//                             rec_val = pressure_as_percent(current_level->S.value);
//                             break;
//                         case 3:
//                             rec_val = pressure_as_percent(current_level->W.value);
//                             break;
//                         }
//                     }
// 
//                     if (!io_val.observed && !current_point)
//                     {
//                         SDL_Rect src_rect = {503, 81, 1, 1};
//                         int offset = 100 - io_val.in_value;
//                         if (mon_index != selected_monitor)
//                             offset = (offset + 2) / 3;
// 
//                         SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + offset) * scale + panel_offset.y, int(width) * scale, 1 * scale};
//                         SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                     }
//                     else
//                     {
//                         int offset = 100 - rec_val;
//                         if (mon_index != selected_monitor)
//                             offset = (offset + 2) / 3;
// 
//                         {
//                             SDL_Rect src_rect = {503, 81, 1, 1};
//                             int t_offset = 100 - io_val.in_value;
//                             if (mon_index != selected_monitor)
//                                 t_offset = (t_offset + 2) / 3;
// 
//                             int size = abs(offset - t_offset) + 1;
//                             int top = offset < t_offset ? offset : t_offset;
// 
//                             SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + top) * scale + panel_offset.y, int(width) * scale, size * scale};
//                             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                         }
//                         if (!current_point || (frame_index % 20 < 10))
//                         {
//                             SDL_Rect src_rect = {503, 80, 1, 1};
//                             SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + offset) * scale + panel_offset.y, int(width) * scale, 1 * scale};
//                             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//                         }
//                     }
//                 }
//             }
//             
//             if (mon_index == selected_monitor)
//                 mon_offset += 112 + 4;
//             else
//                 mon_offset += 48 + 4;
//         }
    } else if (panel_state == PANEL_STATE_TEST)
    {
        for (int port_index = 0; port_index < 4; port_index++)
        {
            render_box(XYPos((port_index * (48)) * scale + panel_offset.x, panel_offset.y), XYPos(48, 128 + 32), port_index);
            {
                SDL_Rect src_rect = {524, 80, 13, 101};
                SDL_Rect dst_rect = {(port_index * 48 + 8 + 13) * scale + panel_offset.x, (8) * scale + panel_offset.y, 13 * scale, 101 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }

            {
                SDL_Rect src_rect = {448, 80, 48, 16};
                SDL_Rect dst_rect = {(port_index * 48) * scale + panel_offset.x, (101 + 14) * scale + panel_offset.y, 48 * scale, 16 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            
            {
                SDL_Rect src_rect = {256 + 80 + (port_index * 6 * 16) , 16, 16, 16};
                SDL_Rect dst_rect = {(port_index * 48 + 8) * scale + panel_offset.x, (101 - int(test_value[port_index])) * scale + panel_offset.y, 16 * scale, 16 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            render_number_2digit(XYPos((port_index * 48 + 8 + 3 ) * scale + panel_offset.x, ((101 - test_value[port_index]) + 5) * scale + panel_offset.y), test_value[port_index]);
            
            {
                SDL_Rect src_rect = {256 + 80 + (port_index * 6 * 16) , 16, 16, 16};
                SDL_Rect dst_rect = {(port_index * 48 + int(test_drive[port_index])) * scale + panel_offset.x, (101 + 16 + 7) * scale + panel_offset.y, 16 * scale, 16 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            //render_number_2digit(XYPos((port_index * 48 + test_drive[port_index] + 3) * scale + panel_offset.x, (101 + 16 + 7 + 5) * scale + panel_offset.y), test_drive[port_index]*3);
            
            render_number_pressure(XYPos((port_index * 48 + 8 + 6 ) * scale + panel_offset.x, (101 + 16 + 20 + 5) * scale + panel_offset.y), test_pressures[port_index].value);

            
        }
        render_box(XYPos(panel_offset.x, (128 + 32 + 8) * scale + panel_offset.y), XYPos(32*7, 128), 5);
        {
            XYPos graph_pos(8 * scale + panel_offset.x, (128 + 32 + 8 + 8) * scale + panel_offset.y);
            {
                SDL_Rect src_rect = {524, 80, 13, 101};
                SDL_Rect dst_rect = {0 + graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            for (int i = 0; i < 192-1; i++)
            {
                PressureRecord* rec1 = &test_pressure_histroy[(test_pressure_histroy_index + i) % 192];
                PressureRecord* rec2 = &test_pressure_histroy[(test_pressure_histroy_index + i + 1) % 192];
                if (rec1->set && rec2->set)
                    for (int port = 0; port < 4; port++)
                    {
                        int myport = ((test_pressure_histroy_index + i) % 192 + port) % 4;
                        int v1 = rec1->values[myport];
                        int v2 = rec2->values[myport];
                        int top = 100 - std::max(v1, v2);
                        int size = abs(v1 - v2) + 1;

                        SDL_Rect src_rect = {502, 80 + myport, 1, 1};
                        SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                    }
            }
            
        }
    } else if (panel_state == PANEL_STATE_MONITOR)
    {
        unsigned test_index = current_level->test_index;
        unsigned test_count = current_level->tests.size();
        pos = XYPos(0,0);
        for (int i = 0; i < 3; i++)                         // Play/ pause
        {
            render_box(XYPos(panel_offset.x + i * 32 * scale, panel_offset.y), XYPos(32, 32), monitor_state == i ? 1 : 0);
            SDL_Rect src_rect = {448 + i * 24, 176, 24, 24};
            SDL_Rect dst_rect = {panel_offset.x + (i * 32 + 4) * scale, panel_offset.y + 4 * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        
        {
            SDL_Rect src_rect = {336, 32, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + (0) * scale, panel_offset.y + (32 + 8 + 16) * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        for (int i = 0; i < test_count; i++)
        {
            SDL_Rect src_rect = {272, 16, 16, 16};
            if (i == test_index)
                src_rect.x = 368;
            else if (i < test_index && monitor_state == MONITOR_STATE_PLAY_ALL && !current_level->touched)
                src_rect.x = 368;
            SDL_Rect dst_rect = {panel_offset.x + (16 + i * 16) * scale, panel_offset.y + (32 + 8) * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            render_number_2digit(XYPos(panel_offset.x + (16 + i * 16 + 3) * scale, panel_offset.y + (32 + 8 + 5) * scale), pressure_as_percent(current_level->tests[i].last_score));
            render_number_2digit(XYPos(panel_offset.x + (16 + i * 16 + 3) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), pressure_as_percent(current_level->tests[i].best_score));
        }
        
        render_number_pressure(XYPos(panel_offset.x + (16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 5) * scale), current_level->last_score);
        render_number_pressure(XYPos(panel_offset.x + (16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), current_level->best_score);

        
        unsigned sim_point_count = current_level->tests[test_index].sim_points.size();
        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8) * scale), XYPos(8*32, 112), 4);
        int y_pos = panel_offset.y + (32 + 32 + 16) * scale;
        
        {
            SDL_Rect src_rect = {448, 144, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            y_pos+= 16 * scale;
        }
        for (int i = 0; i < 4; i++)                 // Inputs
        {
            int pin_index = current_level->pin_order[i];
            if (((current_level->connection_mask >> pin_index) & 1) && pin_index != current_level->tests[test_index].tested_direction)
            {
            
                SDL_Rect src_rect = {256 + pin_index * 16, 144, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                for (int i2 = 0; i2 < sim_point_count; i2++)
                {
                    unsigned value = current_level->tests[test_index].sim_points[i2].values[pin_index];
                    render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + i2 * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == i2 ? 4 : 0);
                }
                y_pos += 16 * scale;
            }
        }

        {
            SDL_Rect src_rect = {448, 160, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            y_pos+= 16 * scale;
        }

        {
            int pin_index = current_level->tests[test_index].tested_direction;
            SDL_Rect src_rect = {256 + pin_index * 16, 144, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            unsigned value = current_level->tests[test_index].sim_points[sim_point_count-1].values[pin_index];
            render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + (sim_point_count-1) * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == sim_point_count - 1 ? 4 : 0);

            render_number_pressure(XYPos(panel_offset.x + (8 + 16 + 3 + 16 + (sim_point_count-1) * 16) * scale, y_pos + (5) * scale), current_level->tests[test_index].last_pressure_log[HISTORY_POINT_COUNT - 1] , 1, 9, 1);

            y_pos += 16 * scale;
        }
        {
            SDL_Rect src_rect = {320, 144, 16, 8};
            SDL_Rect dst_rect = {panel_offset.x + int(8 + 16 + current_level->sim_point_index * 16) * scale, panel_offset.y + (32 + 32 + 16) * scale, 16 * scale, 8 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            src_rect.y += 8;
            src_rect.h = 1;
            dst_rect.y += 8 * scale;
            dst_rect.h = y_pos - dst_rect.y - 8 * scale;
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            src_rect.h = 8;
            dst_rect.y = y_pos - 8 * scale;
            dst_rect.h = 8 * scale;
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }


        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(256, 120), 5);
        {
            XYPos graph_pos(8 * scale + panel_offset.x, (32 + 32 + 8 + 112 + 9) * scale + panel_offset.y);
            {
                SDL_Rect src_rect = {524, 80, 13, 101};
                SDL_Rect dst_rect = {0 + graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
                SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }

            {
                for (int i = 0; i < HISTORY_POINT_COUNT-1; i++)
                {
                    int v1 = pressure_as_percent(current_level->tests[test_index].best_pressure_log[i]);
                    int v2 = pressure_as_percent(current_level->tests[test_index].best_pressure_log[i + 1]);
                    int top = 100 - std::max(v1, v2);
                    int size = abs(v1 - v2) + 1;

                    SDL_Rect src_rect = {503, 81, 1, 1};
                    SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                }
                for (int i = 0; i < int(current_level->tests[test_index].last_pressure_index) - 1; i++)
                {
                    int v1 = pressure_as_percent(current_level->tests[test_index].last_pressure_log[i]);
                    int v2 = pressure_as_percent(current_level->tests[test_index].last_pressure_log[i + 1]);
                    int top = 100 - std::max(v1, v2);
                    int size = abs(v1 - v2) + 1;

                    SDL_Rect src_rect = {503, 86, 1, 1};
                    SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                }
            }

            
        }

    }

    if (show_debug)
    {
        render_number_2digit(XYPos(0, 0), debug_last_second_frames, 3);
        render_number_long(XYPos(0, 3 * 7 * scale), debug_last_second_simticks, 3);
    }
    if (level_win_animation)
    {
        int size = 360 - level_win_animation * 3;
        SDL_Rect src_rect = {336, 32, 16, 16};
        SDL_Rect dst_rect = {(320 - size / 2) * scale, (180 - size / 2) * scale, size * scale, size * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        level_win_animation--;
    }

    if (show_help)
    {
        render_box(XYPos(16 * scale, 0 * scale), XYPos(592, 360), 0);

        render_box(XYPos(16 * scale, 0 * scale), XYPos(592, (128+16)*2+20), 1);
        for (int i = 0; i < 10; i++)
        {
            if (i == show_help_page)
                render_box(XYPos((32 + 48 + i * 32) * scale, (2 * (128 + 16) + 0) * scale), XYPos(32, 32+28), 1);
            else
                render_box(XYPos((32 + 48 + i * 32) * scale, (2 * (128 + 16) + 28) * scale), XYPos(32, 32), 0);
            SDL_Rect src_rect = {352 + (i % 5) * 24, 240 + (i / 5) * 24, 24, 24};
            SDL_Rect dst_rect = {(32 + 48 + 4 + i * 32) * scale, (2 * (128 + 16) + 4 + 28) * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        {
            SDL_Rect src_rect = {464, 144, 16, 16};
            SDL_Rect dst_rect = {(32 + 512 + 32 + 8) * scale, (8) * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        for (int i = 0; i < 2; i++)
        {
            render_box(XYPos((16 + 8 + i * 48) * scale, (i * (128 +16) + 8) * scale), XYPos(128+16, 128+16), 4);
            render_box(XYPos((128 + 16 + 16 + 8 + i * 48) * scale, (i * (128 +16) + 8) * scale), XYPos(384, 128+16), 4);
            unsigned anim_frames = 5;
            const char* text="";
            
            struct HelpPage
            {
                XYPos src_pos;
                int frame_count;
                float fps;
                const char* text;
            }
            pages[] =
            {
                {XYPos(0,14), 4, 1, "In the level select menu, the bottom panel describes\nthe design requirements. Each design has four ports\nand the requirements state the expected output in\nterms of other ports. Each port has a colour\nidentifier."},
                {XYPos(0,13),5,0.5, "Once you achieve a score of 75 or more, the next\ndesign becomes available. You can always come back\nto refine your solution.\n\nPress the pipe button below to continue the\ntutorial. You can return to the help by pressing F1."},
                {XYPos(0,0), 10, 1, "Use left mouse button to place pipes. The pipe will\nextend in the direction of the mouse. Right click to\nexit pipe laying mode."},
                {XYPos(0,2),  5, 1, "Hold the right mouse button to delete pipes and\nother elements."},
                {XYPos(0,4), 15, 1, "The build menu allows you to add components into\nyour design. Select the steam inlet component and\nhover over your design. The arrow buttons change\nthe orientation. This can also be done using keys Q\nand E. Clicking the left mouse button will place the\ncomponent. Right click to exit steam inlet placing\nmode."},
                {XYPos(0,8),  5, 1, "Valves can be placed in the same way. Pressing Tab\nis a shortcut to enter valve placement mode.\nPressing Tab again switches to steam inlet placement."},
                {XYPos(0,7),  5,10, "A steam inlet will supply steam at pressure 100. Any\npipes with open ends will vent the steam to the\natmosphere at pressure 0."},
                {XYPos(1,10), 4,10, "Pressure at different points is visible on pipe\nconnections. Note how each pope has a little resistance. "},
                {XYPos(0,9),  5,10, "Valves allow steam to pass through them if the\n(+) side of the valve is at a higher pressure than\nthe (-) side. The higher it is, the more the valve\nis open. Steam on the (+) and (-) sides is not\nconsumed. Here, the (-) side is vented to atmosphere\nand thus at 0 pressure."},
                {XYPos(0,10), 1, 1, "If the pressure on the (+) side is equal or lower\nthan the (-) side, the valve becomes closed and no\nsteam will pass through."},
                {XYPos(0,11), 5,10, "By pressurising (+) side with a steam inlet, the\nvalve will become open only if the pressure on the\n(-) side is low."},
                {XYPos(0,12), 1, 1, "Applying high pressure to the (-) side will close\nthe valve."},
                {XYPos(1,12), 1, 1, "The test menu allows you to inspect how well your\ndesign is performing. The first three buttons\npause the testing, repeatedly run a single test and\nrun all tests respectively.\n\n"
                                    "The current scores for individual tests are shown\nwith the scores of best design seen below. On the\nright is the final score formed from the average of\nall tests.\n\n"
                                    "The next panel shows the sequence of inputs and\nexpected outputs for the current test. The current\nphase is highlighted. The output recorded on the\nlast run is show to the right."},
                {XYPos(2,12), 3, 1, "The score is based on how close the output is to the\ntarget value. The graph shows the output value\nduring the final stage of the test. The faded line\nin the graph shows the path of the best design so\nfar."},
                {XYPos(4,14), 1, 1, "The experiment menu allows you to manually set the\nports and examine your design's operation. The\nvertical sliders set the desired value. The\nhorizontal sliders below set force of the input.\nSetting the slider all the way left makes it an\noutput. Initial values are set from the current\ntest."},
                {XYPos(0,15), 1, 1, "The graph at the bottom shows the history of the\nport values."},
                
                {XYPos(0,0),  0, 1, ""},
                {XYPos(0,0),  0, 1, ""},
                
                {XYPos(0,0),  0, 1, ""},
                {XYPos(0,0),  0, 1, ""},
            };
            

            HelpPage* page = &pages[show_help_page * 2 + i];

            if (page->frame_count == 0)
                continue;
            int frame = (SDL_GetTicks() * page->fps / 1000);
            frame %= page->frame_count;
            frame += page->src_pos.x + page->src_pos.y * 5;
            int x = frame % 5;
            int y = frame / 5;


            SDL_Rect src_rect = {x * 128, (y * 128), 128, 128};
            SDL_Rect dst_rect = {(32 + i * 48) * scale, (i * (128 +16) + 16) * scale, 128 * scale, 128 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
            
            render_text(XYPos(32 + 128 + 16 + i * 48, i * (128 +16) + 16), page->text);
        }


    }


//         unsigned frame = (SDL_GetTicks() / 1000);
//         int x = frame % 5;
//         int y = frame / 5;
// 
//         render_box(XYPos((16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         
//         SDL_Rect src_rect = {x * 128, 128 * (y % 2), 128, 128};
//         SDL_Rect dst_rect = {32 * scale, 32 * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
//         
//         render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (2 * 128), 128, 128};
//         dst_rect = {((128 +16) * 1 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
//         
//         render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (3 * 128), 128, 128};
//         dst_rect = {((128 +16) * 2 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (4 * 128)  + 128 * (y % 3), 128, 128};
//         dst_rect = {((128 +16) * 3 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
//         
//         render_box(XYPos((0 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (7 * 128), 128, 128};
//         dst_rect = {((128 +16) * 0 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (8 * 128), 128, 128};
//         dst_rect = {((128 +16) * 1 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (9 * 128), 128, 128};
//         dst_rect = {((128 +16) * 2 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (10 * 128), 128, 128};
//         dst_rect = {((128 +16) * 3 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
//     }
// 
//     if (show_help == 2)
//     {
//         render_box(XYPos(16 * scale, 16 * scale), XYPos(592, 328), 0);
//         unsigned frame = (SDL_GetTicks() / 1000);
//         int x = frame % 5;
//         int y = frame / 5;
// 
//         render_box(XYPos((0 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         SDL_Rect src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (11 * 128), 128, 128};
//         SDL_Rect dst_rect = {((128 +16) * 0 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (12 * 128), 128, 128};
//         dst_rect = {((128 +16) * 1 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (13 * 128), 128, 128};
//         dst_rect = {((128 +16) * 2 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//         render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
//         src_rect = {x * 128, (14 * 128), 128, 128};
//         dst_rect = {((128 +16) * 3 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
//         SDL_RenderCopy(sdl_renderer, sdl_tutorial_texture, &src_rect, &dst_rect);
// 
//     }
    
    if (show_main_menu)
    {
        render_box(XYPos(160 * scale, 90 * scale), XYPos(320, 180), 0);
        render_box(XYPos((160 + 32) * scale, (90 + 32)  * scale), XYPos(32, 32), 0);
        {
            SDL_Rect src_rect = {448, 200, 24, 24};
            SDL_Rect dst_rect = {(160 + 32 + 4) * scale, (90 + 32 + 4)  * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        }
        render_box(XYPos((160 + 32 + 64) * scale, (90 + 32)  * scale), XYPos(32, 32), 0);
        {
            SDL_Rect src_rect = {472, 200, 24, 24};
            SDL_Rect dst_rect = {(160 + 32 + 64 + 4) * scale, (90 + 32 + 4)  * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        render_box(XYPos((160 + 32 + 128) * scale, (90 + 32)  * scale), XYPos(32, 128), 1);
        {
            SDL_Rect src_rect = {496, 200, 24, 24};
            SDL_Rect dst_rect = {(160 + 32 + 128 + 4) * scale, (90 + 4)  * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        {
            SDL_Rect src_rect = {526, 80, 12, 101};
            SDL_Rect dst_rect = {(160 + 32 + 128 + 16) * scale, (90 + 32 + 6 + 6)  * scale, 12 * scale, 101 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        {
            SDL_Rect src_rect = {256 + 80 + 96, 16, 16, 16};
            SDL_Rect dst_rect = {(160 + 32 + 128 + 4) * scale, (90 + 32 + 6 + int(100 - sound_volume))  * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        render_box(XYPos((160 + 32 + 192) * scale, (90 + 32)  * scale), XYPos(32, 128), 2);
        {
            SDL_Rect src_rect = {520, 200, 24, 24};
            SDL_Rect dst_rect = {(160 + 32 + 192 + 4) * scale, (90 + 4)  * scale, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {
            SDL_Rect src_rect = {526, 80, 12, 101};
            SDL_Rect dst_rect = {(160 + 32 + 192 + 16) * scale, (90 + 32 + 6 + 6)  * scale, 12 * scale, 101 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }

        {
            SDL_Rect src_rect = {256 + 80 + 192, 16, 16, 16};
            SDL_Rect dst_rect = {(160 + 32 + 192 + 4) * scale, (90 + 32 + 6 + int(100 - music_volume))  * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }


    }
    SDL_RenderPresent(sdl_renderer);
}

 


void GameState::set_level(unsigned level_index)
{
    if (level_index < LEVEL_COUNT)
    {
        if (current_circuit)
            current_circuit->retire();
        current_level_index = level_index;
        current_level = level_set->levels[current_level_index];
        current_circuit = current_level->circuit;
        current_level->reset(level_set);
        show_hint = false;
    }
}

void GameState::mouse_click_in_grid()
{
    XYPos pos = (mouse - grid_offset) / scale;
    XYPos grid = pos / 32;
    if (keyboard_shift)
    {
        select_area_pos = pos;
        mouse_state = MOUSE_STATE_AREA_SELECT;
        return;
    }
    if (keyboard_ctrl)
    {
        if (pos.x < 0 || pos.y < 0)
            return;
        if (grid.x > 8 || grid.y > 8)
            return;
        if (current_circuit->elements[grid.y][grid.x]->is_empty())
            return;
        if (selected_elements.find(grid) == selected_elements.end())
            selected_elements.insert(grid);
        else
            selected_elements.erase(grid);
        
        return;
    }
    if (!selected_elements.empty())
    {
        selected_elements.clear();
        return;
    }
    if (mouse_state == MOUSE_STATE_NONE)
    {
        if (grid.x > 8)
            grid.x = 8;
        if (grid.y > 8)
            grid.y = 8;
        mouse_state = MOUSE_STATE_PIPE;
        XYPos pos = (mouse - grid_offset) / scale;
        pos -= grid * 32;
        pipe_start_grid_pos = grid;
        if (pos.x > pos.y)      // upper right
        {
            if ((31 - pos.x) > pos.y)
            {
                pipe_start_ns = true; //up
            }
            else
            {
                pipe_start_grid_pos.x++; //right
                pipe_start_ns = false;
            }
        }
        else                    // lower left
        {
            if ((31 - pos.x) > pos.y)
            {
                pipe_start_ns = false; //left
            }
            else
            {
                pipe_start_grid_pos.y++; //down
                pipe_start_ns = true;
            }
        }
    }
    else if (mouse_state == MOUSE_STATE_PIPE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
        if (pipe_start_ns)
        {
            mouse_rel.x -= 16;
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_level->blocked[pipe_start_grid_pos.y - 1][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y - 1;
            if (mouse_rel.y < 0)    //south - northwards
            {
                pipe_start_grid_pos.y--;
                Connections con;
                if (-mouse_rel.y > abs(mouse_rel.x) + 16)    // north
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NS);
                }
                else if (mouse_rel.x < 0)               // west
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_WS);
                    pipe_start_ns = false;
                }
                else                                    // east
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_ES);
                    pipe_start_ns = false;
                    pipe_start_grid_pos.x++;
                }

            }
            else
            {
                Connections con;
                if (mouse_rel.y > abs(mouse_rel.x) + 16)    // north
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NS);
                    pipe_start_grid_pos.y++;

                }
                else if (mouse_rel.x < 0)               // west
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NW);
                    pipe_start_ns = false;
                }
                else                                    // east
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NE);
                    pipe_start_ns = false;
                    pipe_start_grid_pos.x++;
                }

            }

        }
        else
        {
            mouse_rel.y -= 16;
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x - 1]))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_level->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
                mouse_rel.x = -mouse_rel.x - 1;
            if (mouse_rel.x < 0)    //east - westwards
            {
                pipe_start_grid_pos.x--;
                Connections con;
                if (-mouse_rel.x > abs(mouse_rel.y) + 16)    // west
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_EW);
                }
                else if (mouse_rel.y < 0)               // north
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NE);
                    pipe_start_ns = true;
                }
                else                                    // south
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_ES);
                    pipe_start_ns = true;
                    pipe_start_grid_pos.y++;
                }

            }
            else                    //west - eastwards
            {
                Connections con;
                if (mouse_rel.x > abs(mouse_rel.y) + 16)    // west
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_EW);
                    pipe_start_grid_pos.x++;

                }
                else if (mouse_rel.y < 0)               // north
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_NW);
                    pipe_start_ns = true;
                }
                else                                    // south
                {
                    current_circuit->set_element_pipe(pipe_start_grid_pos, CONNECTIONS_WS);
                    pipe_start_ns = true;
                    pipe_start_grid_pos.y++;
                }
            }
        }
        current_level->touched = true;
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            current_circuit->set_element_valve(mouse_grid, direction);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            current_circuit->set_element_source(mouse_grid, direction);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_level->blocked[mouse_grid.y][mouse_grid.x])
        {
            XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
            current_circuit->set_element_subcircuit(mouse_grid, direction, placing_subcircuit_level, level_set);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_DELETING)
    {
        current_circuit->undo(level_set);
    }
    else
    {
        assert(0);
    }
}

void GameState::mouse_click_in_panel()
{
    {
        XYPos menu_icons_pos = (mouse / scale - XYPos((8 + 32 * 11), (8)));
        if (menu_icons_pos.y < 0)
            return;
        XYPos menu_icons_grid_pos = menu_icons_pos / 32;
        if (menu_icons_grid_pos.y == 0)
        {
            unsigned sel = menu_icons_grid_pos.x;
            switch (sel)
            {
                case 0:
                    panel_state = PANEL_STATE_LEVEL_SELECT;
                    break;
                case 1:
                    if (!level_set->is_playable(1))
                        break;
                    panel_state = PANEL_STATE_EDITOR;
                    break;
                case 2:
                    if (!level_set->is_playable(3))
                        break;
                    panel_state = PANEL_STATE_MONITOR;
                    break;
                case 3:
                    if (!level_set->is_playable(5))
                        break;
                    panel_state = PANEL_STATE_TEST;
                    break;
                case 5:
                case 6:
                {
                    mouse_state = MOUSE_STATE_SPEED_SLIDER;
                    slider_direction = DIRECTION_E;
                    slider_max = 49;
                    slider_pos = panel_offset.x + (5 * 32 + 8) * scale;
                    slider_value_tgt = &game_speed;
                    mouse_motion();
                    break;
                }
                case 7:
                {
                    show_help = true;
                    break;
                }
            }
            return;
        }
    }

    XYPos panel_pos = ((mouse - panel_offset) / scale);
    if (panel_pos.y < 0 || panel_pos.x < 0)
        return;

    if (panel_state == PANEL_STATE_LEVEL_SELECT)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        unsigned level_index = panel_grid_pos.x + panel_grid_pos.y * 8;
        if (level_index < LEVEL_COUNT && level_set->is_playable(level_index))
        {
            set_level(level_index);
            mouse_state = MOUSE_STATE_NONE;
        }
        else if (panel_pos.y > 176)
        {
            show_hint = !show_hint;
        }
        return;
    } else if (panel_state == PANEL_STATE_EDITOR)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        if (panel_grid_pos.y == 0)
        {
            if (panel_grid_pos.x == 0)
                mouse_state = MOUSE_STATE_PLACING_VALVE;
            else if (panel_grid_pos.x == 1)
                mouse_state = MOUSE_STATE_PLACING_SOURCE;
            else if (panel_grid_pos.x == 2)
                direction = Direction((int(direction) + 4 - 1) % 4);
            else if (panel_grid_pos.x == 3)
                direction = Direction((int(direction) + 1) % 4);
            return;
        }
        panel_grid_pos = (panel_pos - XYPos(0, 32 + 8)) / 32;
        unsigned level_index = panel_grid_pos.x + panel_grid_pos.y * 8;

        if (level_index < LEVEL_COUNT && level_index != current_level_index && !level_set->levels[level_index]->circuit->contains_subcircuit_level(current_level_index, level_set)
                   && level_set->is_playable(level_index))
        {
            mouse_state = MOUSE_STATE_PLACING_SUBCIRCUIT;
            placing_subcircuit_level = level_index;
        }
        return;
    } else if (panel_state == PANEL_STATE_MONITOR)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        if (panel_grid_pos.y == 0)
        {
            if (panel_grid_pos.x == 0)
            {
                monitor_state = MONITOR_STATE_PAUSE;
                current_circuit->updated_ports();
                current_level->touched = true;

            }
            else if (panel_grid_pos.x == 1)
            {
                monitor_state = MONITOR_STATE_PLAY_1;
                current_circuit->updated_ports();
                current_level->touched = true;

            }
            else if (panel_grid_pos.x == 2)
            {
                monitor_state = MONITOR_STATE_PLAY_ALL;
                current_circuit->updated_ports();
                current_level->touched = true;
                current_level->reset(level_set);
            }
            return;
        }

        panel_grid_pos = (panel_pos - XYPos(0, 8)) / 16;
        if (panel_grid_pos.y == 2)
        {
            unsigned test_count = current_level->tests.size();
            unsigned t = panel_grid_pos.x - 1;
            if (t < test_count)
            {
                current_level->select_test(t);
                monitor_state = MONITOR_STATE_PLAY_1;
                current_circuit->updated_ports();
                current_level->touched = true;
            }
                

        }
//         int mon_offset = 0;
//         for (unsigned mon_index = 0; mon_index < 4; mon_index++)
//         {
//             XYPos button_offset = panel_pos - XYPos(200 + 6, mon_offset + 6);
//             if (button_offset.inside(XYPos(16, 16)))
//             {
//                 selected_monitor = mon_index;
//             }
//             
//             if (mon_index == selected_monitor)
//                 mon_offset += 112 + 4;
//             else
//                 mon_offset += 48 + 4;
//         }
    } else if (panel_state == PANEL_STATE_TEST)
    {
        int port_index = panel_pos.x / 48;
        if (port_index > 4)
            return;
        if (panel_pos.y <= (101 + 16))
        {
            monitor_state = MONITOR_STATE_PAUSE;
            current_circuit->updated_ports();
            current_level->touched = true;

            mouse_state = MOUSE_STATE_SPEED_SLIDER;
            slider_direction = DIRECTION_N;
            slider_max = 100;
            slider_pos = panel_offset.y + (101 + 8) * scale;
            slider_value_tgt = &test_value[port_index];
            mouse_motion();
            return;
        }
        else if (panel_pos.y <= (101 + 16 + 7 + 16))
        {
            monitor_state = MONITOR_STATE_PAUSE;
            current_circuit->updated_ports();
            current_level->touched = true;

            mouse_state = MOUSE_STATE_SPEED_SLIDER;
            slider_direction = DIRECTION_E;
            slider_max = 33;
            slider_pos = panel_offset.x + (port_index * 48 + 8) * scale;
            slider_value_tgt = &test_drive[port_index];
            mouse_motion();
            return;
        }

            
        return;
    }
}

void GameState::mouse_motion()
{
    if (mouse_state == MOUSE_STATE_DELETING)
    {
        if (!((mouse - grid_offset) / scale).inside(XYPos(9*32,9*32)))
            return;
        XYPos grid = ((mouse - grid_offset) / scale) / 32;
        if (grid.x >= 9 || grid.y >= 9)
            return;
        current_circuit->set_element_empty(grid);
        current_level->touched = true;

    }
    if (mouse_state == MOUSE_STATE_SPEED_SLIDER)
    {
        XYPos menu_icons_pos = (mouse / scale - XYPos((16 + 32 * 11), (16)));
        int vol;

        switch (slider_direction)
        {
        case DIRECTION_N:
            vol = -(mouse.y - int(slider_pos)) / scale;
            break;
        case DIRECTION_E:
            vol = (mouse.x - int(slider_pos)) / scale;
            break;
        case DIRECTION_S:
            vol = (mouse.y - int(slider_pos)) / scale;
            break;
        case DIRECTION_W:
            vol = -(mouse.x - int(slider_pos)) / scale;
            break;
        default:
            assert(0);
        
        }

        if (vol < 0)
            vol = 0;
        if (vol > slider_max)
            vol = slider_max;
        *slider_value_tgt = vol;
    }
}


bool GameState::events()
{
    SDL_Event e;
    while(SDL_PollEvent(&e))
    {
	    switch (e.type)
        {
            case SDL_QUIT:
		        return true;
            case SDL_KEYDOWN:
            {
                switch (e.key.keysym.scancode)
                {
                    case SDL_SCANCODE_ESCAPE:
                        show_main_menu = !show_main_menu;
                        break;
                    case SDL_SCANCODE_TAB:
                    {
                        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                            mouse_state = MOUSE_STATE_PLACING_SOURCE;
                        else
                            mouse_state = MOUSE_STATE_PLACING_VALVE;
                        break;
                    }
                    case SDL_SCANCODE_Q:
                        direction = Direction((int(direction) + 4 - 1) % 4);
                        break;
                    case SDL_SCANCODE_E:
                        direction = Direction((int(direction) + 1) % 4);
                        break;
                    case SDL_SCANCODE_W:
                        current_circuit->move_selected_elements(selected_elements, DIRECTION_N);
                        break;
                    case SDL_SCANCODE_A:
                        current_circuit->move_selected_elements(selected_elements, DIRECTION_W);
                        break;
                    case SDL_SCANCODE_S:
                        current_circuit->move_selected_elements(selected_elements, DIRECTION_S);
                        break;
                    case SDL_SCANCODE_D:
                        current_circuit->move_selected_elements(selected_elements, DIRECTION_E);
                        break;
                    case SDL_SCANCODE_X:
                    case SDL_SCANCODE_DELETE:
                        current_circuit->delete_selected_elements(selected_elements);
                        selected_elements.clear();
                        break;
                    case SDL_SCANCODE_Z:
                        if (!keyboard_shift)
                            current_circuit->undo(level_set);
                        else
                            current_circuit->redo(level_set);
                        break;
                    case SDL_SCANCODE_Y:
                        current_circuit->redo(level_set);
                        break;
                    case SDL_SCANCODE_F1:
                        show_help = !show_help;
                        break;
                    case SDL_SCANCODE_F5:
                        show_debug = !show_debug;
                        break;
                    case SDL_SCANCODE_LSHIFT:
                        keyboard_shift = true;
                        break;
                    case SDL_SCANCODE_LCTRL:
                        keyboard_ctrl = true;
                        break;
                    default:
                        printf("Uncaught key: %d\n", e.key.keysym.scancode);
                        break;
                }
                break;
            }
            case SDL_KEYUP:
                    if (e.key.keysym.scancode == SDL_SCANCODE_LSHIFT)
                        keyboard_shift = false;
                    else if (e.key.keysym.scancode == SDL_SCANCODE_LCTRL)
                        keyboard_ctrl = false;
                break;
            case SDL_MOUSEMOTION:
            {
                mouse.x = e.motion.x;
                mouse.y = e.motion.y;
                mouse_motion();
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (mouse_state == MOUSE_STATE_SPEED_SLIDER)
                        mouse_state = MOUSE_STATE_NONE;
                    if (mouse_state == MOUSE_STATE_AREA_SELECT)
                    {
                        XYPos tl = ((mouse - grid_offset) / scale) / 32;
                        XYPos br = select_area_pos / 32;
                        if (tl.x > br.x)
                        {
                            int t = tl.x;
                            tl.x = br.x;
                            br.x = t;
                        }
                        if (tl.y > br.y)
                        {
                            int t = tl.y;
                            tl.y = br.y;
                            br.y = t;
                        }
                        XYPos pos;
                        for (pos.y = 0; pos.y < 9; pos.y++)
                        for (pos.x = 0; pos.x < 9; pos.x++)
                        {
                            if (pos.x >= tl.x && pos.x <= br.x && pos.y >= tl.y && pos.y <= br.y && !current_circuit->elements[pos.y][pos.x]->is_empty())
                                selected_elements.insert(pos);
                        }
                        mouse_state = MOUSE_STATE_NONE;
                    }
                }
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    if (mouse_state == MOUSE_STATE_DELETING)
                        mouse_state = MOUSE_STATE_NONE;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (show_main_menu)
                    {
                        XYPos pos = (mouse / scale) - XYPos((160 + 32), (90 + 32));
                        if (pos.inside(XYPos(32, 32)))
                            return true;
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 32)))
                        {
                            full_screen = !full_screen;
                            SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                            SDL_SetWindowBordered(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 128)))
                        {
                            mouse_state = MOUSE_STATE_SPEED_SLIDER;
                            slider_direction = DIRECTION_N;
                            slider_max = 100;
                            slider_pos = (90 + 32 + 100 + 8) * scale;
                            slider_value_tgt = &sound_volume;
                            mouse_motion();
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 128)))
                        {
                            mouse_state = MOUSE_STATE_SPEED_SLIDER;
                            slider_direction = DIRECTION_N;
                            slider_max = 100;
                            slider_pos = (90 + 32 + 100 + 8) * scale;
                            slider_value_tgt = &music_volume;
                            mouse_motion();
                        }
                        break;
                    }
                    else if (show_help)
                    {
                        XYPos pos = (mouse / scale) - XYPos(32 + 512 + 32 + 8, 8);
                        if (pos.inside(XYPos(16, 16)))
                        {
                            show_help = false;
                            break;
                        }
                        pos = (mouse / scale) - XYPos(32 + 48, 2 * (128 + 16) + 28);
                        if (pos.y < 32 && pos.y >=0)
                        {
                            int x = pos.x / 32;
                            if (x < 10)
                                show_help_page = x;
                            break;
                        }
                        show_help_page = (show_help_page + 1) % 10;
                        break;
                    }
                    else if (mouse.x < panel_offset.x)
                        mouse_click_in_grid();
                    else
                        mouse_click_in_panel();
                }
                else if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    selected_elements.clear();
                    if (mouse_state == MOUSE_STATE_NONE)
                    {
                        mouse_state = MOUSE_STATE_DELETING;
                        mouse_motion();
                    }
                    else
                        mouse_state = MOUSE_STATE_NONE;
                }
                else if (e.button.button == SDL_BUTTON_MIDDLE)
                {
                        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                            mouse_state = MOUSE_STATE_PLACING_SOURCE;
                        else
                            mouse_state = MOUSE_STATE_PLACING_VALVE;
                        break;
                }
                break;
            }
            case SDL_MOUSEWHEEL:
            {
                if(e.wheel.y > 0)
                    direction = Direction((int(direction) + 4 - 1) % 4);
                if(e.wheel.y < 0)
                    direction = Direction((int(direction) + 1) % 4);

            }
            {
            break;
            }
        }
    }

   return false;
}
