#include "GameState.h"
#include "SaveState.h"
#include "Misc.h"

#include <cassert>
#include <SDL.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <limits>
#include <math.h>

GameState::GameState(const char* filename)
{
    sdl_window = SDL_CreateWindow( "Pressure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920, 1080, 0);
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	sdl_texture = loadTexture();
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
	assert(sdl_texture);

    std::ifstream loadfile(filename);
    if (loadfile.fail() || loadfile.eof())
    {
        level_set = new LevelSet();
        current_level_index = 0;
    }
    else
    {
        SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();
        level_set = new LevelSet(omap->get_item("levels"));

        SaveObjectList* slist = omap->get_item("levels")->get_list();
        
        current_level_index = omap->get_num("current_level_index");
        if (current_level_index >= LEVEL_COUNT)
            current_level_index = 0;
        game_speed = omap->get_num("game_speed");
        show_debug = omap->get_num("show_debug");
        show_help = omap->get_num("show_help");
        delete omap;
    }
    set_level(current_level_index);
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
    omap.save(outfile);
}


GameState::~GameState()
{
	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);
}

extern const char embedded_data_binary_texture_png_start;
extern const char embedded_data_binary_texture_png_end;

SDL_Texture* GameState::loadTexture()
{
    SDL_Surface* loadedSurface = IMG_Load_RW(SDL_RWFromMem((void*)&embedded_data_binary_texture_png_start,
                                    &embedded_data_binary_texture_png_end - &embedded_data_binary_texture_png_start),1);
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
    if (panel_state != PANEL_STATE_TEST)
    {
        while (count)
        {
            int subcount = count < 100 ? count : 100;
            current_level->advance(subcount);
            count -= subcount;
            debug_simticks += subcount;
            if (SDL_TICKS_PASSED(SDL_GetTicks(), time + 100))
            {
                game_speed--;
                break;
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
                test_N.apply(test_value[0], test_drive[0] * 3);
                test_E.apply(test_value[1], test_drive[1] * 3);
                test_S.apply(test_value[2], test_drive[2] * 3);
                test_W.apply(test_value[3], test_drive[3] * 3);

                test_N.pre();
                test_E.pre();
                test_S.pre();
                test_W.pre();
                PressureAdjacent adj(test_N, test_E, test_S, test_W);

                current_circuit->sim_pre(adj);
                current_circuit->sim_post(adj);

                test_N.post();
                test_E.post();
                test_S.post();
                test_W.post();
                
                if (test_pressure_histroy_sample_downcounter == 0 )
                {
                    test_pressure_histroy_sample_downcounter = 100; // test_pressure_histroy_sample_frequency;

                    test_pressure_histroy[test_pressure_histroy_index].values[0] = pressure_as_percent(test_N.value);
                    test_pressure_histroy[test_pressure_histroy_index].values[1] = pressure_as_percent(test_E.value);
                    test_pressure_histroy[test_pressure_histroy_index].values[2] = pressure_as_percent(test_S.value);
                    test_pressure_histroy[test_pressure_histroy_index].values[3] = pressure_as_percent(test_W.value);
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

void GameState::render_number_2digit(XYPos pos, unsigned value, unsigned scale_mul, unsigned colour)
{
    int myscale = scale * scale_mul;
    {
        SDL_Rect src_rect = {503, 80 + int(colour), 1, 1};
        SDL_Rect dst_rect = {pos.x, pos.y, 9 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    if (value == 100)
    {
        SDL_Rect src_rect = {100, 161, 9, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 9 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    else
    {
        SDL_Rect src_rect = {60 + (int(value) / 10) * 4, 161, 4, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        src_rect.w = 5;
        src_rect.x = 60 + (value % 10) * 4;
        dst_rect.w = 5 * myscale;
        dst_rect.x += 4 * myscale;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

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
    
    SDL_Rect src_rect = {60, 161, 4, 5};
    SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
    while (i < 10)
    {
        src_rect.x = 60 + digits[i] * 4;
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

void GameState::render()
{
    SDL_RenderClear(sdl_renderer);
    XYPos window_size;
    SDL_GetWindowSize(sdl_window, &window_size.x, &window_size.y);
    XYPos pos;
    
    frame_index++;
    debug_frames++;
    
    {
        SDL_Rect src_rect = {320, 300, 320, 180};       // Background
        SDL_Rect dst_rect = {0, 0, 1920, 1080};
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
            
        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    
    for (pos.y = 0; pos.y < 9; pos.y++)
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
    }

    if (mouse_state == MOUSE_STATE_PIPE && (frame_index % 20 < 10))
    {
        Connections con;
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
        if (pipe_start_ns)
        {
            mouse_rel.x -= 16;
            if (mouse_rel.y < 0 && pipe_start_grid_pos.y == 0)
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && pipe_start_grid_pos.y == 9)
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
            if (mouse_rel.x < 0 && pipe_start_grid_pos.x == 0)
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && pipe_start_grid_pos.x == 9)
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
        if (mouse_grid.inside(XYPos(9,9)))
        {
            SDL_Rect src_rect = {direction * 32, 4 * 32, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)))
        {
            SDL_Rect src_rect = {128 + direction * 32, 0, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)))
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

    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (!current_circuit->connections_ns[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ns[pos.y][pos.x].vented);
        unsigned value = pressure_as_percent(current_circuit->connections_ns[pos.y][pos.x].value);
        
        if (vented > 2)
        {
            SDL_Rect src_rect = {16*int(rand & 3), 166, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  + 7 + int(rand % 3)) * scale + grid_offset.x, (pos.y * 32 - 9  + int(rand % 3)) * scale + grid_offset.y, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        render_number_2digit(XYPos((pos.x * 32  + 11) * scale + grid_offset.x, (pos.y * 32 - 3) * scale + grid_offset.y), value, 1, 6);
    }
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        if (!current_circuit->connections_ew[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ew[pos.y][pos.x].vented);
        unsigned value = pressure_as_percent(current_circuit->connections_ew[pos.y][pos.x].value);
        if (vented > 2)
        {
            SDL_Rect src_rect = {16*int(rand & 3), 166, 16, 16};
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
            SDL_Rect src_rect = {256, 112, 32 * 7, 32};
            SDL_Rect dst_rect = {(8 + 32 * 11) * scale, (8) * scale, 32 * 7 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {                                                                                               // Speed slider
            SDL_Rect src_rect = {624, 16, 16, 16};
            SDL_Rect dst_rect = {(8 + 32 * 11 + 32 * 5 + int(game_speed)) * scale, (8 + 8) * scale, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        {                                                                                               // Current Score
            render_number_2digit(XYPos((8 + 32 * 11 + 32 * 4 + 3) * scale, (8 + 8) * scale), current_level->last_score, 3);
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
            if (level_index > 0 && level_set->levels[level_index - 1]->best_score < 90)
                break;
            SDL_Rect src_rect = {256, 80, 32, 32};
            if (level_index == current_level_index)
                src_rect = {288, 80, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale + panel_offset.x, pos.y * 32 * scale + panel_offset.y, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

            src_rect = {0, 182 + (int(level_index) * 24), 24, 24};
            dst_rect = {(pos.x * 32 + 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y, 24 * scale, 24 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            
            unsigned score = level_set->levels[level_index]->best_score;

            render_number_2digit(XYPos((pos.x * 32 + 32 - 9 - 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), score);
            level_index++;
        }

        {
            SDL_Rect src_rect = {(current_level_index % 2) * 256, 2402 + (current_level_index / 2) * 128    , 256, 128};
            SDL_Rect dst_rect = {panel_offset.x, panel_offset.y + 176 * scale, 256 * scale, 128 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
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
            if (level_index > 0 && level_set->levels[level_index - 1]->best_score < 90)
                break;
            SDL_Rect src_rect = {256, 80, 32, 32};
            if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT && level_index == placing_subcircuit_level)
                src_rect.x = 256 + 32;

            SDL_Rect dst_rect = {pos.x * 32 * scale + panel_offset.x, (32 + 8 + pos.y * 32) * scale + panel_offset.y, 32 * scale, 32 * scale};
            if (level_index != current_level_index && !level_set->levels[level_index]->circuit->contains_subcircuit_level(current_level_index, level_set))
                 SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

            src_rect = {direction * 24, 182 + (int(level_index) * 24), 24, 24};
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
    } else if (panel_state == PANEL_STATE_MONITOR)
    {
        int mon_offset = 0;
        for (unsigned mon_index = 0; mon_index < 4; mon_index++)
        {
            int color_table = 256 + mon_index * 48;
            unsigned graph_size;
            if (mon_index == selected_monitor)
            {
                render_box(XYPos(panel_offset.x, mon_offset * scale + panel_offset.y), XYPos(7*32, 112), mon_index);

                {
                    SDL_Rect src_rect = {503, 86, 1, 1};
                    SDL_Rect dst_rect = {5 * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 200 * scale, 101 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

                }
                for (int x = 0; x < 10; x++)
                {
                    SDL_Rect src_rect = {524, 80, 13, 101};
                    SDL_Rect dst_rect = {(x * 20 + 6) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 13 * scale, 101 * scale};
                    if (x != 0)
                    {
                        src_rect.w = 5;
                        dst_rect.w = 5 * scale;
                    }
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                }
            }
            else
            {
                render_box(XYPos(panel_offset.x, mon_offset * scale + panel_offset.y), XYPos(7*32, 48), mon_index);
                {
                    SDL_Rect src_rect = {503, 86, 1, 1};
                    SDL_Rect dst_rect = {5 * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 200 * scale, 35 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

                }
                for (int x = 0; x < 10; x++)
                {
                    SDL_Rect src_rect = {504, 80, 13, 35};
                    SDL_Rect dst_rect = {(x * 20 + 6) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 13 * scale, 35 * scale};
                    if (x != 0)
                    {
                        src_rect.w = 5;
                        dst_rect.w = 5 * scale;
                    }
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                }
            }
                {
                    SDL_Rect src_rect;
                    switch (mon_index)
                    {
                        case 0:
                            src_rect = {256+80, 0+32, 16, 16}; break;
                        case 1:
                            src_rect = {352+80, 0+32, 16, 16}; break;
                        case 2:
                            src_rect = {448+80, 0+32, 16, 16}; break;
                        case 3:
                            src_rect = {544+80, 0+32, 16, 16}; break;
                        default:
                            assert (0);
                    }
                        
                    SDL_Rect dst_rect = {(200 + 5) * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 16 * scale, 16 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                
                }
            if (current_level->sim_point_count)
            {
                unsigned width = 200 / current_level->sim_point_count;
                assert (width);
                for (int x = 0; x < current_level->sim_point_count; x++)
                {
                    bool current_point = (x == current_level->sim_point_index);

                    IOValue& io_val = current_level->sim_points[x].get(mon_index);
                    if (io_val.in_value < 0)
                        continue;

                    int rec_val = io_val.recorded;
                    if (current_point)
                    {
                        switch (mon_index)
                        {
                        case 0:
                            rec_val = pressure_as_percent(current_level->N.value);
                            break;
                        case 1:
                            rec_val = pressure_as_percent(current_level->E.value);
                            break;
                        case 2:
                            rec_val = pressure_as_percent(current_level->S.value);
                            break;
                        case 3:
                            rec_val = pressure_as_percent(current_level->W.value);
                            break;
                        }
                    }

                    if (!io_val.observed && !current_point)
                    {
                        SDL_Rect src_rect = {503, 81, 1, 1};
                        int offset = 100 - io_val.in_value;
                        if (mon_index != selected_monitor)
                            offset = (offset + 2) / 3;

                        SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + offset) * scale + panel_offset.y, int(width) * scale, 1 * scale};
                        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                    }
                    else
                    {
                        int offset = 100 - rec_val;
                        if (mon_index != selected_monitor)
                            offset = (offset + 2) / 3;

                        {
                            SDL_Rect src_rect = {503, 81, 1, 1};
                            int t_offset = 100 - io_val.in_value;
                            if (mon_index != selected_monitor)
                                t_offset = (t_offset + 2) / 3;

                            int size = abs(offset - t_offset) + 1;
                            int top = offset < t_offset ? offset : t_offset;

                            SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + top) * scale + panel_offset.y, int(width) * scale, size * scale};
                            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                        }
                        if (!current_point || (frame_index % 20 < 10))
                        {
                            SDL_Rect src_rect = {503, 80, 1, 1};
                            SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + offset) * scale + panel_offset.y, int(width) * scale, 1 * scale};
                            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                        }
                    }
                }
            }
            
            if (mon_index == selected_monitor)
                mon_offset += 112 + 4;
            else
                mon_offset += 48 + 4;
        }
    } else if (panel_state == PANEL_STATE_TEST)
    {
        for (int port_index = 0; port_index < 4; port_index++)
        {
            render_box(XYPos((port_index * (48)) * scale + panel_offset.x, panel_offset.y), XYPos(48, 128 + 16), port_index);
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

            
        }
        render_box(XYPos(panel_offset.x, (128 + 16 + 8) * scale + panel_offset.y), XYPos(32*7, 128), 5);
        {
            XYPos graph_pos(8 * scale + panel_offset.x, (128 + 16 + 8 + 8) * scale + panel_offset.y);
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
        
    }

    if (show_debug)
    {
        render_number_2digit(XYPos(0, 0), debug_last_second_frames, 3);
        render_number_long(XYPos(0, 3 * 7 * scale), debug_last_second_simticks, 3);
    }


    if (show_help == 1)
    {
        render_box(XYPos(16 * scale, 16 * scale), XYPos(592, 328), 0);
        unsigned frame = (SDL_GetTicks() / 1000);
        int x = frame % 5;
        int y = frame / 5;

        render_box(XYPos((16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
        
        SDL_Rect src_rect = {x * 128, 480 + 128 * (y % 2), 128, 128};
        SDL_Rect dst_rect = {32 * scale, 32 * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        
        render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 2 * 128), 128, 128};
        dst_rect = {((128 +16) * 1 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        
        render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 3 * 128), 128, 128};
        dst_rect = {((128 +16) * 2 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 4 * 128)  + 128 * (y % 3), 128, 128};
        dst_rect = {((128 +16) * 3 + 32) * scale, 32 * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        
        render_box(XYPos((0 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (480 + 7 * 128), 128, 128};
        dst_rect = {((128 +16) * 0 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 8 * 128), 128, 128};
        dst_rect = {((128 +16) * 1 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (480 + 9 * 128), 128, 128};
        dst_rect = {((128 +16) * 2 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (1 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (480 + 10 * 128), 128, 128};
        dst_rect = {((128 +16) * 3 + 32) * scale, (1 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    if (show_help == 2)
    {
        render_box(XYPos(16 * scale, 16 * scale), XYPos(592, 328), 0);
        unsigned frame = (SDL_GetTicks() / 700);
        int x = frame % 5;
        int y = frame / 5;

        render_box(XYPos((0 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        SDL_Rect src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (480 + 11 * 128), 128, 128};
        SDL_Rect dst_rect = {((128 +16) * 0 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((1 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {(int(SDL_GetTicks() / 50) % 5) * 128, (480 + 12 * 128), 128, 128};
        dst_rect = {((128 +16) * 1 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((2 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 13 * 128), 128, 128};
        dst_rect = {((128 +16) * 2 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

        render_box(XYPos((3 * (128 +16) + 16 + 8) * scale, (0 * (128 +16) + 16 + 8) * scale), XYPos(128+16, 128+16), 4);
        src_rect = {x * 128, (480 + 14 * 128), 128, 128};
        dst_rect = {((128 +16) * 3 + 32) * scale, (0 * (128 +16) + 32) * scale, 128 * scale, 128 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);

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
    }
}

void GameState::mouse_click_in_grid()
{
    XYPos pos = (mouse - grid_offset) / scale;
    XYPos grid = pos / 32;
    if (mouse_state == MOUSE_STATE_NONE)
    {
        if (grid.x >= 9 || grid.y >= 9)
            return;
        mouse_state = MOUSE_STATE_PIPE;
        XYPos pos = (mouse - grid_offset) / scale;
        XYPos grid = pos / 32;
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
            if (mouse_rel.y < 0 && pipe_start_grid_pos.y == 0)
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && pipe_start_grid_pos.y == 9)
                mouse_rel.y = -mouse_rel.y;
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
            if (mouse_rel.x < 0 && pipe_start_grid_pos.x == 0)
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && pipe_start_grid_pos.x == 9)
                mouse_rel.x = -mouse_rel.x;
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
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        current_circuit->set_element_valve(mouse_grid, direction);
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        current_circuit->set_element_source(mouse_grid, direction);
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        current_circuit->set_element_subcircuit(mouse_grid, direction, placing_subcircuit_level, level_set);
    }
    else
    {
        assert(0);
    }
}

void GameState::mouse_click_in_panel()
{
    {
        XYPos menu_icons_pos = (mouse / scale - XYPos((16 + 32 * 11), (16)));
        if (menu_icons_pos.y < 0)
            return;
        XYPos menu_icons_grid_pos = menu_icons_pos / 32;
        if (menu_icons_grid_pos.y == 0)
        {
            unsigned sel = menu_icons_grid_pos.x;
            switch (sel)
            {
                case 0:
                    if (panel_state = PANEL_STATE_TEST)
                        current_circuit->ammended();
                    panel_state = PANEL_STATE_LEVEL_SELECT;
                    break;
                case 1:
                    if (panel_state = PANEL_STATE_TEST)
                        current_circuit->ammended();
                    panel_state = PANEL_STATE_EDITOR;
                    break;
                case 2:
                    if (panel_state = PANEL_STATE_TEST)
                        current_circuit->ammended();
                    panel_state = PANEL_STATE_MONITOR;
                    break;
                case 3:
                    current_circuit->ammended();
                    panel_state = PANEL_STATE_TEST;
                    {
                        SimPoint& simp = current_level->sim_points[current_level->sim_point_index];
                        test_value[0] = simp.N.out_value;
                        test_value[1] = simp.E.out_value;
                        test_value[2] = simp.S.out_value;
                        test_value[3] = simp.W.out_value;
                        test_drive[0] = simp.N.out_drive/3;
                        test_drive[1] = simp.E.out_drive/3;
                        test_drive[2] = simp.S.out_drive/3;
                        test_drive[3] = simp.W.out_drive/3;
                        test_pressure_histroy_index = 0;
                        test_pressure_histroy_sample_downcounter = 0;
                        for (int i = 0; i < 192; i++)
                        {
                            test_pressure_histroy[i].values[0]=0;
                            test_pressure_histroy[i].values[1]=2;
                            test_pressure_histroy[i].values[2]=1;
                            test_pressure_histroy[i].values[3]=3;
                            test_pressure_histroy[i].set = false;
                        }
                    }
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
                    show_help = 1;
                    break;
                }
            }
            return;
        }
    }
    
    XYPos panel_pos = ((mouse - panel_offset) / scale);
    if (panel_pos.y < 0 || panel_pos.x < 0)
        return;

//    printf("%d %d\n", panel_grid_pos.x, panel_grid_pos.y);
    
    
    if (panel_state == PANEL_STATE_LEVEL_SELECT)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        unsigned level_index = panel_grid_pos.x + panel_grid_pos.y * 8;
        if (level_index < LEVEL_COUNT && (level_index == 0 || level_set->levels[level_index - 1]->best_score > 90))
        {
            set_level(level_index);
            mouse_state = MOUSE_STATE_NONE;
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
                   && (level_index == 0 || level_set->levels[level_index - 1]->best_score > 90))
        {
            mouse_state = MOUSE_STATE_PLACING_SUBCIRCUIT;
            placing_subcircuit_level = level_index;
        }
        return;
    } else if (panel_state == PANEL_STATE_MONITOR)
    {
        int mon_offset = 0;
        for (unsigned mon_index = 0; mon_index < 4; mon_index++)
        {
            XYPos button_offset = panel_pos - XYPos(200 + 6, mon_offset + 6);
            if (button_offset.inside(XYPos(16, 16)))
            {
                selected_monitor = mon_index;
            }
            
            if (mon_index == selected_monitor)
                mon_offset += 112 + 4;
            else
                mon_offset += 48 + 4;
        }
    } else if (panel_state == PANEL_STATE_TEST)
    {
        int port_index = panel_pos.x / 48;
        if (port_index > 4)
            return;
        if (panel_pos.y <= (101 + 16))
        {
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
        current_circuit->set_element_pipe(grid, CONNECTIONS_NONE);
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
                bool down = (e.type == SDL_KEYDOWN);
                if (down)
                {
                    if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                        return true;
                    else if (e.key.keysym.scancode == SDL_SCANCODE_TAB)
                    {
                        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                            mouse_state = MOUSE_STATE_PLACING_SOURCE;
                        else
                            mouse_state = MOUSE_STATE_PLACING_VALVE;
                    }
                    else if (e.key.keysym.scancode == SDL_SCANCODE_Q)
                        direction = Direction((int(direction) + 4 - 1) % 4);
                    else if (e.key.keysym.scancode == SDL_SCANCODE_E)
                        direction = Direction((int(direction) + 1) % 4);
                    else if (e.key.keysym.scancode == SDL_SCANCODE_F1)
                        show_help = 1;
                    else if (e.key.keysym.scancode == SDL_SCANCODE_F5)
                        show_debug = !show_debug;
                    else printf("Uncaught key: %d\n", e.key.keysym);
                }
                break;
            }
            case SDL_TEXTINPUT:
            {
                break;
            }
            case SDL_KEYUP:
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
                if (show_help)
                {
                    if (show_help == 1)
                        show_help = 2;
                    else
                        show_help = 0;
                    break;
                }
                    
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (mouse.x < panel_offset.x)
                        mouse_click_in_grid();
                    else
                        mouse_click_in_panel();
                }
                else if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    if (mouse_state == MOUSE_STATE_NONE)
                    {
                        mouse_state = MOUSE_STATE_DELETING;
                        mouse_motion();
                    }
                    else
                        mouse_state = MOUSE_STATE_NONE;
                }
                break;
            }
            case SDL_MOUSEWHEEL:
            {
            break;
            }
        }
    }

   return false;
}
