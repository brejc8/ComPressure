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
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
	sdl_texture = loadTexture();
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
	assert(sdl_texture);

    std::ifstream loadfile(filename);
    if (loadfile.fail() || loadfile.eof())
        return;

    SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();


    delete omap;
}

void GameState::save(const char*  filename)
{
    std::ofstream outfile (filename);
    outfile << std::setprecision(std::numeric_limits<double>::digits);
    SaveObjectMap omap;
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
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        GridSquare::State state = squares[pos.x][pos.y].state;
        if (state == GridSquare::STATE_PIPE)
        {
            GridSquare::Connections con = squares[pos.x][pos.y].connections;
            SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale, pos.y * 32 * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
        else if (state == GridSquare::STATE_VALVE)
        {
            Direction dir = squares[pos.x][pos.y].direction;
            SDL_Rect src_rect = {dir * 32, (4) * 32, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale, pos.y * 32 * scale, 32 * scale, 32 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
        }
    }

    if (mouse_state == MOUSE_STATE_PIPE)
    {
        if (pipe_start_ns)
        {
            SDL_Rect src_rect = {0, 0, 1, 1};
            SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 + 16 - 4)  * scale, (pipe_start_grid_pos.y * 32 - 4) * scale, 8 * scale, 8 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            XYPos mouse_grid = (mouse / scale) / 32;
            if (mouse_grid.y >= pipe_start_grid_pos.y)   //north - southwards
            {
                    GridSquare::Connections con;
                    XYPos mouse_rel = (mouse / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.x -= 16;
                    if (mouse_rel.y > abs(mouse_rel.x))     // south
                        con = GridSquare::CONNECTIONS_NS;
                    else if (mouse_rel.x >= 0)               // east
                        con = GridSquare::CONNECTIONS_NE;
                    else                                    // west
                        con = GridSquare::CONNECTIONS_NW;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale, pipe_start_grid_pos.y * 32 * scale, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            else                                        //south - northwards
            {
                    GridSquare::Connections con;
                    XYPos mouse_rel = (mouse / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.x -= 16;
                    if (-mouse_rel.y > abs(mouse_rel.x))    // north
                        con = GridSquare::CONNECTIONS_NS;
                    else if (mouse_rel.x >= 0)               // east
                        con = GridSquare::CONNECTIONS_ES;
                    else                                    // west
                        con = GridSquare::CONNECTIONS_WS;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale, (pipe_start_grid_pos.y - 1) * 32 * scale, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }
        else
        {
            SDL_Rect src_rect = {0, 0, 1, 1};
            SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 - 4)  * scale, (pipe_start_grid_pos.y * 32 + 16 - 4) * scale, 8 * scale, 8 * scale};
            SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            XYPos mouse_grid = (mouse / scale) / 32;
            if (mouse_grid.x >= pipe_start_grid_pos.x)   //west - eastwards
            {
                    GridSquare::Connections con;
                    XYPos mouse_rel = (mouse / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.y -= 16;
                    if (mouse_rel.x > abs(mouse_rel.y))     // west
                        con = GridSquare::CONNECTIONS_EW;
                    else if (mouse_rel.y >= 0)               // south
                        con = GridSquare::CONNECTIONS_WS;
                    else                                    // north
                        con = GridSquare::CONNECTIONS_NW;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {pipe_start_grid_pos.x * 32 * scale, pipe_start_grid_pos.y * 32 * scale, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
            else                                        //east - westwards
            {
                    GridSquare::Connections con;
                    XYPos mouse_rel = (mouse / scale) - (pipe_start_grid_pos * 32);
                    mouse_rel.y -= 16;
                    if (-mouse_rel.x > abs(mouse_rel.y))    // east
                        con = GridSquare::CONNECTIONS_EW;
                    else if (mouse_rel.y >= 0)               // south
                        con = GridSquare::CONNECTIONS_ES;
                    else                                    // north
                        con = GridSquare::CONNECTIONS_NE;
                    SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                    SDL_Rect dst_rect = {(pipe_start_grid_pos.x - 1) * 32 * scale, pipe_start_grid_pos.y * 32 * scale, 32 * scale, 32 * scale};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
            }
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = (mouse / scale) / 32;
        SDL_Rect src_rect = {direction * 32, 4 * 32, 32, 32};
        SDL_Rect dst_rect = {mouse_grid.x * 32 * scale, mouse_grid.y * 32 * scale, 32 * scale, 32 * scale};
        SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
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
                    XYPos grid = (mouse / scale) / 32;
                    squares[grid.x][grid.y].connections = GridSquare::CONNECTIONS_NONE;
                    squares[grid.x][grid.y].state = GridSquare::STATE_PIPE;
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
                        XYPos pos = mouse / scale;
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
                        XYPos pos = mouse / scale;
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
                        XYPos mouse_grid = (mouse / scale) / 32;
                        XYPos mouse_rel = (mouse / scale) - (pipe_start_grid_pos * 32);
                        if (pipe_start_ns)
                        {
                            mouse_rel.x -= 16;
                            if (mouse_rel.y < 0)    //south - northwards
                            {
                                pipe_start_grid_pos.y--;
                                GridSquare::Connections con;
                                if (-mouse_rel.y > abs(mouse_rel.x))    // north
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NS;
                                }
                                else if (mouse_rel.x < 0)               // west
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_WS;
                                    pipe_start_ns = false;
                                }
                                else                                    // east
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_ES;
                                    pipe_start_ns = false;
                                    pipe_start_grid_pos.x++;
                                }
                                
                            }
                            else
                            {
                                GridSquare::Connections con;
                                if (mouse_rel.y > abs(mouse_rel.x))    // north
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NS;
                                    pipe_start_grid_pos.y++;

                                }
                                else if (mouse_rel.x < 0)               // west
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NW;
                                    pipe_start_ns = false;
                                }
                                else                                    // east
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NE;
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
                                GridSquare::Connections con;
                                if (-mouse_rel.x > abs(mouse_rel.y))    // west
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_EW;
                                }
                                else if (mouse_rel.y < 0)               // north
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NE;
                                    pipe_start_ns = true;
                                }
                                else                                    // south
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_ES;
                                    pipe_start_ns = true;
                                    pipe_start_grid_pos.y++;
                                }
                                
                            }
                            else                    //west - eastwards
                            {
                                GridSquare::Connections con;
                                if (mouse_rel.x > abs(mouse_rel.y))    // west
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_EW;
                                    pipe_start_grid_pos.x++;

                                }
                                else if (mouse_rel.y < 0)               // north
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_NW;
                                    pipe_start_ns = true;
                                }
                                else                                    // south
                                {
                                    squares[pipe_start_grid_pos.x][pipe_start_grid_pos.y].connections = GridSquare::CONNECTIONS_WS;
                                    pipe_start_ns = true;
                                    pipe_start_grid_pos.y++;
                                }
                                
                            }
                        }
                    }
                    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                    {
                        XYPos mouse_grid = (mouse / scale) / 32;
                        squares[mouse_grid.x][mouse_grid.y].state = GridSquare::STATE_VALVE;
                        squares[mouse_grid.x][mouse_grid.y].direction = direction;
                    }

                }
                else if (e.button.button == SDL_BUTTON_MIDDLE)
                    mouse_button_middle = true;
                else if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    mouse_button_right = true;
                    if (mouse_state == MOUSE_STATE_NONE)
                    {
                        XYPos grid = (mouse / scale) / 32;
                        squares[grid.x][grid.y].connections = GridSquare::CONNECTIONS_NONE;
                        squares[grid.x][grid.y].state = GridSquare::STATE_PIPE;
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

    XYPos pos = mouse / scale;
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
