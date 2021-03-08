#define _USE_MATH_DEFINES
#include <cmath>

#include "GameState.h"
#include "SaveState.h"
#include "Misc.h"
#include "Dialogue.h"
#include "Demo.h"

#include <cassert>
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <limits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <shellapi.h>
#endif

static void DisplayWebsite(const char* url)
{
    #ifdef _WIN32
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    #else
        char buf[1024];
        snprintf(buf, sizeof(buf), "xdg-open %s", url);
        system(buf);
    #endif
}

GameState::GameState(const char* filename)
{
    bool load_was_good = false;
    try 
    {
        std::ifstream loadfile(filename);
        if (!loadfile.fail() && !loadfile.eof())
        {
            SaveObjectMap* omap;
            omap = SaveObject::load(loadfile)->get_map();
            level_set = new LevelSet(omap->get_item("levels"));

            current_level_index = omap->get_num("current_level_index");
            if (!level_set->is_playable(current_level_index))
                current_level_index = 0;
            game_speed = omap->get_num("game_speed");
            show_debug = omap->get_num("show_debug");
            next_dialogue_level = omap->get_num("next_dialogue_level");
            show_help_page = omap->get_num("show_help_page");
            flash_editor_menu = omap->get_num("flash_editor_menu");
            flash_steam_inlet = omap->get_num("flash_steam_inlet");
            flash_valve = omap->get_num("flash_valve");

            sound_volume = omap->get_num("sound_volume");
            music_volume = omap->get_num("music_volume");
            update_scale(omap->get_num("scale"));
            if (scale < 1)
                scale = 3;
            full_screen = omap->get_num("full_screen");
            minutes_played = omap->get_num("minutes_played");

            delete omap;
            load_was_good = true;
        }
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << "\n";
    }

    if (!load_was_good)
    {
        level_set = new LevelSet();
        current_level_index = 0;
        update_scale(3);
    }

    set_level(current_level_index);

    sdl_window = SDL_CreateWindow( "ComPressure", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640*scale, 360*scale, SDL_WINDOW_RESIZABLE);
    SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED);
	sdl_texture = loadTexture("texture.png");
	sdl_tutorial_texture = loadTexture("tutorial.png");
	sdl_levels_texture = loadTexture("levels.png");
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
    
    {
        SDL_Surface* icon_surface = IMG_Load("icon.png");
        SDL_SetWindowIcon(sdl_window, icon_surface);
	    SDL_FreeSurface(icon_surface);
    }

    font = TTF_OpenFont("fixed.ttf", 10);

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
    
SaveObject* GameState::save(bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("levels", level_set->save(lite));
    omap->add_num("current_level_index", current_level_index);
    omap->add_num("game_speed", game_speed);
    omap->add_num("show_debug", show_debug);
    omap->add_num("next_dialogue_level", next_dialogue_level);
    omap->add_num("show_help_page", show_help_page);
    omap->add_num("flash_editor_menu", flash_editor_menu);
    omap->add_num("flash_steam_inlet", flash_steam_inlet);
    omap->add_num("flash_valve", flash_valve);
    omap->add_num("scale", scale);
    omap->add_num("full_screen", full_screen);
    omap->add_num("sound_volume", sound_volume);
    omap->add_num("music_volume", music_volume);
    omap->add_num("minutes_played", minutes_played + SDL_GetTicks()/ 1000 / 60);

    return omap;
}

void GameState::save(std::ostream& outfile, bool lite)
{
    SaveObject* omap = save(lite);
    omap->save(outfile);
    delete omap;
}


void GameState::save(const char*  filename, bool lite)
{
    std::ofstream outfile (filename);
    save(outfile);
}


class ServerComms
{
public:
    SaveObject* send;
    ServerResp* resp;

    ServerComms(SaveObject* send_, ServerResp* resp_ = NULL):
        send(send_),
        resp(resp_)
    {}
};

static int fetch_from_server_thread(void *ptr)
{
    IPaddress ip;
    TCPsocket tcpsock;
    ServerComms* comms = (ServerComms*)ptr;
    
    if (SDLNet_ResolveHost(&ip, "compressure.brej.org", 42069) == -1)
//    if (SDLNet_ResolveHost(&ip, "192.168.0.81", 42069) == -1)
    {
        printf("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        if (comms->resp)
        {
            comms->resp->error = true;
            comms->resp->done = true;
            comms->resp->working = false;
        }
        delete comms->send;
        delete comms;
        return 0;
    }

    tcpsock = SDLNet_TCP_Open(&ip);
    if (!tcpsock)
    {
        printf("SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        if (comms->resp)
        {
            comms->resp->error = true;
            comms->resp->done = true;
            comms->resp->working = false;
        }
        delete comms->send;
        delete comms;
        return 0;
    }
    
    try 
    {
        std::ostringstream stream;
        comms->send->save(stream);
        std::string comp = compress_string(stream.str());

        uint32_t length = comp.length();
        SDLNet_TCP_Send(tcpsock, (char*)&length, 4);
        SDLNet_TCP_Send(tcpsock, comp.c_str(), length);

        if (comms->resp)
        {
            int got = SDLNet_TCP_Recv(tcpsock, (char*)&length, 4);
            if (got != 4)
                throw(std::runtime_error("Connection closed early"));
            char* data = (char*)malloc(length);
            got = SDLNet_TCP_Recv(tcpsock, data, length);
            if (got != length)
            {
                free (data);
                throw(std::runtime_error("Connection closed early"));
            }
            std::string in_str(data, length);
            free (data);
            std::string decomp = decompress_string(in_str);
            std::istringstream decomp_stream(decomp);
            comms->resp->resp = SaveObject::load(decomp_stream);
        }
        
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << "\n";
        comms->resp->error = true;
    }
    SDLNet_TCP_Close(tcpsock);
    if (comms->resp)
    {
        comms->resp->done = true;
        comms->resp->working = false;
    }
    delete comms->send;
    delete comms;
    return 0;
}

void GameState::post_to_server(SaveObject* send, bool sync)
{
    SDL_Thread *thread = SDL_CreateThread(fetch_from_server_thread, "PostToServer", (void *)new ServerComms(send));
    if (sync)
        SDL_WaitThread(thread, NULL);
}


void GameState::fetch_from_server(SaveObject* send, ServerResp* resp)
{
    resp->working = true;
    resp->done = false;
    resp->error = false;
    if (resp->resp)
        delete resp->resp;
    resp->resp = NULL;
    SDL_Thread *thread = SDL_CreateThread(fetch_from_server_thread, "FetchFromServer", (void *)new ServerComms(send, resp));
}


void GameState::save_to_server(bool sync)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "save");
    omap->add_item("content", save(true));
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    post_to_server(omap, sync);
}

void GameState::score_submit(bool sync)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "score_submit");
    omap->add_item("levels", level_set->save(true));
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    post_to_server(omap, sync);
}

