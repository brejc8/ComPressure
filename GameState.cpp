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

GameState::GameState(const char* filename):
    mouse_button_left(false),
    mouse_button_middle(false),
    mouse_button_right(false)
{
    sdl_window = SDL_CreateWindow( "Junk", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920, 1080, 0);
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	sdl_texture = loadTexture();
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
	assert(sdl_texture);

    std::ifstream loadfile(filename);
    if (loadfile.fail() || loadfile.eof())
    {
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            levels[i] = new Level(i);
        }
//        current_circuit = new Circuit();
    }
    else
    {
        SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();
        SaveObjectList* slist = omap->get_item("levels")->get_list();
        
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            levels[i] = new Level(i, slist->get_item(i));
        }

//        current_circuit = new Circuit(omap->get_item("current_circuit")->get_map());
        delete omap;
    }
    current_circuit = levels[0]->circuit;

}

void GameState::save(const char*  filename)
{
    std::ofstream outfile (filename);
    outfile << std::setprecision(std::numeric_limits<double>::digits);
    SaveObjectMap omap;
    
    omap.add_item("current_circuit", current_circuit->save());

    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        slist->add_item(levels[i]->save());
    }
    omap.add_item("levels", slist);
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
    static int val = 0;
    for (int i = 0; i < 10; i++)
    {
        CircuitPressure N(10 * PRESSURE_SCALAR);
        CircuitPressure E = 20 * PRESSURE_SCALAR;
        CircuitPressure S = 30 * PRESSURE_SCALAR;
        CircuitPressure W = (val > 50 * PRESSURE_SCALAR) ? 25 * PRESSURE_SCALAR : 35 * PRESSURE_SCALAR;
        val+=5;
        if (val > 100 * PRESSURE_SCALAR) val = 0;
        current_circuit->sim_pre(PressureAdjacent(N, E, S, W));
        current_circuit->sim_post(PressureAdjacent(N, E, S, W));
    }
}

void GameState::draw_char(XYPos& pos, char c)
{
    SDL_Rect src_rect = {(c % 16) * 16, 192 + (c / 16) * 16, 16, 16};
    SDL_Rect dst_rect = {pos.x, pos.y, 16, 16};
    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    pos.x += 16;

}