void GameState::score_fetch(unsigned level)
{
    if (scores_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "score_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    omap->add_num("level", level);
    fetch_from_server(omap, &scores_from_server);
}


GameState::~GameState()
{
    Mix_FreeChunk(vent_steam_wav);
    Mix_FreeChunk(move_steam_wav);
    Mix_FreeMusic(music);

    TTF_CloseFont(font);

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
    SDL_Texture* new_texture = SDL_CreateTextureFromSurface(sdl_renderer, loadedSurface);
	assert(new_texture);
	SDL_FreeSurface(loadedSurface);
	return new_texture;
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
        if (skip_to_next_subtest)
        {
            count = current_level->substep_count - current_level->substep_index;
        }
        while (count)
        {
            int subcount = count < 100 ? count : 100;
            current_level->advance(subcount, monitor_state);
            count -= subcount;
            debug_simticks += subcount;
            if (SDL_TICKS_PASSED(SDL_GetTicks(), time + 100))
            {
                if (!skip_to_next_subtest)
                    game_speed--;
                break;
            }
        }
        if (!count)
        {
            if (skip_to_subtest_index < 0 || skip_to_subtest_index == current_level->sim_point_index)
                skip_to_next_subtest = false;
        }

        {
            SimPoint& simp = current_level->tests[current_level->test_index].sim_points[current_level->sim_point_index];
            for (int p = 0; p < 4; p++)
            {
                test_value[p] = simp.values[p];
                test_drive[p] = simp.force[p]/3;
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
    if (current_level->best_score_set)
    {
        current_level->best_score_set = false;
        score_submit(false);
    }
}

void GameState::audio()
{
    Mix_Volume(0, pressure_as_percent(current_circuit->last_vented*40) * sound_volume / 100 / 5);
    Mix_Volume(1, pressure_as_percent(current_circuit->last_moved*1) * sound_volume / 100/ 2);
    Mix_VolumeMusic(music_volume);
}

XYPos GameState::get_text_size(std::string& text)
{
    XYPos rep = XYPos(0,0);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while (true)
    {
        pos = text.find("\n", prev);
        std::string sub = text.substr(prev, pos - prev);
        prev = pos + 1;
        XYPos sub_size;
        TTF_SizeUTF8(font, sub.c_str(), &sub_size.x, &sub_size.y);
        if (sub_size.x > rep.x)
            rep.x = sub_size.x;
        rep.y += TTF_FontLineSkip(font);
        if (pos == std::string::npos)
            break;
    }
    return rep;
}

void GameState::render_texture_custom(SDL_Texture* texture, SDL_Rect& src_rect, SDL_Rect& dst_rect)
{
    SDL_Rect my_dst_rect = {dst_rect.x + screen_offset.x, dst_rect.y + screen_offset.y, dst_rect.w, dst_rect.h};
    SDL_RenderCopy(sdl_renderer, texture, &src_rect, &my_dst_rect);
}

void GameState::render_texture(SDL_Rect& src_rect, SDL_Rect& dst_rect)
{
    render_texture_custom(sdl_texture, src_rect, dst_rect);
}



void GameState::render_number_2digit(XYPos pos, unsigned value, unsigned scale_mul, unsigned bg_colour, unsigned fg_colour)
{
    int myscale = scale * scale_mul;
    {
        SDL_Rect src_rect = {503, 80 + int(bg_colour), 1, 1};
        SDL_Rect dst_rect = {pos.x, pos.y-myscale, 9 * myscale, 7 * myscale};
        render_texture(src_rect, dst_rect);
    }

    if (value == 100)
    {
        SDL_Rect src_rect = {40 + int(fg_colour/4) * 64, 160 + int(fg_colour%4) * 5, 9, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 9 * myscale, 5 * myscale};
        render_texture(src_rect, dst_rect);
    }
    else
    {
        SDL_Rect src_rect = {0 + int(fg_colour/4) * 64 + (int(value) / 10) * 4, 160 + int(fg_colour%4) * 5, 4, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
        render_texture(src_rect, dst_rect);
        src_rect.w = 5;
        src_rect.x = 0 + (value % 10) * 4 + int(fg_colour/4) * 64;
        dst_rect.w = 5 * myscale;
        dst_rect.x += 4 * myscale;
        render_texture(src_rect, dst_rect);
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
        render_texture(src_rect, dst_rect);
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
        render_texture(src_rect, dst_rect);
        dst_rect.x += 4 * myscale;
        i++;

    }
}

void GameState::render_box(XYPos pos, XYPos size, unsigned colour)
{
    int color_table = 256 + colour * 32;
    SDL_Rect src_rect = {color_table, 80, 16, 16};
    SDL_Rect dst_rect = {pos.x, pos.y, 16 * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Top Left

    src_rect = {color_table + 16, 80, 1, 16};
    dst_rect = {pos.x + 16 * scale, pos.y, (size.x - 32) * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Top

    src_rect = {color_table + 16, 80, 16, 16};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y, 16 * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Top Right
    
    src_rect = {color_table, 80 + 16, 16, 1};
    dst_rect = {pos.x, pos.y + 16 * scale, 16 * scale, (size.y - 32) * scale};
    render_texture(src_rect, dst_rect);     //  Left

    src_rect = {color_table + 16, 80 + 16, 1, 1};
    dst_rect = {pos.x + 16 * scale, pos.y + 16 * scale, (size.x - 32) * scale, (size.y - 32) * scale};
    render_texture(src_rect, dst_rect);     //  Middle

    src_rect = {color_table + 16, 80 + 16, 16, 1};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y + 16 * scale, 16 * scale, (size.y - 32) * scale};
    render_texture(src_rect, dst_rect);     //  Right

    src_rect = {color_table, 80 + 16, 16, 16};
    dst_rect = {pos.x, pos.y + (size.y - 16) * scale, 16 * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Bottom Left

    src_rect = {color_table + 16, 80 + 16, 1, 16};
    dst_rect = {pos.x + 16 * scale, pos.y + (size.y - 16) * scale, (size.x - 32) * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Bottom

    src_rect = {color_table + 16, 80 + 16, 16, 16};
    dst_rect = {pos.x + (size.x - 16) * scale, pos.y + (size.y - 16) * scale, 16 * scale, 16 * scale};
    render_texture(src_rect, dst_rect);     //  Bottom Right
}

void GameState::render_button(XYPos pos, XYPos content, unsigned colour)
{
    render_box(pos, XYPos(32,32), colour);
    SDL_Rect src_rect = {content.x , content.y , 24, 24};
    SDL_Rect dst_rect = {pos.x + 4 * scale, pos.y + 4 * scale, 24 * scale, 24 * scale};
    render_texture(src_rect, dst_rect);

}

void GameState::render_text_wrapped(XYPos tl, const char* string, int width)
{
    SDL_Color color={0xff,0xff,0xff};
    SDL_Surface *text_surface = TTF_RenderUTF8_Blended_Wrapped(font, string, color, width);
    SDL_Texture* new_texture = SDL_CreateTextureFromSurface(sdl_renderer, text_surface);
    
    SDL_Rect src_rect;
    SDL_GetClipRect(text_surface, &src_rect);
    SDL_Rect dst_rect = src_rect;
    dst_rect.x = tl.x * scale;
    dst_rect.y = tl.y * scale;
    dst_rect.w = src_rect.w * scale;
    dst_rect.h = src_rect.h * scale;

    render_texture_custom(new_texture, src_rect, dst_rect);
	SDL_DestroyTexture(new_texture);
	SDL_FreeSurface(text_surface);
}

void GameState::render_text(XYPos tl, const char* string)
{
    std::string text = string;
    SDL_Color color={0xff,0xff,0xff};

    XYPos rep = XYPos(0,0);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while (true)
    {
        pos = text.find("\n", prev);
        std::string sub = text.substr(prev, pos - prev);
        prev = pos + 1;

        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font, sub.c_str(), color);
        SDL_Texture* new_texture = SDL_CreateTextureFromSurface(sdl_renderer, text_surface);

        SDL_Rect src_rect;
        SDL_GetClipRect(text_surface, &src_rect);
        SDL_Rect dst_rect = src_rect;
        dst_rect.x = tl.x * scale;
        dst_rect.y = tl.y * scale;
        dst_rect.w = src_rect.w * scale;
        dst_rect.h = src_rect.h * scale;

        render_texture_custom(new_texture, src_rect, dst_rect);
	    SDL_DestroyTexture(new_texture);
    	SDL_FreeSurface(text_surface);

        tl.y += TTF_FontLineSkip(font);
        if (pos == std::string::npos)
            break;
    }
}

void GameState::update_scale(int newscale)
{
    if (scale != newscale)
    {
        scale = newscale;
        grid_offset = XYPos(32 * scale, 32 * scale);
        panel_offset = XYPos((8 + 32 * 11) * scale, (8 + 8 + 32) * scale);
    }
}


void GameState::render()
{
    if (scores_from_server.done && !scores_from_server.error)
    {
        try 
        {
            SaveObjectMap* omap = scores_from_server.resp->get_map();
            unsigned level = omap->get_num("level");
            level_set->levels[level]->global_fetched_score = omap->get_num("score");
            SaveObjectList* glist = omap->get_item("graph")->get_list();
            for (unsigned i = 0; i < 200; i++)
            {
                level_set->levels[level]->global_score_graph[i] = glist->get_num(i);
            }
            level_set->levels[level]->global_score_graph_set = true;
            level_set->levels[level]->global_score_graph_time = SDL_GetTicks();
        }
        catch (const std::runtime_error& error)
        {
            std::cerr << error.what() << "\n";
        }
        if (scores_from_server.resp)
            delete scores_from_server.resp;
        scores_from_server.resp = NULL;
        scores_from_server.done = false;
    }

    SDL_RenderClear(sdl_renderer);
    XYPos window_size;
    SDL_GetWindowSize(sdl_window, &window_size.x, &window_size.y);
    {
        int sx = window_size.x / 640;
        int sy = window_size.y / 360;
        int newscale = std::min(sy, sx);
        if (newscale < 1)
            newscale = 1;
        update_scale(newscale);
        screen_offset.x = (window_size.x - (newscale * 640)) / 2;
        screen_offset.y = (window_size.y - (newscale * 360)) / 2;
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
        for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
        {
            dst_rect.x = 640*x;
            dst_rect.y = 360*y;
            render_texture(src_rect, dst_rect);
        }
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
        render_texture(src_rect, dst_rect);

        src_rect = {224, 0, 32, 32};                // E
        dst_rect = {(9 * 32) * scale + grid_offset.x, (4 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        render_texture(src_rect, dst_rect);

        src_rect = {192, 0, 32, 32};                // N
        dst_rect = {(4 * 32) * scale + grid_offset.x, -32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        render_texture(src_rect, dst_rect);

        src_rect = {128, 0, 32, 32};                // S
        dst_rect = {(4 * 32) * scale + grid_offset.x, (9 * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
        render_texture(src_rect, dst_rect);
    }

    if (current_level->tests[current_level->test_index].sim_points.size() == current_level->sim_point_index + 1)
    {
        unsigned test_index = current_level->test_index;
        int pin_index = current_level->tests[test_index].tested_direction;
        unsigned value = current_level->tests[test_index].sim_points.back().values[pin_index];
        XYPos num_pos;
        switch (pin_index)
        {
            case DIRECTION_N:
                num_pos = XYPos(4, -1); break;
            case DIRECTION_E:
                num_pos = XYPos(9, 4); break;
            case DIRECTION_S:
                num_pos = XYPos(4, 9); break;
            case DIRECTION_W:
                num_pos = XYPos(-1, 4); break;
            default:
                assert(0);
        }
        num_pos *= 32;
        num_pos += XYPos(6, 11);
        num_pos *= scale;
        num_pos += grid_offset;
        render_number_2digit(num_pos, value, 2, 6, 0);
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
        if (current_circuit->blocked[pos.y][pos.x])
            src_rect = {384, 80, 32, 32};
            
        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        render_texture(src_rect, dst_rect);
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
                render_texture(src_rect, dst_rect);
            }
        }


        XYPos src_pos = current_circuit->elements[pos.y][pos.x]->getimage();
        if (src_pos != XYPos(0,0))
        {
            SDL_Rect src_rect = {src_pos.x, src_pos.y, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }

        src_pos = current_circuit->elements[pos.y][pos.x]->getimage_fg();
        if (src_pos != XYPos(0,0))
        {
            SDL_Rect src_rect =  {src_pos.x, src_pos.y, 24, 24};
            SDL_Rect dst_rect = {(pos.x * 32 + 4) * scale + grid_offset.x, (pos.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
            render_texture(src_rect, dst_rect);
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
            if (pos.x >= tl.x && pos.x <= br.x && pos.y >= tl.y && pos.y <= br.y && !current_circuit->elements[pos.y][pos.x]->is_empty() && !current_circuit->is_blocked(pos))
                selected = true;
        }
        
        if (selected)                     //selected
        {
            SDL_Rect src_rect =  {256, 176, 32, 32};
            SDL_Rect dst_rect = {(pos.x * 32) * scale + grid_offset.x, (pos.y * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
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
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_circuit->blocked[pipe_start_grid_pos.y - 1][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
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
                    render_texture(src_rect, dst_rect);
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
                    render_texture(src_rect, dst_rect);
            }
//             SDL_Rect src_rect = {503, 80, 1, 1};
//             SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 + 16 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
//             render_texture(src_rect, dst_rect);
        }
        else
        {
            mouse_rel.y -= 16;
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x - 1]))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
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
                    render_texture(src_rect, dst_rect);
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
                    render_texture(src_rect, dst_rect);
            }
//             SDL_Rect src_rect = {503, 80, 1, 1};
//             SDL_Rect dst_rect = {(pipe_start_grid_pos.x * 32 - 4)  * scale + grid_offset.x, (pipe_start_grid_pos.y * 32 + 16 - 4) * scale + grid_offset.y, 8 * scale, 8 * scale};
//             render_texture(src_rect, dst_rect);
        }
    }
    if (mouse_state == MOUSE_STATE_PIPE_DRAGGING)
    {
        int i = 0;
        XYPos n1,n2,n3;
        std::list<XYPos>::iterator it = pipe_drag_list.begin();
        n2 = *it;
        it++;
        n3 = *it;
        it++;
        for (;it != pipe_drag_list.end(); ++it)
        {
            n1 = n2;
            n2 = n3;
            n3 = *it;
            XYPos d1 = n1 - n2;
            XYPos d2 = n3 - n2;
            XYPos d = d1 + d2;

            if (frame_index % 20 < 10)
            {
                Connections con = Connections(0);

                if (d.y < 0)
                {
                    if (d.x < 0)
                        con = CONNECTIONS_NW;
                    else if (d.x > 0)
                        con = CONNECTIONS_NE;
                    else
                        assert(0);
                }
                else if (d.y > 0)
                {
                    if (d.x < 0)
                        con = CONNECTIONS_WS;
                    else if (d.x > 0)
                        con = CONNECTIONS_ES;
                    else
                        assert(0);
                }
                else
                {
                    if (d1.x)
                        con = CONNECTIONS_EW;
                    else if (d1.y)
                        con = CONNECTIONS_NS;
                    else assert(0);
                }
                SDL_Rect src_rect = {(con % 4) * 32, (con / 4) * 32, 32, 32};
                SDL_Rect dst_rect = {n2.x * 32 * scale + grid_offset.x, n2.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                render_texture(src_rect, dst_rect);
            }
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset)/ scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            SDL_Rect src_rect = {direction * 32, 4 * 32, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            SDL_Rect src_rect = {128 + direction * 32, 0, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            XYPos pos = level_set->levels[placing_subcircuit_level]->getimage(direction);
            if (pos != XYPos(0,0))
            {
                SDL_Rect src_rect = {pos.x, pos.y, 32, 32};
                SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                render_texture(src_rect, dst_rect);
            }


            pos = level_set->levels[placing_subcircuit_level]->getimage_fg(direction);
            if (pos != XYPos(0,0))
            {
                SDL_Rect src_rect = {pos.x, pos.y, 24, 24};
                SDL_Rect dst_rect = {(mouse_grid.x * 32 + 4) * scale + grid_offset.x, (mouse_grid.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
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
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect dst_rect = {tl.x * scale + grid_offset.x, (tl.y + size.y) * scale + grid_offset.y, (size.x + 1) * scale, 1 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect dst_rect = {tl.x * scale + grid_offset.x, tl.y * scale + grid_offset.y, 1 * scale, size.y * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect dst_rect = {(tl.x + size.x) * scale + grid_offset.x, tl.y * scale + grid_offset.y, 1 * scale, (size.y + 1) * scale};
                render_texture(src_rect, dst_rect);
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
            render_texture(src_rect, dst_rect);
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
        if (vented > 20)
        {
            SDL_Rect src_rect = {16*int(rand & 3) + 256, 160, 16, 16};
            SDL_Rect dst_rect = {(pos.x * 32  - 9) * scale + int(rand % 3 * scale) + grid_offset.x, (pos.y * 32 + 7 ) * scale + int(rand % 3 * scale) + grid_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        render_number_2digit(XYPos((pos.x * 32  - 5) * scale + grid_offset.x, (pos.y * 32 + 13) * scale + grid_offset.y), value, 1, 6);
    }
    
    for (auto i = current_circuit->signs.rbegin(); i != current_circuit->signs.rend(); ++i)     // show notes
    {
        Sign& sign = *i;
        sign.set_size(get_text_size(sign.text));
        render_box(sign.get_pos() * scale + grid_offset , sign.get_size(), 4);
        {
            SDL_Rect src_rect = {288 + (int)sign.direction * 16, 176, 16, 16};
            SDL_Rect dst_rect = {(sign.pos.x - 8) * scale + grid_offset.x, (sign.pos.y - 8) * scale + grid_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        render_text(sign.get_pos() + XYPos(32,32) + XYPos(4,4), sign.text.c_str());
    }
    if (mouse_state == MOUSE_STATE_ENTERING_TEXT_INTO_SIGN && (frame_index % 60 < 30))
    {
        Sign& sign = current_circuit->signs.front();
        std::string text = sign.text;
        text.append("\u258f");
        render_text(sign.get_pos() + XYPos(32,32) + XYPos(4,4), text.c_str());
    }

    
    if (mouse_state == MOUSE_STATE_PLACING_SIGN)
    {
        XYPos mouse_grid = (mouse - grid_offset) / scale;
        Sign sign(mouse_grid, direction, "");
        sign.set_size(get_text_size(sign.text));
        render_box(sign.get_pos() * scale + grid_offset , sign.get_size(), 4);
        {
            SDL_Rect src_rect = {288 + (int)sign.direction * 16, 176, 16, 16};
            SDL_Rect dst_rect = {(sign.pos.x - 8) * scale + grid_offset.x, (sign.pos.y - 8) * scale + grid_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        render_text(sign.get_pos() + XYPos(32,32) + XYPos(4,4), sign.text.c_str());
    }

    if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
    {
        XYPos mouse_grid = (mouse - grid_offset) / scale;
        dragged_sign.set_size(get_text_size(dragged_sign.text));
        render_box((dragged_sign.get_pos() + mouse_grid) * scale + grid_offset , dragged_sign.get_size(), 4);
        {
            SDL_Rect src_rect = {288 + (int)dragged_sign.direction * 16, 176, 16, 16};
            SDL_Rect dst_rect = {(dragged_sign.pos.x + mouse_grid.x - 8) * scale + grid_offset.x, (dragged_sign.pos.y + mouse_grid.y - 8) * scale + grid_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        render_text(dragged_sign.get_pos() + mouse_grid + XYPos(32,32) + XYPos(4,4), dragged_sign.text.c_str());
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
                case PANEL_STATE_SCORES:
                    panel_colour = 6;
                    break;
                default:
                    assert(0);
            }
            
            render_box(XYPos((32 * 11) * scale, 0), XYPos(8*32 + 16, 11*32 + 8), panel_colour);
        }
        {                                                                                               // Top Menu
            bool flash_next_level = level_set->is_playable(next_dialogue_level) && (frame_index % 60 < 30);
            render_button(XYPos((8 + 32 * 11) * scale, 8 * scale), XYPos(256 + (flash_next_level ? 24 : 0), 112), panel_state == PANEL_STATE_LEVEL_SELECT);
            if (level_set->is_playable(1) && (!flash_editor_menu || (current_level_index != 1) ||(frame_index % 60 < 30) || show_dialogue))
                render_button(XYPos((8 + 32 * 12) * scale, 8 * scale), XYPos(256+24*2, 112), panel_state == PANEL_STATE_EDITOR);
            if (level_set->is_playable(3))
                render_button(XYPos((8 + 32 * 13) * scale, 8 * scale), XYPos(256+24*3, 112), panel_state == PANEL_STATE_MONITOR);
            if (level_set->is_playable(7))
                render_button(XYPos((8 + 32 * 14) * scale, 8 * scale), XYPos(256+24*4, 112), panel_state == PANEL_STATE_TEST);
            render_box(XYPos((8 + 32 * 15) * scale, (8) * scale), XYPos(32, 32), 0);
            render_box(XYPos((8 + 32 * 16) * scale, (8) * scale), XYPos(64, 32), 3);
        }
        {                                                                                               // Speed arrows
            SDL_Rect src_rect = {256, 136, 53, 5};
            SDL_Rect dst_rect = {(8 + 32 * 16 + 6) * scale, (8 + 6) * scale, 53 * scale, 5 * scale};
            render_texture(src_rect, dst_rect);
        }
        {                                                                                               // Speed slider
            SDL_Rect src_rect = {624, 16, 16, 16};
            SDL_Rect dst_rect = {(8 + 32 * 11 + 32 * 5 + int(game_speed)) * scale, (8 + 8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        {                                                                                               // Current Score
            render_number_2digit(XYPos((8 + 32 * 11 + 32 * 4 + 3) * scale, (8 + 8) * scale), pressure_as_percent(current_level->best_score), 3);
        }
        {                                                                                               // Help Button
            render_button(XYPos((8 + 32 * 11 + 7 * 32) * scale, (8) * scale), XYPos(256+24*5, 112), show_help);
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
            if (next_dialogue_level == level_index && (frame_index % 60 < 30) && !show_dialogue)
                continue;
            render_button(XYPos(pos.x * 32 * scale + panel_offset.x, pos.y * 32 * scale + panel_offset.y), level_set->levels[level_index]->getimage_fg(DIRECTION_N), level_index == current_level_index ? 1 : 0);
            
            unsigned score = pressure_as_percent(level_set->levels[level_index]->best_score);

            render_number_2digit(XYPos((pos.x * 32 + 32 - 9 - 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), score);
            level_index++;
        }
            render_button(XYPos(panel_offset.x, panel_offset.y + 144 * scale), XYPos(304, 256), 0);
        {
            SDL_Rect src_rect = {show_hint * 256, int(current_level_index) * 128, 256, 128};
            SDL_Rect dst_rect = {panel_offset.x, panel_offset.y + 176 * scale, 256 * scale, 128 * scale};
            render_texture_custom(sdl_levels_texture, src_rect, dst_rect);
        }


        XYPos panel_pos = ((mouse - panel_offset) / scale);                 // Tooltip
        if (panel_pos.y >= 0 && panel_pos.x >= 0)
        {
            XYPos panel_grid_pos = panel_pos / 32;
            int level_index = panel_grid_pos.x + panel_grid_pos.y * 8;

            if (level_set->is_playable(level_index))
            {
                SDL_Rect src_rect = {192, 184 + level_index * 24, 64, 16};
                SDL_Rect dst_rect = {(panel_pos.x - 64)* scale + panel_offset.x, panel_pos.y * scale + panel_offset.y, 64 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }

        }


    } else if (panel_state == PANEL_STATE_EDITOR)
    {
        pos = XYPos(0,0);
        bool flasher = (frame_index % 50) < 25;

        if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
            flash_steam_inlet = false;
        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
            flash_valve = false;

        if (level_set->is_playable(2))
            render_button(XYPos(panel_offset.x + 0 * 32 * scale, panel_offset.y), XYPos(544 + direction * 24, 160), mouse_state == MOUSE_STATE_PLACING_VALVE || (flasher && flash_valve && (current_level_index == 2)));
        render_button(XYPos(panel_offset.x + 1 * 32 * scale, panel_offset.y), XYPos(544 + direction * 24, 160 + 24), mouse_state == MOUSE_STATE_PLACING_SOURCE || (flasher && flash_steam_inlet));

        render_button(XYPos(panel_offset.x + 2 * 32 * scale, panel_offset.y), XYPos(400, 112), 0);
        render_button(XYPos(panel_offset.x + 3 * 32 * scale, panel_offset.y), XYPos(400+24, 112), 0);
        
        
        if (level_set->is_playable(8))
            render_button(XYPos(panel_offset.x + 7 * 32 * scale, panel_offset.y), XYPos(544 + direction * 24, 160 + 48), mouse_state == MOUSE_STATE_PLACING_SIGN);


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
                 render_texture(src_rect, dst_rect);
            XYPos level_pos = level_set->levels[level_index]->getimage_fg(direction);
            src_rect = {level_pos.x, level_pos.y, 24, 24};
            dst_rect = {(pos.x * 32 + 4) * scale + panel_offset.x, (32 + 8 + pos.y * 32 + 4) * scale + panel_offset.y, 24 * scale, 24 * scale};
            render_texture(src_rect, dst_rect);
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
                render_texture(src_rect, dst_rect);
            }
            panel_grid_pos = (panel_pos - XYPos(0,8)) / 32;
            int level_index = panel_grid_pos.x + (panel_grid_pos.y - 1) * 8;

            if (level_set->is_playable(level_index))
            {
                SDL_Rect src_rect = {192, 184 + level_index * 24, 64, 16};
                SDL_Rect dst_rect = {(panel_pos.x - 64)* scale + panel_offset.x, panel_pos.y * scale + panel_offset.y, 64 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
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
//                     render_texture(src_rect, dst_rect);
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
//                     render_texture(src_rect, dst_rect);
//                 }
//             }
//             else
//             {
//                 render_box(XYPos(panel_offset.x, mon_offset * scale + panel_offset.y), XYPos(7*32, 48), mon_index);
//                 {
//                     SDL_Rect src_rect = {503, 86, 1, 1};
//                     SDL_Rect dst_rect = {5 * scale + panel_offset.x, (mon_offset + 6) * scale + panel_offset.y, 200 * scale, 35 * scale};
//                     render_texture(src_rect, dst_rect);
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
//                     render_texture(src_rect, dst_rect);
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
//                     render_texture(src_rect, dst_rect);
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
//                         render_texture(src_rect, dst_rect);
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
//                             render_texture(src_rect, dst_rect);
//                         }
//                         if (!current_point || (frame_index % 20 < 10))
//                         {
//                             SDL_Rect src_rect = {503, 80, 1, 1};
//                             SDL_Rect dst_rect = {(x * int(width) + 6) * scale + panel_offset.x, (mon_offset + 6 + offset) * scale + panel_offset.y, int(width) * scale, 1 * scale};
//                             render_texture(src_rect, dst_rect);
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
                render_texture(src_rect, dst_rect);
            }

            {
                SDL_Rect src_rect = {448, 112, 48, 16};
                SDL_Rect dst_rect = {(port_index * 48) * scale + panel_offset.x, (101 + 14) * scale + panel_offset.y, 48 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            
            {
                SDL_Rect src_rect = {256 + 80 + (port_index * 6 * 16) , 16, 16, 16};
                SDL_Rect dst_rect = {(port_index * 48 + 8) * scale + panel_offset.x, (101 - int(test_value[port_index])) * scale + panel_offset.y, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            render_number_2digit(XYPos((port_index * 48 + 8 + 3 ) * scale + panel_offset.x, ((101 - test_value[port_index]) + 5) * scale + panel_offset.y), test_value[port_index]);
            
            {
                SDL_Rect src_rect = {256 + 80 + (port_index * 6 * 16) , 16, 16, 16};
                SDL_Rect dst_rect = {(port_index * 48 + int(test_drive[port_index])) * scale + panel_offset.x, (101 + 16 + 7) * scale + panel_offset.y, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
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
                render_texture(src_rect, dst_rect);
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
                        render_texture(src_rect, dst_rect);
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
            render_texture(src_rect, dst_rect);
        }
        
        {
            SDL_Rect src_rect = {336, 32, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + (0) * scale, panel_offset.y + (32 + 8 + 16) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }

        for (int i = 0; i < test_count; i++)
        {
            SDL_Rect src_rect = {272, 16, 16, 16};
            if (i == test_index)
                src_rect.x = 368;
            else if (i < test_index && monitor_state == MONITOR_STATE_PLAY_ALL && !current_level->touched)
                src_rect.x = 368;
            SDL_Rect dst_rect = {panel_offset.x + (16 + i * 16) * scale, panel_offset.y + (32 + 8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
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
            render_texture(src_rect, dst_rect);
            y_pos+= 16 * scale;
        }
        for (int i = 0; i < 4; i++)                 // Inputs
        {
            int pin_index = current_level->pin_order[i];
            if (((current_level->connection_mask >> pin_index) & 1) && pin_index != current_level->tests[test_index].tested_direction)
            {
            
                SDL_Rect src_rect = {256 + pin_index * 16, 144, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
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
            render_texture(src_rect, dst_rect);
            y_pos+= 16 * scale;
        }

        {                                       // Output
            int pin_index = current_level->tests[test_index].tested_direction;
            SDL_Rect src_rect = {256 + pin_index * 16, 144, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
            unsigned value = current_level->tests[test_index].sim_points[sim_point_count-1].values[pin_index];
            render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + (sim_point_count-1) * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == sim_point_count - 1 ? 4 : 0);
            render_number_pressure(XYPos(panel_offset.x + (8 + 16 + 3 + 16 + (sim_point_count-1) * 16) * scale, y_pos + (5) * scale), current_level->tests[test_index].last_pressure_log[HISTORY_POINT_COUNT - 1] , 1, 9, 1);
            y_pos += 16 * scale;
        }
        {
            SDL_Rect src_rect = {320, 144, 16, 8};
            SDL_Rect dst_rect = {panel_offset.x + int(8 + 16 + current_level->sim_point_index * 16) * scale, panel_offset.y + (32 + 32 + 16) * scale, 16 * scale, 8 * scale};
            render_texture(src_rect, dst_rect);
            src_rect.y += 8;
            src_rect.h = 1;
            dst_rect.y += 8 * scale;
            dst_rect.h = y_pos - dst_rect.y - 8 * scale;
            render_texture(src_rect, dst_rect);
            src_rect.h = 8;
            dst_rect.y = y_pos - 8 * scale;
            dst_rect.h = 8 * scale;
            render_texture(src_rect, dst_rect);
        }


        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(256, 120), 5);
        {
            XYPos graph_pos(8 * scale + panel_offset.x, (32 + 32 + 8 + 112 + 9) * scale + panel_offset.y);
            {
                int target_value = current_level->tests[test_index].sim_points.back().values[current_level->tests[test_index].tested_direction];
                int pos = 100 - target_value;
                SDL_Rect src_rect = {503, 83, 1, 1};
                SDL_Rect dst_rect = {graph_pos.x, (100 - target_value) * scale + graph_pos.y, (HISTORY_POINT_COUNT - 1) * scale, 1 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {524, 80, 13, 101};
                SDL_Rect dst_rect = {graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
                render_texture(src_rect, dst_rect);
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
                    render_texture(src_rect, dst_rect);
                }
                for (int i = 0; i < int(current_level->tests[test_index].last_pressure_index) - 1; i++)
                {
                    int v1 = pressure_as_percent(current_level->tests[test_index].last_pressure_log[i]);
                    int v2 = pressure_as_percent(current_level->tests[test_index].last_pressure_log[i + 1]);
                    int top = 100 - std::max(v1, v2);
                    int size = abs(v1 - v2) + 1;

                    SDL_Rect src_rect = {503, 86, 1, 1};
                    SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                    render_texture(src_rect, dst_rect);
                }
            }

            
        }

    } else if (panel_state == PANEL_STATE_SCORES)
    {
        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(256, 120), 5);
        XYPos graph_pos(8 * scale + panel_offset.x, (32 + 32 + 8 + 112 + 9) * scale + panel_offset.y);
        {
            SDL_Rect src_rect = {524, 80, 13, 101};
            SDL_Rect dst_rect = {graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
            render_texture(src_rect, dst_rect);
        }


        if (current_level->global_score_graph_set)
        {
            Pressure my_score = current_level->global_fetched_score;
            for (int i = 0; i < 200-1; i++)
            {
                unsigned colour = 6;
                int v1 = pressure_as_percent(current_level->global_score_graph[i]);
                int v2 = pressure_as_percent(current_level->global_score_graph[i + 1]);
                if ((current_level->global_score_graph[i] >= my_score) && (current_level->global_score_graph[i + 1] <= my_score))
                    colour = 3;
                int top = 100 - std::max(v1, v2);
                int size = abs(v1 - v2) + 1;

                SDL_Rect src_rect = {503, 80 + (int)colour, 1, 1};
                SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                render_texture(src_rect, dst_rect);
            }
        }
        if (!current_level->global_score_graph_set || SDL_TICKS_PASSED(SDL_GetTicks(), current_level->global_score_graph_set + 1000 * 60))
        {
            if (!scores_from_server.working)
                score_fetch(current_level_index);
        }
    }

    if (show_debug)
    {
        render_number_2digit(XYPos(0, 0), debug_last_second_frames, 3);
        render_number_long(XYPos(0, 3 * 7 * scale), debug_last_second_simticks, 3);
    }

    if (!dialogue[current_level_index][dialogue_index].text)
        show_dialogue = false;

#ifdef COMPRESSURE_DEMO
    if (current_level_index == (LEVEL_COUNT - 1) && dialogue_index)
        show_dialogue = false;
#endif

    if (show_dialogue)
    {
        const char* text = dialogue[current_level_index][dialogue_index].text;
        DialogueCharacter character = dialogue[current_level_index][dialogue_index].character;
#ifdef COMPRESSURE_DEMO
        if (current_level_index == (LEVEL_COUNT - 1))
        {
            text = "Alas our journey must stop somewhere. The adventure continues\nin the full game. Be sure to join our Discord group to drive\nits direction.\n\nTo be continued...";
            character = DIALOGUE_ADA;
        }
#endif

        bool pic_on_left = true;
        XYPos pic_src;
        switch (character)
        {
            case DIALOGUE_CHARLES:
                pic_on_left = true;
                pic_src = XYPos(0,0);
                break;
            case DIALOGUE_NICOLA:
                pic_on_left = false;
                pic_src = XYPos(1,0);
                break;
            case DIALOGUE_ADA:
                pic_on_left = true;
                pic_src = XYPos(1,1);
                break;
            case DIALOGUE_ANNIE:
                pic_on_left = true;
                pic_src = XYPos(0,1);
                break;
             case DIALOGUE_GEORGE:
                pic_on_left = true;
                pic_src = XYPos(1,2);
                break;
             case DIALOGUE_JOSEPH:
                pic_on_left = true;
                pic_src = XYPos(0,2);
                break;
       }
        render_box(XYPos(16 * scale, (180 + 16) * scale), XYPos(640-32, 180-32), 4);
        
        render_box(XYPos((pic_on_left ? 16 : 640 - 180 + 16) * scale, (180 + 16) * scale), XYPos(180-32, 180-32), 0);
        SDL_Rect src_rect = {640-256 + pic_src.x * 128, 480 + pic_src.y * 128, 128, 128};
        SDL_Rect dst_rect = {pic_on_left ? 24 * scale : (640 - 24 - 128) * scale, (180 + 24) * scale, 128 * scale, 128 * scale};
        render_texture(src_rect, dst_rect);
        render_text_wrapped(XYPos(pic_on_left ? 48 + 128 : 24, 180 + 24), text, 640 - 80 - 128);

    }
    
    if (level_win_animation)
    {
        int size = 360 - level_win_animation * 3;
        SDL_Rect src_rect = {336, 32, 16, 16};
        SDL_Rect dst_rect = {(320 - size / 2) * scale, (180 - size / 2) * scale, size * scale, size * scale};
        render_texture(src_rect, dst_rect);
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
            render_texture(src_rect, dst_rect);
        }

        {
            SDL_Rect src_rect = {464, 144, 16, 16};
            SDL_Rect dst_rect = {(32 + 512 + 32 + 8) * scale, (8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
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
                {XYPos(0,14), 4, 1, "In the level select menu, the bottom panel describes the design requirements. Each design has four ports and the requirements state the expected output in terms of other ports. Each port has a colour identifier. Click on the requirements to recieve a hint."},
                {XYPos(0,13),5,0.5, "Once you achieve a score of 75 or more, the next design becomes available. You can always come back to refine your solution.\n\nPress the pipe button below to continue the tutorial. You can return to the help by pressing F1."},
                {XYPos(0,0), 10, 1, "Pipes can be laid down by either left mouse button dragging the pipe from the source to the desination, or by clicking left mouse button to extend the pipe in the direction of the mouse. Right click to cancel pipe laying mode."},
                {XYPos(0,2),  5, 1, "Hold the right mouse button to delete pipes and other elements."},
                {XYPos(0,4), 15, 1, "The build menu allows you to add components into your design. Select the steam inlet component and hover over your design. The arrow buttons change the orientation. This can also be done using keys Q and E or the mouse scroll wheel. Clicking the left mouse button will place the component. Right click to exit steam inlet placing mode."},
                {XYPos(0,8),  5, 1, "Valves can be placed in the same way. Pressing Tab, or middle mouse button, is a shortcut to enter valve placement mode. Pressing Tab, or middle mouse button, again switches to steam inlet placement."},
                {XYPos(0,7),  5,10, "A steam inlet will supply steam at pressure 100. Any pipes with open ends will vent the steam to the atmosphere at pressure 0."},
                {XYPos(1,10), 4,10, "Pressure at different points is visible on pipe connections. Note how each pipe has a little resistance."},
                {XYPos(0,9),  5,10, "Valves allow steam to pass through them if the (+) side or the valve is at a higher pressure than the (-) side. The higher it is, the more the valve is open. Steam on the (+) and (-) sides is not consumed. Here, the (-) side is vented to atmosphere and thus at 0 pressure."},
                {XYPos(0,10), 1, 1, "If the pressure on the (+) side is equal or lower than the (-) side, the valve becomes closed and no steam will pass through."},
                {XYPos(0,11), 5,10, "By pressurising (+) side with a steam inlet, the valve will become open only if the pressure on the (-) side is low."},
                {XYPos(0,12), 1, 1, "Applying high pressure to the (-) side will close the valve."},
                {XYPos(1,12), 1, 1, "The test menu allows you to inspect how well your design is performing. The first three buttons pause the testing, repeatedly run a single test and run all tests respectively.\n\n"
                                    "The current scores for individual tests are shown with the scores of best design seen below. On the right is the final score formed from the average of all tests.\n\n"
                                    "The next panel shows the sequence of inputs and expected outputs for the current test. The current phase is highlighted. The output recorded on the last run is shown to the right."},
                {XYPos(2,12), 3, 1, "The score is based on how close the output is to the target value. The graph shows the output value during the final stage of the test. The faded line in the graph shows the path of the best design so far."},
                {XYPos(4,14), 1, 1, "The experiment menu allows you to manually set the ports and examine your design's operation. The vertical sliders set the desired value. The horizontal sliders below set force of the input. Setting the slider all the way left makes it an output. Initial values are set from the current test."},
                {XYPos(0,15), 1, 1, "The graph at the bottom shows the history of the port values."},
                
                {XYPos(1,15), 4, 1, "Components can be selected by either clicking while holding Ctrl, or dragging while holding Shift. Selected components can be moved using WASD keys if the destination is empty. To delete selected components, press X or Delete.\n\nUndo is reached through Z key (Ctrl is optional) and Redo through either Y or Shift+Z. Undo can also be triggered by holding right mouse button and clicking the left one."},
                {XYPos(0,16), 1, 1, "Pressing Esc shows the game menu. The buttons allow you to exit the game, switch between windowed and full screen, join our Discord group and show credits.\n\nThe sliders adjust the sound effects and music volumes."},
                
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
            render_texture_custom(sdl_tutorial_texture, src_rect, dst_rect);
            
            render_text_wrapped(XYPos(32 + 128 + 16 + i * 48, i * (128 +16) + 16), page->text, 384 - 16);
        }
    }

    if (show_main_menu)
    {
        render_box(XYPos(160 * scale, 90 * scale), XYPos(320, 180), 0);
        if (!display_about)
        {
            render_box(XYPos((160 + 32) * scale, (90 + 32)  * scale), XYPos(32, 32), 0);
            {
                SDL_Rect src_rect = {448, 200, 24, 24};
                SDL_Rect dst_rect = {(160 + 32 + 4) * scale, (90 + 32 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);

            }
            render_box(XYPos((160 + 32 + 64) * scale, (90 + 32)  * scale), XYPos(32, 32), 0);
            {
                SDL_Rect src_rect = {full_screen ? 280 : 256, 280, 24, 24};
                SDL_Rect dst_rect = {(160 + 32 + 64 + 4) * scale, (90 + 32 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
            }

            render_box(XYPos((160 + 32) * scale, (90 + 32 + 64)  * scale), XYPos(32, 32), 0);
            {
                SDL_Rect src_rect = {256, 256, 24, 24};
                SDL_Rect dst_rect = {(160 + 32 + 4) * scale, (90 + 32 + 64 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);

            }

            render_box(XYPos((160 + 32+ 64) * scale, (90 + 32 + 64)  * scale), XYPos(32, 32), 0);
            {
                SDL_Rect src_rect = {256+24, 256, 24, 24};
                SDL_Rect dst_rect = {(160 + 32+ 64 + 4) * scale, (90 + 32 + 64 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);

            }

            render_box(XYPos((160 + 32 + 128) * scale, (90 + 32)  * scale), XYPos(32, 128), 1);
            {
                SDL_Rect src_rect = {496, 200, 24, 24};
                SDL_Rect dst_rect = {(160 + 32 + 128 + 4) * scale, (90 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
            }

            {
                SDL_Rect src_rect = {526, 80, 12, 101};
                SDL_Rect dst_rect = {(160 + 32 + 128 + 16) * scale, (90 + 32 + 6 + 6)  * scale, 12 * scale, 101 * scale};
                render_texture(src_rect, dst_rect);
            }

            {
                SDL_Rect src_rect = {256 + 80 + 96, 16, 16, 16};
                SDL_Rect dst_rect = {(160 + 32 + 128 + 4) * scale, (90 + 32 + 6 + int(100 - sound_volume))  * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }

            render_box(XYPos((160 + 32 + 192) * scale, (90 + 32)  * scale), XYPos(32, 128), 2);
            {
                SDL_Rect src_rect = {520, 200, 24, 24};
                SDL_Rect dst_rect = {(160 + 32 + 192 + 4) * scale, (90 + 4)  * scale, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {526, 80, 12, 101};
                SDL_Rect dst_rect = {(160 + 32 + 192 + 16) * scale, (90 + 32 + 6 + 6)  * scale, 12 * scale, 101 * scale};
                render_texture(src_rect, dst_rect);
            }

            {
                SDL_Rect src_rect = {256 + 80 + 192, 16, 16, 16};
                SDL_Rect dst_rect = {(160 + 32 + 192 + 4) * scale, (90 + 32 + 6 + int(100 - music_volume))  * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
        }
        else
        {
            const char* about_text = "Created by Charlie Brej\n\nMusic by stephenpalmermail\n\nGraphic assets by Carl Olsson";
            render_text_wrapped(XYPos(160 + 32 + 4, 90 + 32 + 4), about_text, 320-64);

        
        }


    }





    SDL_RenderPresent(sdl_renderer);
}

void GameState::set_level(unsigned level_index)
{
    if (level_set->is_playable(level_index))
    {
        mouse_state = MOUSE_STATE_NONE;
        if (current_circuit)
            current_circuit->retire();
        current_level_index = level_index;
        current_level = level_set->levels[current_level_index];
        current_circuit = current_level->circuit;
        current_level->reset(level_set);
        show_hint = false;
        selected_elements.clear();
        if (level_index == next_dialogue_level)
        {
            show_dialogue = true;
            dialogue_index = 0;
            next_dialogue_level++;
        }
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
        if (current_circuit->blocked[grid.y][grid.x])
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

        for (auto it = current_circuit->signs.begin(); it != current_circuit->signs.end(); it++)
        {
            Sign& sign = *it;
            if ((pos - sign.get_pos()).inside(sign.get_size()))
            {
                mouse_state = MOUSE_STATE_DRAGGING_SIGN;
                dragged_sign_motion = false;
                dragged_sign = sign;
                dragged_sign.pos -= pos;
                current_circuit->ammend();
                current_circuit->signs.erase(it);
                return;
            }
        }

        if (grid.x > 8)
            grid.x = 8;
        if (grid.y > 8)
            grid.y = 8;
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

        pipe_drag_list.clear();

        if (pipe_start_ns)
        {
            if (current_circuit->is_blocked(pipe_start_grid_pos) && current_circuit->is_blocked(pipe_start_grid_pos - XYPos(0,1)))
                return;
            pipe_drag_list.push_back(XYPos(pipe_start_grid_pos));
            pipe_drag_list.push_back(XYPos(pipe_start_grid_pos) - XYPos(0,1));
        }
        else
        {
            if (current_circuit->is_blocked(pipe_start_grid_pos) && current_circuit->is_blocked(pipe_start_grid_pos - XYPos(1,0)))
                return;
            pipe_drag_list.push_back(XYPos(pipe_start_grid_pos));
            pipe_drag_list.push_back(XYPos(pipe_start_grid_pos) - XYPos(1,0));
        }
        pipe_dragged = false;
        mouse_state = MOUSE_STATE_PIPE_DRAGGING;
    }
    else if (mouse_state == MOUSE_STATE_PIPE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        XYPos mouse_rel = ((mouse - grid_offset) / scale) - (pipe_start_grid_pos * 32);
        if (pipe_start_ns)
        {
            mouse_rel.x -= 16;
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_circuit->blocked[pipe_start_grid_pos.y - 1][pipe_start_grid_pos.x]))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
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
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x - 1]))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_circuit->blocked[pipe_start_grid_pos.y][pipe_start_grid_pos.x]))
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
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            current_circuit->set_element_valve(mouse_grid, direction);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            current_circuit->set_element_source(mouse_grid, direction);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
            current_circuit->set_element_subcircuit(mouse_grid, direction, placing_subcircuit_level, level_set);
            current_level->touched = true;
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SIGN)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale);
        if (mouse_grid.inside(XYPos(9*32,9*32)))
        {
            Sign sign(mouse_grid, direction, "");
            current_circuit->add_sign(sign);
        }
    }
    else if (mouse_state == MOUSE_STATE_DELETING)
    {
        current_circuit->undo(level_set);
        first_deletion = true;
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
                    flash_editor_menu = false;
                    break;
                case 2:
                    if (!level_set->is_playable(3))
                        break;
                    panel_state = PANEL_STATE_MONITOR;
                    break;
                case 3:
                    if (!level_set->is_playable(7))
                        break;
                    panel_state = PANEL_STATE_TEST;
                    break;
                case 4:
                    panel_state = PANEL_STATE_SCORES;
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
        }
        else if (panel_pos.y > 176)
        {
            show_hint = true;
        }
        else if ((panel_pos - XYPos(0,144)).inside(XYPos(32,32)))
        {
            show_dialogue = true;
            dialogue_index = 0;
            
        }
        return;
    } else if (panel_state == PANEL_STATE_EDITOR)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        if (panel_grid_pos.y == 0)
        {
            if (panel_grid_pos.x == 0 && level_set->is_playable(2))
                mouse_state = MOUSE_STATE_PLACING_VALVE;
            else if (panel_grid_pos.x == 1)
                mouse_state = MOUSE_STATE_PLACING_SOURCE;
            else if (panel_grid_pos.x == 2)
                direction = Direction((int(direction) + 4 - 1) % 4);
            else if (panel_grid_pos.x == 3)
                direction = Direction((int(direction) + 1) % 4);
            else if (panel_grid_pos.x == 7)
                mouse_state = MOUSE_STATE_PLACING_SIGN;
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
        XYPos subtest_pos = panel_pos - XYPos(8 + 16, 32 + 32 + 16);
        if (subtest_pos.inside(XYPos(current_level->tests[current_level->test_index].sim_points.size() * 16, popcount(current_level->connection_mask) * 16 + 32)))
        {
            skip_to_subtest_index = subtest_pos.x / 16;
            skip_to_next_subtest = true;
            monitor_state = MONITOR_STATE_PLAY_1;
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
    if (mouse_state == MOUSE_STATE_PIPE_DRAGGING)
    {
//        if (!((mouse - grid_offset) / scale).inside(XYPos(9*32,9*32)))
//            return;
        XYPos grid = (((mouse - grid_offset) / scale) + XYPos(32,32)) / 32 - XYPos(1,1);

        XYPos last_pos = pipe_drag_list.back();
        XYPos direction =  grid - last_pos;
        XYPos next_pos = last_pos;

        if (grid  == last_pos)
            return;
        if (direction.x > 0) next_pos += XYPos(1,0);
        else if (direction.x < 0) next_pos += XYPos(-1,0);
        else if (direction.y > 0) next_pos += XYPos(0,1);
        else if (direction.y < 0) next_pos += XYPos(0,-1);
        else assert(0);
        
        
        std::list<XYPos>::reverse_iterator rit = pipe_drag_list.rbegin();
        rit++;
        if (next_pos == *rit)
        {
            if (pipe_drag_list.size() == 2)
            {
                pipe_drag_list.pop_front();
                pipe_drag_list.push_back(next_pos);
                return;
            }
            pipe_drag_list.pop_back();
            return;
        }
        if (current_circuit->is_blocked(last_pos))
            return;
        pipe_dragged = true;
        pipe_drag_list.push_back(next_pos);
    }

    if (mouse_state == MOUSE_STATE_DELETING)
    {
        XYPos pos = ((mouse - grid_offset) / scale);
        for (auto it = current_circuit->signs.begin(); it != current_circuit->signs.end(); it++)
        {
            Sign& sign = *it;
            if ((pos - sign.get_pos()).inside(sign.get_size()))
            {
                current_circuit->signs.erase(it);
                return;
            }
        }

        if (!((mouse - grid_offset) / scale).inside(XYPos(9*32,9*32)))
            return;
        XYPos grid = ((mouse - grid_offset) / scale) / 32;
        if (grid.x >= 9 || grid.y >= 9)
            return;
        if (current_circuit->is_blocked(grid))
            return;
        if (current_circuit->elements[grid.y][grid.x]->is_empty())
            return;

        current_circuit->set_element_empty(grid, !first_deletion);
        first_deletion = false;
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
                        mouse_state = MOUSE_STATE_NONE;
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
                        if (!SDL_IsTextInputActive())
                            direction = Direction((int(direction) + 4 - 1) % 4);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(false);
                        break;
                    case SDL_SCANCODE_E:
                        if (!SDL_IsTextInputActive())
                            direction = Direction((int(direction) + 1) % 4);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(true);
                        break;
                    case SDL_SCANCODE_W:
                        if (!SDL_IsTextInputActive())
                            current_circuit->move_selected_elements(selected_elements, DIRECTION_N);
                        break;
                    case SDL_SCANCODE_A:
                        if (!SDL_IsTextInputActive())
                            current_circuit->move_selected_elements(selected_elements, DIRECTION_W);
                        break;
                    case SDL_SCANCODE_S:
                        if (!SDL_IsTextInputActive())
                            current_circuit->move_selected_elements(selected_elements, DIRECTION_S);
                        break;
                    case SDL_SCANCODE_D:
                        if (!SDL_IsTextInputActive())
                            current_circuit->move_selected_elements(selected_elements, DIRECTION_E);
                        break;
                    case SDL_SCANCODE_X:
                        if (!SDL_IsTextInputActive())
                        {
                            current_circuit->delete_selected_elements(selected_elements);
                            selected_elements.clear();
                        }
                        break;
                    case SDL_SCANCODE_DELETE:
                        if (!SDL_IsTextInputActive())
                        {
                            current_circuit->delete_selected_elements(selected_elements);
                            selected_elements.clear();
                        }
                        break;
                    case SDL_SCANCODE_BACKSPACE:
                        if (!SDL_IsTextInputActive())
                        {
                            current_circuit->delete_selected_elements(selected_elements);
                            selected_elements.clear();
                        }
                        else
                        {
                            if (mouse_state == MOUSE_STATE_ENTERING_TEXT_INTO_SIGN)
                            {
                                Sign& sign = current_circuit->signs.front();
                                while (!sign.text.empty() && is_leading_utf8_byte(sign.text.back()))
                                {
                                    sign.text.pop_back();
                                }
                                if (!sign.text.empty())
                                    sign.text.pop_back();
                                break;
                            }
                        }
                        break;
                    case SDL_SCANCODE_RETURN:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT_INTO_SIGN)
                        {
                            Sign& sign = current_circuit->signs.front();
                            sign.text.append("\n");
                        }
                        break;
                    case SDL_SCANCODE_Z:
                        if (!SDL_IsTextInputActive())
                        {
                            if (!keyboard_shift)
                                current_circuit->undo(level_set);
                            else
                                current_circuit->redo(level_set);
                        }
                        break;
                    case SDL_SCANCODE_Y:
                        if (!SDL_IsTextInputActive())
                            current_circuit->redo(level_set);
                        break;
                    case SDL_SCANCODE_PAGEUP:
                        if (!SDL_IsTextInputActive())
                        {
                            if (level_set->is_playable(current_level_index + 1))
                                set_level(current_level_index + 1);
                            else
                                set_level(0);
                        }
                        break;
                    case SDL_SCANCODE_PAGEDOWN:
                        if (!SDL_IsTextInputActive())
                        {
                            if (current_level_index)
                                set_level(current_level_index - 1);
                            else
                                set_level(level_set->top_playable());
                        }
                        break;
                    case SDL_SCANCODE_F1:
                        show_help = !show_help;
                        break;
                    case SDL_SCANCODE_F5:
                        show_debug = !show_debug;
                        break;
                    case SDL_SCANCODE_F11:
                        full_screen = !full_screen;
                        SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        SDL_SetWindowBordered(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                        SDL_SetWindowInputFocus(sdl_window);
                        break;
                    case SDL_SCANCODE_LSHIFT:
                        keyboard_shift = true;
                        break;
                    case SDL_SCANCODE_LCTRL:
                        keyboard_ctrl = true;
                        break;
                    case SDL_SCANCODE_SPACE:
                        if (!SDL_IsTextInputActive())
                        {
                            skip_to_next_subtest = true;
                            skip_to_subtest_index = -1;
                        }
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
            case SDL_TEXTINPUT:
            {
                if (mouse_state == MOUSE_STATE_ENTERING_TEXT_INTO_SIGN)
                {
                    Sign& sign = current_circuit->signs.front();
                    sign.text.append(e.text.text);
                    break;
                }
            }
            case SDL_MOUSEMOTION:
            {
                dragged_sign_motion = true;
                mouse.x = e.motion.x;
                mouse.y = e.motion.y;
                mouse -= screen_offset;
                mouse_motion();
                break;
            }
            case SDL_MOUSEBUTTONUP:
            {
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                mouse -= screen_offset;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (mouse_state == MOUSE_STATE_SPEED_SLIDER)
                        mouse_state = MOUSE_STATE_NONE;
                    if (mouse_state == MOUSE_STATE_PIPE_DRAGGING)
                    {
                        if (pipe_drag_list.size() == 2)
                        {
                            pipe_drag_list.clear();
                            if (!pipe_dragged)
                                mouse_state = MOUSE_STATE_PIPE;
                            else
                                mouse_state = MOUSE_STATE_NONE;
                            break;
                        }
                        current_circuit->add_pipe_drag_list(pipe_drag_list);
                        pipe_drag_list.clear();
                        mouse_state = MOUSE_STATE_NONE;
                    }
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
                            if (pos.x >= tl.x && pos.x <= br.x && pos.y >= tl.y && pos.y <= br.y && !current_circuit->elements[pos.y][pos.x]->is_empty() && !current_circuit->is_blocked(pos))
                                selected_elements.insert(pos);
                        }
                        mouse_state = MOUSE_STATE_NONE;
                    }
                    if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                    {
                        XYPos pos = (mouse - grid_offset) / scale;
                        Sign sign(dragged_sign.pos + pos, dragged_sign.direction, dragged_sign.text);
                        current_circuit->add_sign(sign, true);
                        if (dragged_sign_motion)
                            mouse_state = MOUSE_STATE_NONE;
                        else
                        {
                            mouse_state = MOUSE_STATE_ENTERING_TEXT_INTO_SIGN;
                            SDL_StartTextInput();
                        }
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
                if (mouse_state == MOUSE_STATE_ENTERING_TEXT_INTO_SIGN)
                {
                    mouse_state = MOUSE_STATE_NONE;
                    break;
                }
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                mouse -= screen_offset;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (show_main_menu)
                    {
                        if (display_about)
                        {
                            display_about = false;
                            break;
                        }
                        
                        XYPos pos = (mouse / scale) - XYPos((160 + 32), (90 + 32));
                        if (pos.inside(XYPos(32, 32)))
                            return true;
                        if ((pos - XYPos(0, 64)).inside(XYPos(32, 32)))
                        {
                            DisplayWebsite("https://discord.gg/7ZVZgA7gkS");
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 32)))
                        {
                            full_screen = !full_screen;
                            SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                            SDL_SetWindowBordered(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                            SDL_SetWindowInputFocus(sdl_window);
                        }
                        if ((pos - XYPos(0, 64)).inside(XYPos(32, 32)))
                        {
                            display_about = true;
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
                    else if (show_dialogue)
                    {
                        dialogue_index++;
                        if (!dialogue[current_level_index][dialogue_index].text)
                            show_dialogue = false;
                    }
                    else if (mouse.x < panel_offset.x)
                        if (e.button.clicks == 2)
                        {
//                             XYPos pos = (mouse - grid_offset) / scale;
//                             XYPos grid = pos / 32;
//                             if (grid.inside(XYPos(9,9)))
//                             {
//                                 Circuit* sub_circuit = current_circuit->elements[grid.y][grid.x]->get_subcircuit();
//                                 if (sub_circuit)
//                                 {
//                                     current_circuit = sub_circuit;
//                                     mouse_state = MOUSE_STATE_NONE;
//                                 }
//                             }
                            mouse_click_in_grid();
                        }
                        else
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
                        first_deletion = true;
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

                break;
            }
            default:
            {
//                printf("event:0x%x\n", e.type);
            
            
            }
        }
    }
    if (SDL_IsTextInputActive() && mouse_state != MOUSE_STATE_ENTERING_TEXT_INTO_SIGN)
        SDL_StopTextInput();

    return false;
}