void GameState::render()
{
    SDL_RenderClear(sdl_renderer);
    XYPos window_size;
    SDL_GetWindowSize(sdl_window, &window_size.x, &window_size.y);
    XYPos pos;
    
    {
        SDL_Rect src_rect = {0, 32, 32, 32};
        SDL_Rect dst_rect = {-32 * scale + grid_offset.x, (4 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.x = (9 * 32) * scale + grid_offset.x;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        
        src_rect = {96, 0, 32, 32};
        dst_rect = {(4 * 32) * scale + grid_offset.x, -32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        dst_rect.y = (9 * 32) * scale + grid_offset.y;
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        SDL_Rect src_rect = {256+32, 32, 16, 16};
        if (pos.y > 0)
            src_rect.y += 16;
        if (pos.y == 8)
            src_rect.y += 16;
        if (pos.x > 0)
            src_rect.x += 16;
        if (pos.x == 8)
            src_rect.x += 16;
            
        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        SDL_Rect src_rect = current_circuit->elements[pos.y][pos.x]->getimage();
        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    if (mouse_state == MOUSE_STATE_PIPE)
    {
        if (pipe_start_ns)
        {
            SDL_Rect src_rect = {0, 0, 1, 1};
            SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 + 16 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
            if (mouse_grid.y >= pipe_start_grid_pos.y)   //north - southwards
            {
                    Connections con;
                    XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.x -= 16;
                    if (mouse_rel.y > abs(mouse_rel.x))     // south
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
                    Connections con;
                    XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.x -= 16;
                    if (-mouse_rel.y > abs(mouse_rel.x))    // north
                        con = CONNECTIONS_NS;
                    else if (mouse_rel.x >= 0)               // east
                        con = CONNECTIONS_ES;
                    else                                    // west
                        con = CONNECTIONS_WS;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale + grid_offset.x, (pipe_start_grid_pos.y - 1) * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }
        else
        {
            SDL_Rect src_rect = {0, 0, 1, 1};
            SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 + 16 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
            if (mouse_grid.x >= pipe_start_grid_pos.x)   //west - eastwards
            {
                    Connections con;
                    XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.y -= 16;
                    if (mouse_rel.x > abs(mouse_rel.y))     // west
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
                    Connections con;
                    XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.y -= 16;
                    if (-mouse_rel.x > abs(mouse_rel.y))    // east
                        con = CONNECTIONS_EW;
                    else if (mouse_rel.y >= 0)               // south
                        con = CONNECTIONS_ES;
                    else                                    // north
                        con = CONNECTIONS_NE;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {(pipe_start_grid_pos.x - 1) * 32 * scale + grid_offset.x, pipe_start_grid_pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset)/ scale) / 32;
        SDL_Rect src_rect = {direction * 32, 4 * 32, 32, 32};
        SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        SDL_Rect src_rect = {(4 + direction) * 32, 0, 32, 32};
        SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
    }

    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (!current_circuit->connections_ns[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ns[pos.y][pos.x].vented);
        Pressure value = (current_circuit->connections_ns[pos.y][pos.x].value + PRESSURE_SCALAR/2) / PRESSURE_SCALAR;
        
        if (vented > 2)
        {
            SDL_Rect src_rect = {16*int(rand & 3), 166, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  + 7 + int(rand % 3)) * scale + grid_offset.x, (pos.y * 32 - 9  + int(rand % 3)) * scale + grid_offset.y, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        if (value == 100)
        {
            SDL_Rect src_rect = {40, 161, 9, 5};
            SDL_Rect dst_rect = {(pos.x * 32  + 11) * scale + grid_offset.x, (pos.y * 32 - 3) * scale + grid_offset.y, 9 * scale, 5 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        else
        {
            SDL_Rect src_rect = {(value / 10) * 4, 161, 4, 5};
            SDL_Rect dst_rect = {(pos.x * 32  + 11) * scale + grid_offset.x, (pos.y * 32 - 3) * scale + grid_offset.y, 4 * scale, 5 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            src_rect.w = 5;
            src_rect.x = (value % 10) * 4;
            dst_rect.w = 5 * scale;
            dst_rect.x += 4 * scale;
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        if (!current_circuit->connections_ew[pos.y][pos.x].touched)
            continue;
        Pressure vented = (current_circuit->connections_ew[pos.y][pos.x].vented);
        Pressure value = (current_circuit->connections_ew[pos.y][pos.x].value + PRESSURE_SCALAR/2) / PRESSURE_SCALAR;
        if (vented > 2)
        {
            SDL_Rect src_rect = {16*int(rand & 3), 166, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  - 9) * scale + int(rand % 3 * scale) + grid_offset.x, (pos.y * 32 + 7 ) * scale + int(rand % 3 * scale) + grid_offset.y, 16 * scale, 16 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        if (value == 100)
        {
            SDL_Rect src_rect = {40, 161, 9, 5};
            SDL_Rect dst_rect = {(pos.x * 32  - 5) * scale + grid_offset.x, (pos.y * 32 + 13) * scale + grid_offset.y, 9 * scale, 5 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        else
        {
            SDL_Rect src_rect = {(value / 10) * 4, 161, 4, 5};
            SDL_Rect dst_rect = {(pos.x * 32  - 5) * scale + grid_offset.x, (pos.y * 32 + 13) * scale + grid_offset.y, 4 * scale, 5 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            src_rect.w = 5;
            src_rect.x = (value % 10) * 4;
            dst_rect.w = 5 * scale;
            dst_rect.x += 4 * scale;
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }


    SDL_RenderPresent(sdl_renderer);
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
                if (mouse_state == MOUSE_STATE_DELETING)
                {
                    XYPos grid = ((mouse - grid_offset) / scale) / 32;
                    if (grid.x >= 9 || grid.y >= 9)
                        break;
                    current_circuit->set_element_pipe(grid, CONNECTIONS_NONE);
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                if (e.button.button == SDL_BUTTON_LEFT)
                    mouse_button_left = false;
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_middle = false;
                if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    if (mouse_state == MOUSE_STATE_DELETING)
                        mouse_state = MOUSE_STATE_NONE;
                    mouse_button_right = false;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                {
                        XYPos pos = (mouse - grid_offset) / scale;
                        XYPos grid = pos / 32;
                        if (grid.x >= 9 || grid.y >= 9)
                            break;
                }
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    mouse_button_left = true;
                    if (mouse_state == MOUSE_STATE_NONE)
                    {
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
                            if (mouse_rel.y < 0)    //south - northwards
                            {
                                pipe_start_grid_pos.y--;
                                Connections con;
                                if (-mouse_rel.y > abs(mouse_rel.x))    // north
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
                                if (mouse_rel.y > abs(mouse_rel.x))    // north
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
                            if (mouse_rel.x < 0)    //east - westwards
                            {
                                pipe_start_grid_pos.x--;
                                Connections con;
                                if (-mouse_rel.x > abs(mouse_rel.y))    // west
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
                                if (mouse_rel.x > abs(mouse_rel.y))    // west
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

                }
                else if (e.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_middle = true;
                else if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    mouse_button_right = true;
                    if (mouse_state == MOUSE_STATE_NONE)
                    {
                        XYPos grid = ((mouse - grid_offset) / scale) / 32;
                        current_circuit->set_element_pipe(grid, CONNECTIONS_NONE);
                        mouse_state = MOUSE_STATE_DELETING;
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

    if (mouse_button_left)
    {
    }
    if (mouse_button_middle)
    {
    }
    if (mouse_button_right)
    {
    }

   return false;
}
