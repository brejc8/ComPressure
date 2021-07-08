#define _USE_MATH_DEFINES
#include <cmath>

#include "GameState.h"
#include "SaveState.h"
#include "Misc.h"

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
#include <codecvt>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <shellapi.h>
#endif

static void DisplayWebsite(const char* url)
{
#ifdef __linux__
    char buf[1024];
    snprintf(buf, sizeof(buf), "xdg-open %s", url);
    system(buf);
#else
    SDL_OpenURL(url);
#endif

}


void GameState::load_lang()
{
    delete languages;
    {
        std::string save_filename =  "lang.json";
        std::ifstream loadfile(save_filename);
        languages = SaveObject::load(loadfile)->get_map();
    }

    {
        char* save_path = SDL_GetPrefPath("CharlieBrej", "ComPressure");
        std::string save_filename = std::string(save_path) + "lang.json";
        SDL_free(save_path);

        std::ifstream loadfile(save_filename);
        if (!loadfile.fail() && !loadfile.eof())
        {
            delete languages;
            languages = SaveObject::load(loadfile)->get_map();
        }
    }
}

GameState::GameState(const char* filename)
{
    load_lang();
    bool load_was_good = false;
    try 
    {
        std::ifstream loadfile(filename);
        if (!loadfile.fail() && !loadfile.eof())
        {
            SaveObjectMap* omap;
            omap = SaveObject::load(loadfile)->get_map();
            level_set_accuracy = new LevelSet(omap->get_item("levels"));
            if (omap->has_key("levels_price"))
                level_set_price = new LevelSet(omap->get_item("levels_price"));
            else
                level_set_price = new LevelSet();
            
            if (omap->has_key("levels_steam"))
                level_set_steam = new LevelSet(omap->get_item("levels_steam"));
            else
                level_set_steam = new LevelSet();
            
            edited_level_set = level_set_accuracy;
            level_set = edited_level_set;

            next_dialogue_level = omap->get_num("next_dialogue_level");
            highest_level = next_dialogue_level - 1;
            if (omap->has_key("highest_level"))
                highest_level = omap->get_num("highest_level");
            current_level_index = omap->get_num("current_level_index");
            if (!edited_level_set->is_playable(current_level_index, highest_level))
                current_level_index = 0;
            game_speed = omap->get_num("game_speed");
            show_debug = omap->get_num("show_debug");
                
            show_help_page = omap->get_num("show_help_page");
            if (omap->has_key("next_help_highlight"))
                next_help_highlight = omap->get_num("next_help_highlight");
            else
                next_help_highlight = 12;
            flash_editor_menu = omap->get_num("flash_editor_menu");
            flash_steam_inlet = omap->get_num("flash_steam_inlet");
            flash_valve = omap->get_num("flash_valve");
//            requesting_help = omap->get_num("requesting_help");

            sound_volume = omap->get_num("sound_volume");
            music_volume = omap->get_num("music_volume");
            update_scale(omap->get_num("scale"));
            if (scale < 1)
                scale = 2;
            full_screen = omap->get_num("full_screen");
            minutes_played = omap->get_num("minutes_played");
            discord_joined = omap->get_num("discord_joined");
            if (omap->has_key("language"))
            	language_name = omap->get_string("language");
            if (omap->has_key("number_high_precision"))
                number_high_precision = omap->get_num("number_high_precision");

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
        level_set_accuracy = new LevelSet();
        level_set_price = new LevelSet();
        level_set_steam = new LevelSet();
        edited_level_set = level_set_accuracy;
        level_set = edited_level_set;
        current_level_index = 0;
        update_scale(2);
        display_language_dialogue = true;
    }

    if (!languages->has_key(language_name))
        language_name = "English";
    current_language = languages->get_item(language_name)->get_map();



    set_level(current_level_index);

    sdl_window = SDL_CreateWindow( "ComPressure", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*scale, 360*scale, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | (full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP  | SDL_WINDOW_BORDERLESS : 0));
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	sdl_texture = loadTexture("texture.png");
	sdl_tutorial_texture = loadTexture("tutorial.png");
	sdl_levels_texture = loadTexture("levels.png");
    SDL_SetRenderDrawColor(sdl_renderer, 0x0, 0x0, 0x0, 0xFF);
    
    {
        SDL_Surface* icon_surface = IMG_Load("icon.png");
        SDL_SetWindowIcon(sdl_window, icon_surface);
	    SDL_FreeSurface(icon_surface);
    }

    font = TTF_OpenFont("fixed.ttf", 19);

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    Mix_AllocateChannels(16);
    
    vent_steam_wav = Mix_LoadWAV("vent_steam.ogg");
    Mix_PlayChannel(0, vent_steam_wav, -1);
    Mix_Volume(0, 0);
    
    move_steam_wav = Mix_LoadWAV("move_steam.ogg");
    Mix_PlayChannel(1, move_steam_wav, -1);
    Mix_Volume(1, 0);
    
    Mix_VolumeMusic(music_volume);
    music = Mix_LoadMUS("music.ogg");
    Mix_PlayMusic(music, -1);
    
    {
        SDL_Rect dst_rect = {0, 0, 600, 600};
        SDL_SetTextInputRect(&dst_rect);
    }
    
    set_level_set(edited_level_set);
}
    
SaveObject* GameState::save(bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("levels", level_set_accuracy->save_all(highest_level, lite));
    omap->add_item("levels_price", level_set_price->save_all(highest_level, lite));
    omap->add_item("levels_steam", level_set_steam->save_all(highest_level, lite));

    omap->add_num("current_level_index", current_level_index);
    omap->add_num("game_speed", game_speed);
    omap->add_num("show_debug", show_debug);
    omap->add_num("highest_level", highest_level);
    omap->add_num("next_dialogue_level", next_dialogue_level);
    omap->add_num("show_help_page", show_help_page);
    omap->add_num("next_help_highlight", next_help_highlight);
 
    omap->add_num("flash_editor_menu", flash_editor_menu);
    omap->add_num("flash_steam_inlet", flash_steam_inlet);
    omap->add_num("flash_valve", flash_valve);
//    omap->add_num("requesting_help", requesting_help);
    omap->add_num("scale", scale);
    omap->add_num("full_screen", full_screen);
    omap->add_num("sound_volume", sound_volume);
    omap->add_num("music_volume", music_volume);
    omap->add_num("minutes_played", minutes_played + SDL_GetTicks()/ 1000 / 60);
    omap->add_num("discord_joined", discord_joined);
    omap->add_string("language", language_name);
    omap->add_num("number_high_precision", number_high_precision);
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
            SDL_AtomicUnlock(&comms->resp->working);
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
            SDL_AtomicUnlock(&comms->resp->working);
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
            got = 0;
            while (got != length)
            {
                int n = SDLNet_TCP_Recv(tcpsock, &data[got], length - got);
                got += n;
                if (!n)
                {
                    free (data);
                    throw(std::runtime_error("Connection closed early"));
                }
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
        if (comms->resp)
        {
            comms->resp->error = true;
        }
    }
    SDLNet_TCP_Close(tcpsock);
    if (comms->resp)
    {
        comms->resp->done = true;
        SDL_AtomicUnlock(&comms->resp->working);
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
    SDL_AtomicLock(&resp->working);
    resp->done = false;
    resp->error = false;
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

void GameState::score_submit(int level, bool sync)
{
    if ((level >= LEVEL_COUNT) && !edited_level_set->levels[level]->global)
        return;
        
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "score_submit");
    omap->add_num("level_index", level);
    omap->add_item("levels", edited_level_set->levels[level]->best_design->save_all(highest_level, true));
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    post_to_server(omap, sync);
}

void GameState::global_design_submit(int level)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "global_design_submit");
    omap->add_num("level_index", level);
    omap->add_item("levels", edited_level_set->save_one(level));
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    post_to_server(omap, true);
}


void GameState::score_fetch(int level_index)
{
    if (level_index >= LEVEL_COUNT)
        return;
    if (scores_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "score_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_num("type", test_mode);
    omap->add_string("steam_username", steam_username);
    omap->add_num("level_index", level_index);

    SaveObjectList* slist = new SaveObjectList;
    for (uint64_t f : friends)
        slist->add_num(f);
    omap->add_item("friends", slist);

    fetch_from_server(omap, &scores_from_server);
}

void GameState::score_fetch(std::string name)
{
    if (scores_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "score_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_num("type", test_mode);
    omap->add_string("steam_username", steam_username);
    omap->add_string("name", name);

    SaveObjectList* slist = new SaveObjectList;
    for (uint64_t f : friends)
        slist->add_num(f);
    omap->add_item("friends", slist);

    fetch_from_server(omap, &scores_from_server);
}

void GameState::server_levels_fetch()
{
    if (server_levels_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "server_levels_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    fetch_from_server(omap, &server_levels_from_server);
}

void GameState::server_level_fetch(std::string name)
{
    if (design_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "server_level_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_string("steam_username", steam_username);
    omap->add_string("name", name);
    fetch_from_server(omap, &design_from_server);
}

void GameState::design_fetch(uint64_t level_steam_id, int level_index)
{
    if (design_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "design_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_num("type", test_mode);
    omap->add_string("steam_username", steam_username);
    omap->add_num("level_steam_id", level_steam_id);
    omap->add_num("level_index", level_index);
    fetch_from_server(omap, &design_from_server);
}

void GameState::design_fetch(uint64_t level_steam_id, std::string name)
{
    if (design_from_server.working)
        return;
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("command", "design_fetch");
    omap->add_num("steam_id", steam_id);
    omap->add_num("type", test_mode);
    omap->add_string("steam_username", steam_username);
    omap->add_num("level_steam_id", level_steam_id);
    omap->add_string("name", name);
    fetch_from_server(omap, &design_from_server);
}

GameState::~GameState()
{
    Mix_FreeChunk(vent_steam_wav);
    Mix_FreeChunk(move_steam_wav);
    Mix_FreeMusic(music);

    TTF_CloseFont(font);

    delete level_set_accuracy;
    delete level_set_price;
    delete level_set_steam;
    delete clipboard_level_set;

    if (last_clip)
        SDL_free(last_clip);

	SDL_DestroyTexture(sdl_texture);
	SDL_DestroyTexture(sdl_tutorial_texture);
	SDL_DestroyTexture(sdl_levels_texture);
	SDL_DestroyRenderer(sdl_renderer);
	SDL_DestroyWindow(sdl_window);

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

    if (SDL_TICKS_PASSED(SDL_GetTicks(), time_last_progress + (1000 * 60 * 30)))
    {
        if (!discord_joined && !discord_prompt_shown)
        {
            discord_prompt_shown = true;
            show_dialogue_discord_prompt = true;
        }
        time_last_progress = SDL_GetTicks();
    }

    if (SDL_TICKS_PASSED(SDL_GetTicks(), time_last_progress + (1000 * 60 * (2 + tip_revealed * 2))))
    {
        if (highest_level > tip_revealed)
            tip_revealed++;
    }

    if (SDL_TICKS_PASSED(time, debug_last_time + period))
    {
        float mul = 1000 / float(time - debug_last_time);
        debug_last_second_simticks = round(debug_simticks * mul);
        debug_last_second_frames = round(debug_frames * mul);
        debug_simticks = 0;
        debug_frames = 0;
        debug_last_time = SDL_GetTicks();
    }
    deal_with_design_fetch();

    int count = pow(1.2, game_speed) * 2;
    if (game_speed == 0)
        count = ((frame_index % 10) == 0);
    
    if (game_speed == 1)
        count = 1;
    {
        if (skip_to_next_subtest)
        {
            count = current_level->substep_count - current_level->substep_index;
        }
        if (!count)
            current_level->advance(0);
        while (count)
        {
            int subcount = count < 2000 ? count : 2000;
            current_level->advance(subcount);
            count -= subcount;
            debug_simticks += subcount;
            if (SDL_TICKS_PASSED(SDL_GetTicks(), time + 20))
            {
                if (!skip_to_next_subtest)
                {
                    late_frames++;
                    if (late_frames > 4)
                        game_speed--;
                }
                break;
            }
            else
            {
                late_frames = 0;
            }
        }
        if (!count)
        {
            if (skip_to_subtest_index < 0 || skip_to_subtest_index == current_level->sim_point_index)
                skip_to_next_subtest = false;
        }
    }
    if (!current_level->server_refreshed && !current_level_set_is_inspected && current_level->best_design)
    {
        current_level->server_refreshed = true;
        score_submit(current_level_index, false);
    }

    if (current_level->best_score_set && (test_mode == TEST_MODE_ACCURACY) && !current_level_set_is_inspected)
    {
        current_level->best_score_set = false;
        edited_level_set->record_best_score(current_level_index);
        score_submit(current_level_index, false);
        time_last_progress = SDL_GetTicks();
    }
    if (current_level->best_price_set && (test_mode == TEST_MODE_PRICE) && !current_level_set_is_inspected)
    {
        current_level->best_price_set = false;
        edited_level_set->record_best_score(current_level_index);
        score_submit(current_level_index, false);
        time_last_progress = SDL_GetTicks();
    }
    if (current_level->best_steam_set && (test_mode == TEST_MODE_STEAM) && !current_level_set_is_inspected)
    {
        current_level->best_steam_set = false;
        edited_level_set->record_best_score(current_level_index);
        score_submit(current_level_index, false);
        time_last_progress = SDL_GetTicks();
    }
}

void GameState::audio()
{
    Mix_Volume(0, pressure_as_percent(current_circuit->last_vented*10) * sound_volume / 100);
    Mix_Volume(1, pressure_as_percent(current_circuit->last_moved*10) * sound_volume / 100);
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

void GameState::render_score_2digit_err(XYPos pos, Pressure value, unsigned scale_mul, unsigned bg_colour, unsigned fg_colour)
{
    if (value)
    {
        render_number_2digit(pos, pressure_as_percent(value), scale_mul, bg_colour, fg_colour);
    }
    else
    {
        int myscale = scale * scale_mul;
        SDL_Rect src_rect = {503, 80 + int(bg_colour), 1, 1};
        SDL_Rect dst_rect = {pos.x, pos.y-myscale, 9 * myscale, 7 * myscale};
        render_texture(src_rect, dst_rect);

        src_rect = {54, 160, 10, 5};
        dst_rect = {pos.x, pos.y, 10 * myscale, 5 * myscale};
        render_texture(src_rect, dst_rect);
    }

}
 

void GameState::render_number_pressure(XYPos pos, Pressure value, unsigned scale_mul, unsigned bg_colour, unsigned fg_colour)
{
    int myscale = scale * scale_mul;
    int64_t p = (int64_t(value) * 10000 + PRESSURE_SCALAR / 2) / PRESSURE_SCALAR;
    render_number_2digit(pos, (p / 10000), scale_mul, bg_colour, fg_colour);
    pos.x += 9 * myscale;
    {
        SDL_Rect src_rect = {49 + int(fg_colour/4) * 64, 160 + int(fg_colour%4) * 5, 1, 5};
        SDL_Rect dst_rect = {pos.x, pos.y, 1 * myscale, 5 * myscale};
        render_texture(src_rect, dst_rect);
    }
    pos.x += 1 * myscale;
    render_number_2digit(pos, (p / 100) % 100, scale_mul, bg_colour, fg_colour);
    if (number_high_precision)
    {
        pos.x += 8 * myscale;
        render_number_2digit(pos, p % 100, scale_mul, bg_colour, fg_colour);
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

int GameState::render_number_long_get_width(unsigned value, unsigned scale_mul)
{
    int i = 0;
    while (value)
    {
        i++;
        value /= 10;
    }
    return i * 4 * scale_mul;
}

int GameState::render_number_compact_get_width(int64_t value, unsigned scale_mul)
{
    return 6 * 4 * scale_mul;
}

void GameState::render_number_compact(XYPos pos, int64_t value, unsigned scale_mul)
{
    if (value == 0)
        return;
    int myscale = scale * scale_mul;
    int64_t scalar = 0;
    if (value < 1000)
    {
        scalar = 0;
        value *= 1000;
    }
    else if (value < 1000000)
    {
        scalar = 1;
    }
    else if (value < 1000000000)
    {
        scalar = 2;
        value /= 1000;
    }
    else
    {
        scalar = 3;
        value /= 1000000;
    }

    SDL_Rect src_rect = {0, 160, 4, 5};
    SDL_Rect dst_rect = {pos.x, pos.y, 4 * myscale, 5 * myscale};
    
    unsigned digits_till_dp = 3;
    
    while (value < 100000)
    {
        value *= 10;
        digits_till_dp--;
    }
    
    for (int i = 0; i < 3; i++)
    {
        int v = value / 100000;
        value -= v * 100000;
        value *= 10;
        src_rect.x = 0 + v * 4;
        render_texture(src_rect, dst_rect);
        dst_rect.x += 4 * myscale;
        digits_till_dp--;
        if (!digits_till_dp)
        {
            if (scalar == 0)
                return;
            if (i == 2)
                break;
            SDL_Rect src_rect2 = {48, 160, 2, 5};
            SDL_Rect dst_rect2 = {dst_rect.x, dst_rect.y, 2 * myscale, 5 * myscale};
            render_texture(src_rect2, dst_rect2);
            dst_rect.x += 2 * myscale;
        }
    }
    {
        SDL_Rect src_rect2 = {147 + int(scalar * 5), 160, 5, 5};
        SDL_Rect dst_rect2 = {dst_rect.x + myscale, dst_rect.y, 5 * myscale, 5 * myscale};
        render_texture(src_rect2, dst_rect2);
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

void GameState::render_button(XYPos pos, XYPos content, unsigned colour, const char* tooltip, SDL_Texture* texture)
{
    render_box(pos, XYPos(32,32), colour);
    SDL_Rect src_rect = {content.x , content.y , 24, 24};
    SDL_Rect dst_rect = {pos.x + 4 * scale, pos.y + 4 * scale, 24 * scale, 24 * scale};
    if (!texture)
        texture = sdl_texture;
    render_texture_custom(texture, src_rect, dst_rect);
    if (((mouse - pos)/scale).inside(XYPos(32,32)) && tooltip)
        tooltip_string = tooltip;
}

void GameState::render_tooltip()
{
    if (tooltip_string != "")
    {
        if (current_language->get_item("tooltips")->get_map()->has_key(tooltip_string.c_str()))
            tooltip_string = current_language->get_item("tooltips")->get_map()->get_string(tooltip_string.c_str());
        XYPos tip_size = get_text_size(tooltip_string) + XYPos(2,0);
        XYPos tip_pos = mouse / scale - XYPos(tip_size.x, 0);
        if (tip_pos.x < 0)
            tip_pos.x = 0;
        {
            SDL_Rect src_rect = {503, 83, 1, 1};
            SDL_Rect dst_rect = {tip_pos.x * scale, tip_pos.y * scale, tip_size.x * scale, tip_size.y * scale};
            render_texture(src_rect, dst_rect);
        }

        render_text(tip_pos + XYPos(1,0), tooltip_string.c_str(), SDL_Color{0x0,0x0,0x0});
        tooltip_string = "";
    }
}

void GameState::render_scroll_bar(ScrollBar& sbar)
{
    normalize_scroll_bar(sbar);
    {
        SDL_Rect src_rect = {304, 280, 16, 16};
        SDL_Rect dst_rect = {sbar.pos.x * scale, sbar.pos.y * scale, 16 * scale, 16 * scale};
        render_texture(src_rect, dst_rect);
    }
    {
        int total_size = sbar.height - 32;
        int bar_size = total_size;
        if (sbar.total_rows > sbar.visible_rows)
            bar_size = (total_size * sbar.visible_rows) / sbar.total_rows;
        int bar_offset = (total_size * sbar.offset_rows)/ sbar.total_rows + 16;
        
        SDL_Rect src_rect = {304, 296, 16, 9};
        SDL_Rect dst_rect = {sbar.pos.x * scale, (sbar.pos.y + bar_offset) * scale, 16 * scale, 9 * scale};
        render_texture(src_rect, dst_rect);

        src_rect = {304, 308, 16, 1};
        dst_rect = {sbar.pos.x * scale, (sbar.pos.y + bar_offset + 9) * scale, 16 * scale, (bar_size - 17) * scale};
        render_texture(src_rect, dst_rect);

        src_rect = {304, 312, 16, 8};
        dst_rect = {sbar.pos.x * scale, (sbar.pos.y + bar_offset + bar_size - 8) * scale, 16 * scale, 8 * scale};
        render_texture(src_rect, dst_rect);

    }
    {
        SDL_Rect src_rect = {304, 320, 16, 16};
        SDL_Rect dst_rect = {sbar.pos.x * scale, (sbar.pos.y + sbar.height - 16) * scale, 16 * scale, 16 * scale};
        render_texture(src_rect, dst_rect);
    }

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

void GameState::render_text(XYPos tl, const char* string, SDL_Color color, unsigned scale_mul)
{
    std::string text = string;

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
        dst_rect.w = src_rect.w * scale * scale_mul;
        dst_rect.h = src_rect.h * scale * scale_mul;

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

static int max_help_page(int next_dialogue_level)
{
    if (next_dialogue_level <= 1)
        return 2;
    if (next_dialogue_level <= 2)
        return 4;
    if (next_dialogue_level <= 3)
        return 5;
    if (next_dialogue_level <= 4)
        return 6;
    if (next_dialogue_level <= 6)
        return 7;
    if (next_dialogue_level <= 8)
        return 9;
    if (next_dialogue_level <= 24)
        return 11;
    return 12;
}

void GameState::render(bool saving)
{
    check_clipboard();
    deal_with_scores();
    deal_with_server_levels_from_server();
    current_circuit->render_prep();

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
    
    if (edited_level_set->levels[highest_level]->best_score && (highest_level + 1) < LEVEL_COUNT)
    {
        highest_level++;
        level_win_animation = 100;
        if (highest_level == 20)
            number_high_precision = true;
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

    if ((current_level->tests[current_level->test_index].sim_points.size() == current_level->sim_point_index + 1) && !current_circuit_is_inspected_subcircuit)
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
        render_number_2digit(num_pos, value, 2, 6, 0);                  // Values on input pipes
    }

    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        SDL_Rect src_rect = {256, 480, 32, 32};
        if (pos.y > 0)
            src_rect.y += 32;
        if (pos.y == 8)
            src_rect.y += 32;
        if (pos.x > 0)
            src_rect.x += 32;
        if (pos.x == 8)
            src_rect.x += 32;
        if (current_circuit_is_read_only)
            src_rect.y += 96;
        else if (current_circuit_is_inspected_subcircuit)
            src_rect.y += 96 * 2;

        SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
        render_texture(src_rect, dst_rect);
        
        bool blocked = current_circuit->is_blocked(pos);
        
        if (mouse_state == MOUSE_STATE_LOCKING_BLOCKS)
        {
            XYPos mouse_grid = ((mouse - grid_offset)/ scale) / 32;
            if (mouse_grid == pos)
                blocked = !blocked;
        }
        
        if (blocked)
        {
            src_rect = {384, 80, 32, 32};
            render_texture(src_rect, dst_rect);
        }

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
        if (src_pos != XYPos(-1,-1))
        {
            SDL_Rect src_rect = {src_pos.x, src_pos.y, 32, 32};
            SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
        
        
        src_pos = current_circuit->elements[pos.y][pos.x]->getimage_fg();
        if (src_pos != XYPos(-1,-1))
        {
            WrappedTexture* w_tex = current_circuit->elements[pos.y][pos.x]->getimage_fg_texture();
            SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
            SDL_Rect src_rect =  {src_pos.x, src_pos.y, 24, 24};
            SDL_Rect dst_rect = {(pos.x * 32 + 4) * scale + grid_offset.x, (pos.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
            render_texture_custom(tex, src_rect, dst_rect);
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
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_circuit->is_blocked(pipe_start_grid_pos + XYPos(0,-1))))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_circuit->is_blocked(pipe_start_grid_pos)))
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
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_circuit->is_blocked(pipe_start_grid_pos + XYPos(-1,0))))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_circuit->is_blocked(pipe_start_grid_pos)))
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
            SDL_Rect src_rect = {dir_flip.get_n() * 32, 4 * 32, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            SDL_Rect src_rect = {128 + dir_flip.get_n() * 32, 0, 32, 32};
            SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            XYPos pos = edited_level_set->levels[placing_subcircuit_level]->getimage(dir_flip);
            {
                SDL_Rect src_rect = {208, 160, 24, 24};
                SDL_Rect dst_rect = {(mouse_grid.x * 32 + 4) * scale + grid_offset.x, (mouse_grid.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
            }
            if (pos != XYPos(-1,-1))
            {
                SDL_Rect src_rect = {pos.x, pos.y, 32, 32};
                SDL_Rect dst_rect = {mouse_grid.x * 32 * scale + grid_offset.x, mouse_grid.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                render_texture(src_rect, dst_rect);
            }

            pos = edited_level_set->levels[placing_subcircuit_level]->getimage_fg(dir_flip);
            if (pos != XYPos(-1,-1))
            {
                WrappedTexture* w_tex = edited_level_set->levels[placing_subcircuit_level]->getimage_fg_texture();
                SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
                SDL_Rect src_rect = {pos.x, pos.y, 24, 24};
                SDL_Rect dst_rect = {(mouse_grid.x * 32 + 4) * scale + grid_offset.x, (mouse_grid.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
                render_texture_custom(tex, src_rect, dst_rect);
            }

        }
    }
    else if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
    {
        clipboard.elaborate(level_set);
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        
        mouse_grid.x = std::max(std::min(mouse_grid.x, 9 - clipboard.size().x), 0);
        mouse_grid.y = std::max(std::min(mouse_grid.y, 9 - clipboard.size().y), 0);
        
        for (Clipboard::ClipboardElement& clip_elem: clipboard.elements)
        {
            XYPos pos = clip_elem.pos + mouse_grid;
            
            
            CircuitElement* element = clip_elem.element;
            {
                SDL_Rect src_rect = element->getimage_bg();
                if (src_rect.w)
                {
                    int xoffset = (32 - src_rect.w) / 2;
                    int yoffset = (32 - src_rect.h) / 2;
                    SDL_Rect dst_rect = {(pos.x * 32  + xoffset) * scale + grid_offset.x, (pos.y * 32 + yoffset) * scale + grid_offset.y, src_rect.w * scale, src_rect.h * scale};
                    render_texture(src_rect, dst_rect);
                }
            }


            XYPos src_pos = element->getimage();
            if (src_pos != XYPos(-1,-1))
            {
                SDL_Rect src_rect = {src_pos.x, src_pos.y, 32, 32};
                SDL_Rect dst_rect = {pos.x * 32 * scale + grid_offset.x, pos.y * 32 * scale + grid_offset.y, 32 * scale, 32 * scale};
                render_texture(src_rect, dst_rect);
            }

            src_pos = element->getimage_fg();
            WrappedTexture* w_tex = element->getimage_fg_texture();
            if (src_pos != XYPos(-1,-1))
            {
                SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
                SDL_Rect src_rect =  {src_pos.x, src_pos.y, 24, 24};
                SDL_Rect dst_rect = {(pos.x * 32 + 4) * scale + grid_offset.x, (pos.y * 32 + 4) * scale + grid_offset.y, 24 * scale, 24 * scale};
                render_texture_custom(tex, src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect =  {256, 176, 32, 32};
                SDL_Rect dst_rect = {(pos.x * 32) * scale + grid_offset.x, (pos.y * 32) * scale + grid_offset.y, 32 * scale, 32 * scale};
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
        if (current_circuit->touched_ns[pos.y][pos.x] == 0)
            continue;
        Pressure vented = (current_circuit->touched_ns[pos.y][pos.x] == 1) ? (current_circuit->connections_ns[pos.y][pos.x].value) : 0;
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
        if (current_circuit->touched_ew[pos.y][pos.x] == 0)
            continue;
        Pressure vented = (current_circuit->touched_ew[pos.y][pos.x] == 1) ? (current_circuit->connections_ew[pos.y][pos.x].value) : 0;
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

        {
            std::string text = sign.text;
            if (frame_index % 60 < 30 && (&sign == &current_circuit->signs.front()) && (mouse_state == MOUSE_STATE_ENTERING_TEXT) && (text_entry_target == TEXT_TARGET_SIGN))
                text.insert(text_entry_offset, std::string((const char*)u8"\u258F"));
            render_text(sign.get_pos() + XYPos(32,32) + XYPos(4,4), text.c_str());
        }
    }

    
    if (mouse_state == MOUSE_STATE_PLACING_SIGN)
    {
        XYPos mouse_grid = (mouse - grid_offset) / scale;
        Sign sign(mouse_grid, dir_flip.get_n(), "");
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

    {
        int x = 0;
        {
            WrappedTexture* w_tex = current_level->getimage_fg_texture();
            SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
            render_button(XYPos(x * scale, 0 * scale), current_level->getimage_fg(DIRECTION_N), 0, current_level->name.c_str(), tex);
        }
        x += 32;

        if (current_circuit_is_inspected_subcircuit)
        {
            bool editable = true;
            for (auto &sub : inspection_stack)
            {
                if (x == 32*5)
                    x += 32;
                int l_index;
                sub->get_subcircuit(&l_index);
                if (!sub->get_custom() || sub->get_read_only())
                    editable = false;
                
                unsigned color = editable ? 1 : 4;
                
                WrappedTexture* w_tex = sub->getimage_fg_texture();
                SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
                const char* l_name = l_index < 0 ? "Lost level" : level_set->levels[l_index]->name.c_str();
                render_button(XYPos(x * scale, 0 * scale), sub->getimage_fg(), color, l_name, tex);
                x += 32;
            }
        }

        if (current_circuit_is_inspected_subcircuit && !current_level_set_is_inspected)
        {
            for (auto &sub : inspection_stack)
            {
                if (sub->get_read_only())
                    break;
                if (!sub->get_custom())
                {
                    if (sub == inspection_stack.back())
                        render_button(XYPos(10 * 32 * scale, 0), XYPos(304, 112), 0, "Customize");
                    break;
                }
            }
        }
        else if (current_level_set_is_inspected)
        {
            if (current_level_index >= LEVEL_COUNT && !current_circuit_is_inspected_subcircuit)
                render_button(XYPos(7 * 32 * scale, 0), XYPos(376, 136), 0, "Import level");
            if (deletable_level_set && !current_circuit_is_inspected_subcircuit)
                render_button(XYPos(8 * 32 * scale, 0), XYPos(400, 160), 0, "Delete");
            if (!current_circuit_is_inspected_subcircuit && ((current_level_index <  LEVEL_COUNT) || ((edited_level_set->find_custom_by_name(current_level->name)) > 0) ))
                render_button(XYPos(9 * 32 * scale, 0), XYPos(376, 136), 0, "Copy design\nto current");
            render_button(XYPos(10 * 32 * scale, 0), XYPos(352, 136), 0, "Return");
        }
        else if (editing_level)
        {
            render_button(XYPos(9 * 32 * scale, 0), XYPos(400, 160), 0, "Delete level");
            render_button(XYPos(10 * 32 * scale, 0), XYPos(352, 136), 0, "Return");
        }
        else if (current_level_index >= LEVEL_COUNT && next_dialogue_level > 24)
        {
            if (current_level->global)
                render_button(XYPos(10 * 32 * scale, 0), XYPos(400, 160), 0, "Delete");
            else
                render_button(XYPos(10 * 32 * scale, 0), XYPos(304, 112), 0, "Level Editor");
        }

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
            
            render_box(XYPos((32 * 11) * scale, 0), XYPos(8*32 + 32, 11*32 + 8), panel_colour);
        }
        {                                                                                               // Top Menu
            bool flash_next_level = (next_dialogue_level == highest_level) && (frame_index % 60 < 30) && (highest_level < LEVEL_COUNT);
            render_button(XYPos((8 + 32 * 11) * scale, 8 * scale), XYPos(256 + (flash_next_level ? 24 : 0), 112), panel_state == PANEL_STATE_LEVEL_SELECT, "Level select");
            if (!current_circuit_is_read_only && (next_dialogue_level > 1) && (!flash_editor_menu || (current_level_index != 1) ||(frame_index % 60 < 30) || show_dialogue))
                render_button(XYPos((8 + 32 * 12) * scale, 8 * scale), XYPos(256+24*2, 112), panel_state == PANEL_STATE_EDITOR, "Design");
            if (next_dialogue_level > 4)
                render_button(XYPos((8 + 32 * 13) * scale, 8 * scale), XYPos(256+24*3, 112), panel_state == PANEL_STATE_MONITOR, "Test");
            if (next_dialogue_level > 6)
                render_button(XYPos((8 + 32 * 14) * scale, 8 * scale), XYPos(256+24*4, 112), panel_state == PANEL_STATE_TEST, "Experiment");
            render_button(XYPos((8 + 32 * 15) * scale, (8) * scale), XYPos(0, 0), panel_state == PANEL_STATE_SCORES, "Scores");
        }
        if (next_dialogue_level > 4)
        {                                                                                               // Speed arrows
            render_box(XYPos((8 + 32 * 16) * scale, (8) * scale), XYPos(64, 32), 3);
            SDL_Rect src_rect = {256, 136, 53, 5};
            SDL_Rect dst_rect = {(8 + 32 * 16 + 6) * scale, (8 + 6) * scale, 53 * scale, 5 * scale};
            render_texture(src_rect, dst_rect);

            src_rect = {624, 16, 16, 16};
            dst_rect = {(8 + 32 * 11 + 32 * 5 + int(game_speed)) * scale, (8 + 8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        {                                                                                               // Current Score
            render_number_2digit(XYPos((8 + 32 * 11 + 32 * 4 + 3) * scale, (8 + 8) * scale), pressure_as_percent(current_level->best_score), 3);
        }
        {                                                                                               // Help Button
            unsigned highlight = show_help;
            if (max_help_page(next_dialogue_level) > next_help_highlight && (frame_index % 60 < 30))
                highlight = 1;
            render_button(XYPos((8 + 32 * 11 + 7 * 32) * scale, (8) * scale), XYPos(256+24*5, 112), highlight, "Help");
        }

    }

    if (panel_state == PANEL_STATE_LEVEL_SELECT && !editing_level)
    {
        int visible_rows = level_select_requirements_visible ? 4 : 8;
        
        level_select_scroll.visible_rows = visible_rows;
        level_select_scroll.height = visible_rows * 32;

        level_select_scroll.total_rows = (level_set->top_playable(highest_level) + 8) / 8;
        normalize_scroll_bar(level_select_scroll);

        for (pos.y = 0; pos.y < visible_rows; pos.y++)
        for (pos.x = 0; pos.x < 8; pos.x++)
        {
            unsigned index =  pos.y * 8 + pos.x;
            int level_index =  index + level_select_scroll.offset_rows * 8;

            if (!level_set->is_playable(level_index, highest_level))
                continue;
            if (next_dialogue_level == level_index && (frame_index % 60 < 30) && !show_dialogue && (next_dialogue_level < LEVEL_COUNT))
                continue;

            WrappedTexture* w_tex = level_set->levels[level_index]->getimage_fg_texture();
            SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
            render_button(XYPos(pos.x * 32 * scale + panel_offset.x, pos.y * 32 * scale + panel_offset.y), level_set->levels[level_index]->getimage_fg(DIRECTION_N), level_index == current_level_index ? 1 : (level_set->levels[level_index]->global ? 2 : 0), level_set->levels[level_index]->name.c_str(), tex);
            

            switch (test_mode)
            {
                case TEST_MODE_ACCURACY:
                    render_score_2digit_err(XYPos((pos.x * 32 + 32 - 9 - 4) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), level_set->levels[level_index]->best_score);
                    break;
                case TEST_MODE_PRICE:
                {
                    SDL_Rect src_rect = {144, 160, 5, 5};
                    SDL_Rect dst_rect = {(pos.x * 32 + 32 - 8 - render_number_long_get_width(level_set->levels[level_index]->best_price)) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y, 5 * scale, 5 * scale};
                    render_texture(src_rect, dst_rect);
                    render_number_long(XYPos((pos.x * 32 + 32 - 4 - render_number_long_get_width(level_set->levels[level_index]->best_price)) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), level_set->levels[level_index]->best_price);
                    break;
                }
                case TEST_MODE_STEAM:
                    render_number_compact(XYPos((pos.x * 32 + 32 - 4 - render_number_compact_get_width(level_set->levels[level_index]->best_steam)) * scale + panel_offset.x, (pos.y * 32 + 4) * scale + panel_offset.y), level_set->levels[level_index]->best_steam);
                    break;
            }
        }
        render_scroll_bar(level_select_scroll);

        XYPos buttons_offset = panel_offset + XYPos(0, 144 * scale);
        if (!level_select_requirements_visible)
            buttons_offset.y += 128 * scale;
        
        render_button(XYPos(buttons_offset.x, buttons_offset.y), XYPos(304, 256), 0, "Repeat\ndialogue");                   // Blah
        render_button(XYPos(buttons_offset.x + 32 * scale, buttons_offset.y), XYPos(328, 256), 0, "Hint");                  // Hint
        if (tip_revealed >= current_level_index && edited_level_set->levels[current_level_index]->help_design)
            render_button(XYPos(buttons_offset.x + 64 * scale, buttons_offset.y), XYPos(352, 160), 0, "Reveal a solution");   // Help
        render_button(XYPos(buttons_offset.x + 96 * scale, buttons_offset.y), XYPos(280, 376), level_select_requirements_visible, "Requirements");                  // Requirements
        if (next_dialogue_level > 32)
        {
            switch (test_mode)
            {
                case TEST_MODE_ACCURACY:
                    render_button(XYPos(buttons_offset.x + 6*32 * scale, buttons_offset.y), XYPos(256, 352), 0, "Accuracy mode");
                    break;
                case TEST_MODE_PRICE:
                    render_button(XYPos(buttons_offset.x + 6*32 * scale, buttons_offset.y), XYPos(280, 352), 0, "Price mode");
                    break;
                case TEST_MODE_STEAM:
                    render_button(XYPos(buttons_offset.x + 6*32 * scale, buttons_offset.y), XYPos(256, 376), 0, "Steam mode");
                    break;
            }
        }

        if (next_dialogue_level > 24 && !current_circuit_is_read_only)
            render_button(XYPos(buttons_offset.x + 7*32 * scale, buttons_offset.y), XYPos(376, 160), 0, "New design");          // New

        if (scoring_mode_dialogue)
        {
            render_box(XYPos(buttons_offset.x + (160-8) * scale, buttons_offset.y - 8 * scale), XYPos(32*3+16, 32+16), 4);
            render_button(XYPos(buttons_offset.x + 5*32 * scale, buttons_offset.y), XYPos(256, 352), test_mode == TEST_MODE_ACCURACY, "Accuracy mode");
            render_button(XYPos(buttons_offset.x + 6*32 * scale, buttons_offset.y), XYPos(280, 352), test_mode == TEST_MODE_PRICE, "Price mode");
            render_button(XYPos(buttons_offset.x + 7*32 * scale, buttons_offset.y), XYPos(256, 376), test_mode == TEST_MODE_STEAM, "Steam mode");
        }
    }
    else if (panel_state == PANEL_STATE_LEVEL_SELECT && editing_level)
    {
        render_box(panel_offset, XYPos(256, 32), 4);

        std::string text = current_level->name;
        if ((frame_index % 60 < 30) && (mouse_state == MOUSE_STATE_ENTERING_TEXT) && (text_entry_target == TEXT_TARGET_LEVEL_NAME))
            text.insert(text_entry_offset, std::string((const char*)u8"\u258F"));
        render_text(panel_offset / scale + XYPos(4,4), text.c_str(), SDL_Color{0xff,0xff,0xff}, 2);
        
        render_box(XYPos((0) * scale + panel_offset.x, (32)* scale + panel_offset.y), XYPos(12*16+16, 12*16+16), 0);
        render_box(XYPos((12*16+16) * scale + panel_offset.x, (32)* scale + panel_offset.y), XYPos(32, 12*16+16), 0);
        render_box(XYPos((0) * scale + panel_offset.x, (32 + 12*16+16)* scale + panel_offset.y), XYPos(24*8+8, 32), 0);
        {
            SDL_Rect src_rect = {503, 80, 1, 8};
            SDL_Rect dst_rect = {(12*16+16+8) * scale + panel_offset.x, (32 + 8)* scale + panel_offset.y, 16 * scale, 16 * 8 * scale};
            render_texture(src_rect, dst_rect);
            src_rect = {320, 144, 16, 16};
            dst_rect = {(12*16+16+8) * scale + panel_offset.x, (32 + 8 + pixel_colour * 16)* scale + panel_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);

            src_rect = {icon_rotate ? 416 : 368, 208, 16, 16};
            dst_rect = {(12*16+16+8) * scale + panel_offset.x, (32 + 8 + 10 * 16)* scale + panel_offset.y, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        
        for (pos.y = 0; pos.y < 24; pos.y++)
        for (pos.x = 0; pos.x < 24; pos.x++)
        {
            int colour;
            if (editing_icon_index == -1)
            {
                colour = level_set->levels[current_level_index]->icon_pixels[pos.y][pos.x];
                for (int i = 0; i < 8; i++)
                {
                    XYPos npos = icon_rotate ? DirFlip(i).trans(pos, 24) : pos;
                    if (colour != level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24])
                    {
                        colour = 9;
                        break;
                    }
                }
            }
            else
                colour = level_set->levels[current_level_index]->icon_pixels[pos.y][pos.x + editing_icon_index * 24];
            SDL_Rect src_rect = {503, 80 + colour, 1, 1};
            SDL_Rect dst_rect = {(pos.x * 8 + 8) * scale + panel_offset.x, (32 + 8 + pos.y * 8)* scale + panel_offset.y, 8 * scale, 8 * scale};
            if (colour == 8)
            {
                src_rect = {488, 80, 8, 8};
            }
            else if (colour == 9)
            {
                src_rect = {480, 80, 8, 8};
            }
            render_texture(src_rect, dst_rect);
        }
        {
            SDL_Texture* tex = level_set->levels[current_level_index]->getimage_fg_texture()->get_texture();
            for (int i = 0; i < 8; i++)
            {
                SDL_Rect src_rect = {editing_icon_index == i ? 232 : 208, 160, 24, 24};
                SDL_Rect dst_rect = {(4 + i * 24)* scale + panel_offset.x, (32 + 12*16+16 + 4)* scale + panel_offset.y, 24 * scale, 24 * scale};
                render_texture(src_rect, dst_rect);
            }
            SDL_Rect src_rect = {0, 0, 24*8, 24};
            SDL_Rect dst_rect = {(4)* scale + panel_offset.x, (32 + 12*16+16 + 4)* scale + panel_offset.y, 24*8 * scale, 24 * scale};
            render_texture_custom(tex, src_rect, dst_rect);
        }
        render_button(XYPos(panel_offset.x, (32 + 32 + 12*16+16)* scale + panel_offset.y), XYPos(280, 376), 0, "Requirements");
//        render_button(XYPos(panel_offset.x + 32 * scale, (32 + 32 + 12*16+16)* scale + panel_offset.y), XYPos(304, 256), 0, "Dialogue");
    }

    if (panel_state == PANEL_STATE_EDITOR)
    {
        int visible_rows = editor_requirements_visible ? 4 : 8;

        editor_scroll.visible_rows = visible_rows;
        editor_scroll.height = visible_rows * 32;

        editor_scroll.total_rows = (level_set->top_playable(highest_level) + 8) / 8;
        normalize_scroll_bar(editor_scroll);

    
        pos = XYPos(0,0);
        bool flasher = (frame_index % 50) < 25;

        if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
            flash_steam_inlet = false;
        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
            flash_valve = false;

        if (next_dialogue_level > 2)
            render_button(XYPos(panel_offset.x + 0 * 32 * scale, panel_offset.y), XYPos(544 + dir_flip.get_n() * 24, 160), mouse_state == MOUSE_STATE_PLACING_VALVE || (flasher && flash_valve && (current_level_index == 2)), "Valve");
        render_button(XYPos(panel_offset.x + 1 * 32 * scale, panel_offset.y), XYPos(544 + dir_flip.get_n() * 24, 160 + 24), mouse_state == MOUSE_STATE_PLACING_SOURCE || (flasher && flash_steam_inlet), "Steam Inlet");

        if (next_dialogue_level > 1)
        {
            render_button(XYPos(panel_offset.x + 2 * 32 * scale, panel_offset.y), XYPos(400, 112), 0, "Rotate left");
            render_button(XYPos(panel_offset.x + 3 * 32 * scale, panel_offset.y), XYPos(400+24, 112), 0, "Rotate right");
        }
        if (next_dialogue_level > 6)
        {
            render_button(XYPos(panel_offset.x + 4 * 32 * scale, panel_offset.y), XYPos(400, 184), 0, "Reflect\nvertically");
            render_button(XYPos(panel_offset.x + 5 * 32 * scale, panel_offset.y), XYPos(400+24, 184), 0, "Reflect\nhorizontally");
        }

        if (editing_level)
            render_button(XYPos(panel_offset.x + 6 * 32 * scale, panel_offset.y), XYPos(400+24, 160), mouse_state == MOUSE_STATE_LOCKING_BLOCKS, "Lock blocks");
        else
            render_button(XYPos(panel_offset.x + 6 * 32 * scale, panel_offset.y), XYPos(280, 376), editor_requirements_visible, "Requirements");

        if (next_dialogue_level > 8)
            render_button(XYPos(panel_offset.x + 7 * 32 * scale, panel_offset.y), XYPos(544 + dir_flip.get_n() * 24, 160 + 48), mouse_state == MOUSE_STATE_PLACING_SIGN, "Add sign");


        int level_index = 0;

        if (next_dialogue_level > 8)
        for (pos.y = 0; pos.y < visible_rows; pos.y++)
        for (pos.x = 0; pos.x < 8; pos.x++)
        {
            int level_index =  pos.y * 8 + pos.x + editor_scroll.offset_rows * 8;
            if (level_index >= edited_level_set->levels.size())
                break;
            if (level_index != current_level_index && !edited_level_set->levels[level_index]->circuit->contains_subcircuit_level(current_level_index, edited_level_set) && edited_level_set->is_playable(level_index, highest_level))
            {
                WrappedTexture* w_tex = edited_level_set->levels[level_index]->getimage_fg_texture();
                SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
                render_button((pos * 32 + XYPos(0, 32 + 8)) * scale + panel_offset, edited_level_set->levels[level_index]->getimage_fg(dir_flip), mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT && level_index == placing_subcircuit_level, edited_level_set->levels[level_index]->name.c_str(), tex);
            }
        }
        render_scroll_bar(editor_scroll);


//         XYPos panel_pos = ((mouse - panel_offset) / scale);                 // Tooltip
//         XYPos panel_grid_pos = panel_pos / 32;
//         if (panel_pos.y >= 0 && panel_pos.x >= 0 && panel_grid_pos.x < 8)
//         {
//             if (panel_grid_pos.y == 0 &&  panel_grid_pos.x < 4)
//             {
//                 SDL_Rect src_rect = {496, 124 + panel_grid_pos.x * 12, 28, 12};
//                 SDL_Rect dst_rect = {(panel_pos.x - 28)* scale + panel_offset.x, panel_pos.y * scale + panel_offset.y, 28 * scale, 12 * scale};
//                 render_texture(src_rect, dst_rect);
//             }
//         }

    }
    {
        bool visible = false;
        if (panel_state == PANEL_STATE_LEVEL_SELECT)
        {
            if (!editing_level)
                visible = level_select_requirements_visible;
            else
                if ((mouse_state == MOUSE_STATE_ENTERING_TEXT) && (text_entry_target == TEXT_TARGET_LEVEL_DESCRIPTION))
                    visible = true;
        }
        if (panel_state == PANEL_STATE_EDITOR)
            visible = editor_requirements_visible;
            
        if (visible)
        {
            if (current_level_index < LEVEL_COUNT)
            {
                SDL_Rect src_rect = {show_hint * 256, int(current_level_index) * 128, 256, 128};                    // Requirements
                SDL_Rect dst_rect = {panel_offset.x, panel_offset.y + 176 * scale, 256 * scale, 128 * scale};
                render_texture_custom(sdl_levels_texture, src_rect, dst_rect);
            }
            else
            {
                render_box(XYPos((0) * scale + panel_offset.x, 176 * scale + panel_offset.y), XYPos(256, 128), 4);
                std::string text = level_set->levels[current_level_index]->description;
                if (frame_index % 60 < 30 && (mouse_state == MOUSE_STATE_ENTERING_TEXT) && (text_entry_target == TEXT_TARGET_LEVEL_DESCRIPTION))
                    text.insert(text_entry_offset, std::string((const char*)u8"\u258F"));
                render_text_wrapped(XYPos(panel_offset.x / scale + 4, panel_offset.y / scale + 176 + 4), text.c_str(), 256-8);
            }
        }
    }

    

    if (panel_state == PANEL_STATE_TEST)
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
                SDL_Rect dst_rect = {(port_index * 48 + 8) * scale + panel_offset.x, (101 - int(current_level->current_simpoint.values[port_index])) * scale + panel_offset.y, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            render_number_2digit(XYPos((port_index * 48 + 8 + 3 ) * scale + panel_offset.x, ((101 - current_level->current_simpoint.values[port_index]) + 5) * scale + panel_offset.y), current_level->current_simpoint.values[port_index]);
            
            {
                SDL_Rect src_rect = {256 + 80 + (port_index * 6 * 16) , 16, 16, 16};
                SDL_Rect dst_rect = {(port_index * 48 + int(current_level->current_simpoint.force[port_index])/ 3) * scale + panel_offset.x, (101 + 16 + 7) * scale + panel_offset.y, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            //render_number_2digit(XYPos((port_index * 48 + current_level->current_simpoint.force[port_index] + 3) * scale + panel_offset.x, (101 + 16 + 7 + 5) * scale + panel_offset.y), current_level->current_simpoint.force[port_index]*3);
            
            render_number_pressure(XYPos((port_index * 48 + 8 + (number_high_precision ? 2 : 6)) * scale + panel_offset.x, (101 + 16 + 20 + 5) * scale + panel_offset.y), current_level->ports[port_index].value);

            
        }

        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(256-32-16, 120), 5);
        render_box(XYPos(panel_offset.x + (256-32-16) * scale, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(48, 120), 0);
        {
            XYPos graph_pos(8 * scale + panel_offset.x, (32 + 32 + 8 + 112 + 9) * scale + panel_offset.y);
            {
                SDL_Rect src_rect = {524, 80, 13, 101};
                SDL_Rect dst_rect = {0 + graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {524, 80, 5, 101};
                SDL_Rect dst_rect = {panel_offset.x + (256-32-16+32) * scale, graph_pos.y, 5 * scale, 101 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {464, 128, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + (256-32-16+8) * scale, graph_pos.y, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {480, 128, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + (256-32-16+8) * scale, graph_pos.y + (100 - 16) * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            {
                SDL_Rect src_rect = {256 + 80 , 16, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + (256-32-16+16) * scale, graph_pos.y + ((int)current_level->test_pressure_histroy_speed - 8) * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }
            
            for (int i = 0; i < 192-1; i++)
            {
                Level::PressureRecord& rec1 = current_level->test_pressure_histroy[(current_level->test_pressure_histroy_index + i) % 192];
                Level::PressureRecord& rec2 = current_level->test_pressure_histroy[(current_level->test_pressure_histroy_index + i + 1) % 192];
                if ((rec1.values[0] >= 0)  && (rec2.values[0] >= 0))
                {
                    if (rec1.marker)
                    {
                        SDL_Rect src_rect = {448, 136, 8, 8};
                        SDL_Rect dst_rect = {i * scale + graph_pos.x, 101 * scale + graph_pos.y, 8 * scale, 8 * scale};
                        render_texture(src_rect, dst_rect);
                    }
                    if (rec1.marker)
                    {
                        SDL_Rect src_rect = {526, 80, 2, 101};
                        SDL_Rect dst_rect = {i * scale + graph_pos.x, scale + graph_pos.y, 2 * scale, 101 * scale};
                        render_texture(src_rect, dst_rect);
                    }
                    for (int port = 0; port < 4; port++)
                    {
                        int myport = ((current_level->test_pressure_histroy_index + i) % 192 + port) % 4;
                        int v1 = pressure_as_percent(rec1.values[myport]);
                        int v2 = pressure_as_percent(rec2.values[myport]);
                        int top = 100 - std::max(v1, v2);
                        int size = abs(v1 - v2) + 1;

                        SDL_Rect src_rect = {502, 80 + myport, 1, 1};
                        SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                        render_texture(src_rect, dst_rect);
                    }
                }
            }
            
        }
    }
    else if (panel_state == PANEL_STATE_MONITOR)
    {
        unsigned test_index = current_level->test_index;
        unsigned test_count = current_level->tests.size();
        pos = XYPos(0,0);
        render_button(XYPos(panel_offset.x + 0 * 32 * scale, panel_offset.y), XYPos(448 + 0 * 24, 176), current_level->monitor_state == MONITOR_STATE_PAUSE, "Stop tests");
        render_button(XYPos(panel_offset.x + 1 * 32 * scale, panel_offset.y), XYPos(448 + 1 * 24, 176), current_level->monitor_state == MONITOR_STATE_PLAY_1, "Repeat 1 test");
        render_button(XYPos(panel_offset.x + 2 * 32 * scale, panel_offset.y), XYPos(448 + 2 * 24, 176), current_level->monitor_state == MONITOR_STATE_PLAY_ALL, "Run all tests");
        
        if ((next_dialogue_level > 8) && !current_level_set_is_inspected)
        {
            render_button(XYPos(panel_offset.x + 4 * 32 * scale, panel_offset.y), XYPos(400, 136), 0, "Export to\nclipboard");
            if (clipboard_level_set)
                render_button(XYPos(panel_offset.x + 5 * 32 * scale, panel_offset.y), XYPos(424, 136), 0, "Import from\nclipboard");

            for (int i = 0; i < 4; i++)                         // save/restore
            {
                SDL_Rect src_rect = {432 + i * 96, 0, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + (i * 16 + 32 * 6) * scale, panel_offset.y + 16 * scale, 16 * scale, 16 * scale};
                if (i == 3)
                    src_rect = {432 + 2 * 96, 80, 16, 16};
                render_texture(src_rect, dst_rect);
                if (((mouse - XYPos(dst_rect.x, dst_rect.y))/scale).inside(XYPos(16,16)))
                    tooltip_string = "Save";
                src_rect.y += 32;
                dst_rect.y -= 16 * scale;
                if (current_level->saved_designs[i])                     // restore stars star
                {
                    render_texture(src_rect, dst_rect);
                    if (((mouse - XYPos(dst_rect.x, dst_rect.y))/scale).inside(XYPos(16,16)))
                        tooltip_string = "Restore";
                }
            }
        }



        if (current_level->best_design)                     // Little star
        {
            SDL_Rect src_rect = {336, 32, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + (0) * scale, panel_offset.y + (32 + 8 + 16) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
            if (((mouse - XYPos(panel_offset.x + (0) * scale, panel_offset.y + (32 + 8 + 16) * scale))/scale).inside(XYPos(16,16)))
                tooltip_string = "Best solution";

        }

        for (int i = 0; i < test_count; i++)
        {
            SDL_Rect src_rect = {272, 16, 16, 16};
            if (i == test_index)
                src_rect.x = 368;
            else if (i < test_index && current_level->monitor_state == MONITOR_STATE_PLAY_ALL && !current_level->touched)
                src_rect.x = 368;
            SDL_Rect dst_rect = {panel_offset.x + (16 + i * 16) * scale, panel_offset.y + (32 + 8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
            render_score_2digit_err(XYPos(panel_offset.x + (16 + i * 16 + 3) * scale, panel_offset.y + (32 + 8 + 5) * scale), current_level->tests[i].last_score);
            render_score_2digit_err(XYPos(panel_offset.x + (16 + i * 16 + 3) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), current_level->tests[i].best_score);
        }

        if (editing_level)
        {
            SDL_Rect src_rect = {464, 160, 8, 16};
            SDL_Rect dst_rect = {panel_offset.x + (16 + int(test_count) * 16) * scale, panel_offset.y + (32 + 8) * scale, 16 * scale, 32 * scale};
            render_texture(src_rect, dst_rect);
        }
        else
        {
            switch (test_mode)
            {
                case TEST_MODE_ACCURACY:
                    render_number_pressure(XYPos(panel_offset.x + (16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 5) * scale), current_level->last_score);
                    render_number_pressure(XYPos(panel_offset.x + (16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), current_level->best_score);
                    break;
                case TEST_MODE_PRICE:
                    {
                        SDL_Rect src_rect = {144, 160, 5, 5};
                        SDL_Rect dst_rect = {panel_offset.x + int(16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 5) * scale, 5 * scale, 5 * scale};
                        render_texture(src_rect, dst_rect);

                        render_number_long(XYPos(panel_offset.x + (16 + test_count * 16 + 3 + 5) * scale, panel_offset.y + (32 + 8 + 5) * scale), current_level->last_price);

                        dst_rect = {panel_offset.x + int(16 + test_count * 16 + 3) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale, 5 * scale, 5 * scale};
                        render_texture(src_rect, dst_rect);
                        render_number_long(XYPos(panel_offset.x + (16 + test_count * 16 + 3 + 5) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), current_level->best_price);
                    }
                    break;
                case TEST_MODE_STEAM:
                    {
                        render_number_compact(XYPos(panel_offset.x + (16 + test_count * 16 + 3 + 5) * scale, panel_offset.y + (32 + 8 + 5) * scale), current_level->last_steam);
                        render_number_compact(XYPos(panel_offset.x + (16 + test_count * 16 + 3 + 5) * scale, panel_offset.y + (32 + 8 + 16 + 5) * scale), current_level->best_steam);
                    }
                    break;
            }
        }

        int sim_point_count = current_level->tests[test_index].sim_points.size();
        int sim_point_index = current_level->sim_point_index;
        int sim_point_offset = std::max(std::min(sim_point_count - 12, sim_point_index - 6), 0);

        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8) * scale), XYPos(8*32, 112), 4);
        int y_pos = panel_offset.y + (32 + 32 + 16) * scale;
        
        {
            SDL_Rect src_rect = {448, 144, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        if (sim_point_offset)
        {
            SDL_Rect src_rect = {448, 128, 8, 8};
            SDL_Rect dst_rect = {panel_offset.x + 28 * scale, y_pos + 4 * scale, 8 * scale, 8 * scale};
            render_texture(src_rect, dst_rect);
        }
        if (sim_point_count > (sim_point_offset + 12))
        {
            SDL_Rect src_rect = {456, 128, 8, 8};
            SDL_Rect dst_rect = {panel_offset.x + (28 + 11 * 16) * scale, y_pos + 4 * scale, 8 * scale, 8 * scale};
            render_texture(src_rect, dst_rect);
        }
        y_pos+= 16 * scale;
        for (int i = 0; i < 4; i++)                 // Inputs
        {
            int pin_index = current_level->pin_order[i];
            if ((pin_index >= 0) && (pin_index != current_level->tests[test_index].tested_direction))
            {
                SDL_Rect src_rect = {256 + pin_index * 16, 144, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + 8 * scale, y_pos, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
                for (int i2 = sim_point_offset; (i2 < sim_point_count) && (i2 < (sim_point_offset + 12)); i2++)
                {
                    unsigned value = current_level->tests[test_index].sim_points[i2].values[pin_index];
                    render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + (i2 - sim_point_offset) * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == i2 ? 4 : 0);
                }
                y_pos += 16 * scale;
            }
        }
        if (editing_level)
        {
            for (int i2 = sim_point_offset; (i2 < sim_point_count) && (i2 < (sim_point_offset + 12)); i2++)
            {
                if (current_level->tests[test_index].first_simpoint == i2)
                {
                    SDL_Rect src_rect = {352, 208, 16, 16};
                    SDL_Rect dst_rect = {panel_offset.x + (8 + 16 + (i2 - sim_point_offset) * 16) * scale, y_pos, 16 * scale, 16 * scale};
                    render_texture(src_rect, dst_rect);
                }
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

            for (int i2 = sim_point_offset; (i2 < sim_point_count) && (i2 < (sim_point_offset + 12)); i2++)
            {
                unsigned force = current_level->tests[test_index].sim_points[i2].force[pin_index];
                unsigned value = current_level->tests[test_index].sim_points[i2].values[pin_index];
                if (force || (i2 == sim_point_count-1))
                {
                    render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + (i2 - sim_point_offset) * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == i2 ? 4 : (force ? 3: 0));
                }
            }
            if ((sim_point_count  - sim_point_offset) <= 12)
            {
//                unsigned value = current_level->tests[test_index].sim_points[sim_point_count-1].values[pin_index];
//                render_number_2digit(XYPos(panel_offset.x + (8 + 16 + 3 + (sim_point_count - sim_point_offset - 1) * 16) * scale, y_pos + (5) * scale), value, 1, 9, current_level->sim_point_index == sim_point_count - 1 ? 4 : 0);
                if (editing_level)
                {
                    SDL_Rect src_rect = {464, 160, 8, 16};
                    SDL_Rect dst_rect = {panel_offset.x + (8 + 16 + 16 + (sim_point_count - sim_point_offset - 1) * 16) * scale, panel_offset.y + (32 + 32 + 32) * scale, 16 * scale, 32 * scale};
                    render_texture(src_rect, dst_rect);
                }
                else
                {
                    render_number_pressure(XYPos(panel_offset.x + (8 + 16 + 3 + 16 + (sim_point_count - sim_point_offset - 1) * 16) * scale, y_pos + (5) * scale), current_level->tests[test_index].last_pressure_log[HISTORY_POINT_COUNT - 1] , 1, 9, 1);
                }
            }
            y_pos += 16 * scale;
        }

        if (editing_level)
        {
            SDL_Rect src_rect = {368 + current_level->tests[test_index].reset * 16, 208, 16, 16};
            SDL_Rect dst_rect = {panel_offset.x + 156 * scale, panel_offset.y + (32 + 32 + 16) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);


            src_rect.x = 352;
            dst_rect.x = panel_offset.x + 132 * scale;
            render_texture(src_rect, dst_rect);
        }

        if (editing_level)
        {
            SDL_Rect src_rect = {464, 160, 8, 16};
            SDL_Rect dst_rect = {panel_offset.x + 116 * scale, panel_offset.y + (32 + 32 + 16) * scale, 8 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
            
            src_rect = {448, 136, 8, 8};
            dst_rect = {panel_offset.x + 108 * scale, panel_offset.y + (32 + 32 + 16 + 5) * scale, 8 * scale, 8 * scale};
            render_texture(src_rect, dst_rect);
            
            int c = int64_t(int64_t(current_level->substep_count) * PRESSURE_SCALAR) / 10000;
            
            render_number_pressure(XYPos(panel_offset.x + (number_high_precision ? 54 : 71) * scale, panel_offset.y + (32 + 32 + 16 + 3) * scale), c, 2, 9, 1);


        }
        if (editing_level)
        {
            for (int i = 0; i < 4; i++)                 // Editing selecting inputs 
            {
                SDL_Rect src_rect = {256 + i * 16, 144, 16, 16};
                SDL_Rect dst_rect = {panel_offset.x + (i * 16 + 180) * scale, panel_offset.y + (32 + 32 + 16) * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
                if ((current_level->connection_mask >> i) & 1)
                {
                    src_rect = {472, 160, 16, 16};
                    render_texture(src_rect, dst_rect);
                }
            }
        }


        {
            SDL_Rect src_rect = {320, 144, 16, 8};      // Highlight box
            SDL_Rect dst_rect = {panel_offset.x + int(8 + 16 + (sim_point_index  - sim_point_offset) * 16) * scale, panel_offset.y + (32 + 32 + 32) * scale, 16 * scale, 8 * scale};
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

    } else if (panel_state == PANEL_STATE_SCORES && !show_server_levels)
    {
        XYPos table_pos = XYPos((8 + 32 * 11), (8 + 8 + 32));
        
        Level* level;
        if (current_level_index < LEVEL_COUNT)
            level = edited_level_set->levels[current_level_index];
        else
        {
            int i = edited_level_set->find_custom_by_name(current_level->name);
            if (i < 0)
                level = current_level;
            else
                level = edited_level_set->levels[i];
        }
        friend_score_scroll.total_rows = level->friend_scores.size();

        render_scroll_bar(friend_score_scroll);


        for (int i = 0; i < 11; i++)
        {
            if (i >= level->friend_scores.size())
                break;
            Level::FriendScore& score = level->friend_scores[friend_score_scroll.offset_rows + i];
            render_text(table_pos+XYPos(16,0), score.steam_username.c_str());
            switch (test_mode)
            {
                case TEST_MODE_ACCURACY:
                    render_number_pressure((table_pos + XYPos(190,3)) * scale, score.score, 2);
                    break;
                case TEST_MODE_PRICE:
                {
                    SDL_Rect src_rect = {144, 160, 5, 5};
                    SDL_Rect dst_rect = {(table_pos.x + 190) * scale, (table_pos.y + 3) * scale, 10 * scale, 10 * scale};
                    render_texture(src_rect, dst_rect);
                    render_number_long((table_pos + XYPos(200,3)) * scale, score.score, 2);
                }
                    break;
                case TEST_MODE_STEAM:
                    render_number_long((table_pos + XYPos(190,3)) * scale, score.score, 2);
                    break;
            }
            if (score.visible)
            {
                SDL_Rect src_rect = {336, 32, 16, 16};
                SDL_Rect dst_rect = {(table_pos.x) * scale, table_pos.y * scale, 16 * scale, 16 * scale};
                render_texture(src_rect, dst_rect);
            }

            table_pos.y += 16;
        }

        render_box(XYPos(panel_offset.x, panel_offset.y + (32 + 32 + 8 + 112) * scale), XYPos(256, 120), 5);
        XYPos graph_pos(8 * scale + panel_offset.x, (32 + 32 + 8 + 112 + 9) * scale + panel_offset.y);
        {
            SDL_Rect src_rect = {524, 80, 13, 101};
            SDL_Rect dst_rect = {graph_pos.x, graph_pos.y, 13 * scale, 101 * scale};
            render_texture(src_rect, dst_rect);
        }
        
        if (level->global_score_graph_set)
        {
            int64_t my_score = level->global_fetched_score;
            for (int i = 0; i < 200-1; i++)
            {
                int top;
                int size;
                unsigned colour = 6;
                if (test_mode == TEST_MODE_ACCURACY)
                {
                    int v1 = pressure_as_percent(level->global_score_graph[i]);
                    int v2 = pressure_as_percent(level->global_score_graph[i + 1]);
                    if ((level->global_score_graph[i] >= my_score) && (level->global_score_graph[i + 1] <= my_score))
                        colour = 3;
                    top = 100 - std::max(v1, v2);
                    size = abs(v1 - v2) + 1;
                }
                else
                {
                    int64_t v1 = level->global_score_graph[i];
                    int64_t v2 = level->global_score_graph[i + 1];
                    if ((v1 <= my_score) && (v2 >= my_score))
                        colour = 3;
                    int64_t range = level->global_score_graph[199];
                    if (!range)
                        range = 1;
                    top = 100 - (std::max(v1, v2) * 100) / range;
                    int bottom = 100 - (std::min(v1, v2) * 100) / range;
                    size = bottom - top + 1;
                }

                SDL_Rect src_rect = {503, 80 + (int)colour, 1, 1};
                SDL_Rect dst_rect = {i * scale + graph_pos.x, top * scale + graph_pos.y, 1 * scale, size * scale};
                render_texture(src_rect, dst_rect);
            }
        }
        if (!level->global_score_graph_set || SDL_TICKS_PASSED(SDL_GetTicks(), level->global_score_graph_time + 1000 * 10))
        {
            level->global_score_graph_time = SDL_GetTicks();
            if (current_level_index >= LEVEL_COUNT)
            {
                if (level->global)
                    score_fetch(level->name);
            }
            else
                score_fetch(current_level_index);
        }

        XYPos pos = ((mouse - graph_pos) / scale);
        if (level->global_score_graph_set && pos.y >= 0 && pos.x >= 0 && pos.x < 200)
        {
            if (test_mode == TEST_MODE_ACCURACY)
                tooltip_string = std::to_string(pressure_as_percent_float(level->global_score_graph[pos.x]));
            else
                tooltip_string = std::to_string(level->global_score_graph[pos.x]);
        }

    } else if (panel_state == PANEL_STATE_SCORES && show_server_levels)
    {
        XYPos table_pos = XYPos((8 + 32 * 11), (8 + 8 + 32));
        
        friend_score_scroll.total_rows = server_levels.size();

        render_scroll_bar(friend_score_scroll);

        for (int i = 0; i < 11; i++)
        {
            if (i >= server_levels.size())
                break;
            ServerLevel& level = server_levels[friend_score_scroll.offset_rows + i];
            render_text(table_pos+XYPos(16,0), level.name.c_str());
            SDL_Rect src_rect = {336, 32, 16, 16};
            SDL_Rect dst_rect = {(table_pos.x) * scale, table_pos.y * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
            table_pos.y += 16;
        }

        if (SDL_TICKS_PASSED(SDL_GetTicks(), server_levels_time + 1000 * 10))
        {
            server_levels_time = SDL_GetTicks();
            server_levels_fetch();
        }
    }


    if (show_confirm)
    {
        render_button((confirm_box_pos + XYPos(0,0))*scale, XYPos(352, 184), 1, "Confirm");
        render_button((confirm_box_pos + XYPos(32,0))*scale, XYPos(376, 184), 0, "Cancel");
    }

    render_tooltip();

    if (show_debug)
    {
        render_number_2digit(XYPos(0, 0), debug_last_second_frames, 3);
        render_number_long(XYPos(0, 3 * 7 * scale), debug_last_second_simticks, 3);
    }
    if ((show_dialogue || show_dialogue_hint || show_dialogue_discord_prompt) && !display_language_dialogue)
    {
        enum
        {
            DIALOGUE_CHARLES,
            DIALOGUE_NICOLA,
            DIALOGUE_ADA,
            DIALOGUE_ANNIE,
            DIALOGUE_GEORGE,
            DIALOGUE_JOSEPH
        } character = DIALOGUE_CHARLES;
        std::string text;
        int dialogue_index_max = 1;
        if (show_dialogue_discord_prompt)
        {
            character = DIALOGUE_CHARLES;
            text = "If you are stuck, consider discussing your challenges with the brightest minds of our High Pressure Steam Society on Discord. Press Esc and click the \"Join our Discord group\" button.";
        }
        else
        {
            bool fail = true;
            std::string who = "charles";
            if (current_level_index < LEVEL_COUNT)
            {
                const char* key = show_dialogue_hint ? "hints" : "dialogue";
                if (current_language->has_key(key) && current_language->get_item(key)->is_list())
                {
                    SaveObjectList* lis = current_language->get_item(key)->get_list()->get_item(current_level_index)->get_list();
                    dialogue_index_max = lis->get_count();
                    if (dialogue_index < dialogue_index_max)
                    {
                        SaveObjectMap* dia = lis->get_item(dialogue_index)->get_map();
                        text = dia->get_string("text");
                        who = dia->get_string("who");
                        fail = false;
                    }
                }
            }
            else
            {
                dialogue_index_max = current_level->dialogue.size();
                if (dialogue_index >= dialogue_index_max)
                {
                    std::list<Level::DialogueScreen>::iterator it = current_level->dialogue.begin();
                    std::advance(it, dialogue_index);
                    who = it->who;
                    text = it->text;
                    fail = false;
                }
            }
            
            if (fail)
            {
                dialogue_index = 0;
                show_dialogue_hint = false;
                show_dialogue = false;
            }
            else
            {
                character = DIALOGUE_CHARLES;
                if (who == "charles")
                    character = DIALOGUE_CHARLES;
                else if (who == "nicola")
                    character = DIALOGUE_NICOLA;
                else if (who == "ada")
                    character = DIALOGUE_ADA;
                else if (who == "annie")
                    character = DIALOGUE_ANNIE;
                else if (who == "george")
                    character = DIALOGUE_GEORGE;
                else if (who == "joseph")
                    character = DIALOGUE_JOSEPH;
            }
        }
        if (show_dialogue || show_dialogue_hint || show_dialogue_discord_prompt)
        {
        
            if (current_language->get_item("tooltips")->get_map()->has_key(text))
                text = current_language->get_item("tooltips")->get_map()->get_string(text);

            if ((show_dialogue || show_dialogue_hint || show_dialogue_discord_prompt) && current_level_index < LEVEL_COUNT)
            {
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
                render_box(XYPos(16 * scale, (180) * scale), XYPos(32, 32), 4);
                render_number_2digit(XYPos((16 + 8) * scale, (180 + 4) * scale), dialogue_index + 1, 2);
                render_box(XYPos((16 + 32) * scale, (180) * scale), XYPos(32, 32), 4);
                render_number_2digit(XYPos((16 + 32 + 8) * scale, (180 + 4) * scale), dialogue_index_max, 2);

                render_box(XYPos(16 * scale, (180 + 16) * scale), XYPos(640-32, 180-32), 4);

                render_box(XYPos((pic_on_left ? 16 : 640 - 180 + 16) * scale, (180 + 16) * scale), XYPos(180-32, 180-32), 0);
                SDL_Rect src_rect = {640-256 + pic_src.x * 128, 480 + pic_src.y * 128, 128, 128};
                SDL_Rect dst_rect = {pic_on_left ? 24 * scale : (640 - 24 - 128) * scale, (180 + 24) * scale, 128 * scale, 128 * scale};
                render_texture(src_rect, dst_rect);
                render_text_wrapped(XYPos(pic_on_left ? 48 + 120 : 24, 180 + 24), text.c_str(), 640 - 80 - 110);

            }
        }
    }
    
    if (push_in_animation)
    {
        Circuit* sub_circuit = current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]->get_subcircuit();

        for (pos.y = 0; pos.y < 9; pos.y++)
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            SDL_Rect src_rect = {256, 480, 32, 32};
            if (pos.y > 0)
                src_rect.y += 32;
            if (pos.y == 8)
                src_rect.y += 32;
            if (pos.x > 0)
                src_rect.x += 32;
            if (pos.x == 8)
                src_rect.x += 32;
            if (!current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]->get_custom() || current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]->get_read_only())
                src_rect.y += 96;
            else
                src_rect.y += 96 * 2;

            SDL_Rect dst_rect = {((32 - push_in_animation) * push_in_animation_grid.x + pos.x * push_in_animation) * scale + grid_offset.x, ((32 - push_in_animation) * push_in_animation_grid.y + pos.y * push_in_animation) * scale + grid_offset.y, push_in_animation * scale, push_in_animation * scale};
            render_texture(src_rect, dst_rect);

            if (sub_circuit->is_blocked(pos))
            {
                src_rect = {384, 80, 32, 32};
                render_texture(src_rect, dst_rect);
            }

            {
                SDL_Rect src_rect = sub_circuit->elements[pos.y][pos.x]->getimage_bg();
                if (src_rect.w)
                {
                    int xoffset = ((32 - src_rect.w + 1) * push_in_animation) / (2 * 32);
                    int yoffset = ((32 - src_rect.h + 1) * push_in_animation) / (2 * 32);
                    SDL_Rect dst_rect = {((32 - push_in_animation) * push_in_animation_grid.x + pos.x * push_in_animation  + xoffset) * scale + grid_offset.x, ((32 - push_in_animation) * push_in_animation_grid.y + pos.y * push_in_animation  + yoffset) * scale + grid_offset.y, ((src_rect.w + 1) * push_in_animation) / 32 * scale, ((src_rect.h + 1) * push_in_animation) / 32 * scale};
                    render_texture(src_rect, dst_rect);
                }
            }

            XYPos src_pos = sub_circuit->elements[pos.y][pos.x]->getimage();
            if (src_pos != XYPos(-1,-1))
            {
                SDL_Rect src_rect = {src_pos.x, src_pos.y, 32, 32};
                render_texture(src_rect, dst_rect);
            }

            src_pos = sub_circuit->elements[pos.y][pos.x]->getimage_fg();
            if (src_pos != XYPos(-1,-1))
            {
                WrappedTexture* w_tex = sub_circuit->elements[pos.y][pos.x]->getimage_fg_texture();
                SDL_Texture* tex = w_tex ? w_tex->get_texture() : sdl_texture ;
                SDL_Rect src_rect =  {src_pos.x, src_pos.y, 24, 24};
                SDL_Rect dst_rect = {((32 - push_in_animation) * push_in_animation_grid.x + pos.x * push_in_animation + push_in_animation / 8) * scale + grid_offset.x, ((32 - push_in_animation) * push_in_animation_grid.y + pos.y * push_in_animation + push_in_animation / 8) * scale + grid_offset.y, push_in_animation * scale * 3 / 4, push_in_animation * scale * 3 / 4};
                render_texture_custom(tex, src_rect, dst_rect);
            }
        }


        push_in_animation++;
        if (push_in_animation >= 32)
        {
            if (!current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]->get_custom() || current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]->get_read_only())
                current_circuit_is_read_only = true;
            inspection_stack.push_back(current_circuit->elements[push_in_animation_grid.y][push_in_animation_grid.x]);
            current_circuit = sub_circuit;
            current_circuit_is_inspected_subcircuit = true;
            selected_elements.clear();
            push_in_animation = 0;
            mouse_state = MOUSE_STATE_NONE;

        }

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
        if (show_help_page >= next_help_highlight)
            next_help_highlight = show_help_page + 1;

        for (int i = 0; i < 12; i++)
        {
            if (i >= max_help_page(next_dialogue_level))
                break;
            if (i == show_help_page)
                render_box(XYPos((32 + 48 + i * 32) * scale, (2 * (128 + 16) + 0) * scale), XYPos(32, 32+28), 1);
            else
                render_box(XYPos((32 + 48 + i * 32) * scale, (2 * (128 + 16) + 28) * scale), XYPos(32, 32), (i >= next_help_highlight) && (frame_index % 60 < 30));
            SDL_Rect src_rect = {352 + (i % 5) * 24, 224 + (i / 5) * 24, 24, 24};
            SDL_Rect dst_rect = {(32 + 48 + 4 + i * 32) * scale, (2 * (128 + 16) + 4 + 28) * scale, 24 * scale, 24 * scale};
            render_texture(src_rect, dst_rect);
        }

        {
            SDL_Rect src_rect = {464, 144, 16, 16};
            SDL_Rect dst_rect = {(32 + 512 + 32 + 8) * scale, (8) * scale, 16 * scale, 16 * scale};
            render_texture(src_rect, dst_rect);
        }
        if (show_help_page > 11)
            show_help_page = 11;

        for (int i = 0; i < 2; i++)
        {
            render_box(XYPos((16 + 8 + i * 48) * scale, (i * (128 +16) + 8) * scale), XYPos(128+16, 128+16), 4);
            render_box(XYPos((128 + 16 + 16 + 8 + i * 48) * scale, (i * (128 +16) + 8) * scale), XYPos(384, 128+16), 4);
            unsigned anim_frames = 5;
            
            struct HelpPage
            {
                XYPos src_pos;
                int frame_count;
                float fps;
            }
            pages[] =
            {
                {XYPos(0,14), 4, 1},
                {XYPos(0,13),5,0.5},
                {XYPos(0,0), 10, 1},
                {XYPos(0,2),  5, 1},
                {XYPos(0,4), 15, 1},
                {XYPos(0,8),  5, 1},
                {XYPos(0,7),  5,10},
                {XYPos(1,10), 4,10},
                {XYPos(0,9),  5,10},
                {XYPos(0,10), 1, 1},
                {XYPos(0,11), 5,10},
                {XYPos(0,12), 1, 1},
                {XYPos(1,12), 1, 1},
                {XYPos(2,12), 3, 1},
                {XYPos(4,14), 1, 1},
                {XYPos(0,15), 1, 1},
                {XYPos(1,15), 4, 1},
                {XYPos(0,16), 1, 1},
                {XYPos(1,16), 2, 1},
                {XYPos(3,16), 1, 1},
                {XYPos(4,16), 1, 1},
                {XYPos(0,17), 5,10},
                {XYPos(0,18), 1, 1},
                {XYPos(1,18), 1, 1},
            };
            

            HelpPage* page = &pages[show_help_page * 2 + i];
            std::string text = current_language->get_item("help")->get_list()->get_string(show_help_page * 2 + i).c_str();

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
            
            render_text_wrapped(XYPos(32 + 128 + 14 + i * 48, i * (128 +16) + 11), text.c_str(), 384 - 8);
        }
    }

    if (display_language_dialogue)
    {
        render_box(XYPos(160 * scale, 90 * scale), XYPos(320, 200), 0);
        int i = 0;
        int col = 0;
        for (std::map<std::string, SaveObject*>::iterator it = languages->omap.begin(); it != languages->omap.end(); ++it)
        {
            render_box(XYPos((160 + 32 + col * 160) * scale, (90 + 16 + i * 24) * scale), XYPos(160 - 64, 24), 0);
            render_text(XYPos((160 + 32 + 4 + col * 160), (90 + 16 + i * 24 + 5)), it->first.c_str());
            i++;
            if (i >= 7)
            {
                col++;
                i = 0;
            }
        }
    }
    else if (show_main_menu)
    {
        render_box(XYPos(160 * scale, 90 * scale), XYPos(320, 200), 0);
        if (!display_about)
        {
            render_button(XYPos((160 + 32) * scale, (90 + 32)  * scale), XYPos(448, 200), 0, "Exit");
            render_button(XYPos((160 + 32 + 64) * scale, (90 + 32)  * scale), XYPos(full_screen ? 280 : 256, 280), 0, full_screen ? "Windowed" : "Full Screen");

            render_button(XYPos((160 + 32) * scale, (90 + 32 + 48)  * scale), XYPos(256, 256), 0, "Join our Discord group");
            render_button(XYPos((160 + 32 + 64) * scale, (90 + 32 + 48)  * scale), XYPos(256+24, 256), 0, "Info");

            render_button(XYPos((160 + 32) * scale, (90 + 96 + 32)  * scale), XYPos(280, 304), 0, "Select language");
            if (highest_level >= 20)
                render_button(XYPos((160 + 32 + 64) * scale, (90 + 96 + 32)  * scale), XYPos(number_high_precision ? 280 : 256, 400), 0, "Number resolution");

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
            const char* about_text = "Created by Charlie Brej\n\nMusic by stephenpalmermail\nGraphic assets by Carl Olsson\nSilver font by Poppy Works\nTranslations\n    Chinese:    Leaving Leaves\n    Japaneese: ATALOSS(\xe3\x82\xa2\xe3\x83\x88)\n    Czech:      Detros\n    French:    Akiel & Lemnis\n\nBuild: " 
#include "date.string"
            ;
            render_text_wrapped(XYPos(160 + 32 + 4, 90 + 16 + 4), about_text, 320 - 64);
        }
        render_tooltip();
    }

    if (saving)
    {
        SDL_Rect src_rect = {256, 305, 24, 24};
        SDL_Rect dst_rect = {0,0, 24 * scale, 24 * scale};
        render_texture(src_rect, dst_rect);
    }

    SDL_RenderPresent(sdl_renderer);
}

void GameState::set_level(int level_index)
{
    dialogue_index = 0;
    editing_level = false;
    if (!level_set->is_playable(level_index, highest_level))
        level_index = last_edited_level_index;
    while (!level_set->is_playable(level_index, highest_level))
    {
        if (level_index == 0)
            break;
        level_index--;
    }
    if (!level_set->is_playable(level_index, highest_level))
    {
        while (!level_set->is_playable(level_index, highest_level))
        {
            level_index++;
        }
    }
    if (level_set == edited_level_set)
        last_edited_level_index = level_index;
    
    current_circuit_is_inspected_subcircuit = false;
    current_circuit_is_read_only = current_level_set_is_inspected;

    mouse_state = MOUSE_STATE_NONE;
    skip_to_next_subtest = false;
    current_level_index = level_index;
    current_level = level_set->levels[current_level_index];
    current_circuit = current_level->circuit;
    current_circuit->elaborate(level_set);
    level_set->remove_circles(current_level_index);
    inspection_stack.clear();
    level_set->reset(current_level_index);
    show_hint = false;
    selected_elements.clear();
    show_dialogue = false;
    show_dialogue_hint = false;
    if (level_index == next_dialogue_level && !current_level_set_is_inspected &&  (current_level_index < LEVEL_COUNT))
    {
        show_dialogue = true;
        next_dialogue_level++;
    }
}

void GameState::set_level(std::string name)
{
    int found = level_set->find_custom_by_name(name);
    if (found < 0)
        set_level(current_level_index);
    else
        set_level(found);
}

void GameState::click_scroll_bar(ScrollBar& sbar, XYPos mouse_click)
{
    int total_size = sbar.height - 32;
    int bar_size = total_size;
    if (sbar.total_rows > sbar.visible_rows)
        bar_size = (total_size * sbar.visible_rows) / sbar.total_rows;
    int bar_offset = (total_size * sbar.offset_rows)/ sbar.total_rows + 16;

    if (!mouse_click.inside(XYPos(16, sbar.height)))
        return;
    if (mouse_click.y < 16)
    {
        sbar.offset_rows--;
    }
    else if (mouse_click.y > (sbar.height - 16))
    {
        sbar.offset_rows++;
    }
    else if (mouse_click.y < bar_offset)
    {
        sbar.offset_rows -= sbar.visible_rows;
    }
    else if (mouse_click.y < bar_offset + bar_size)
    {
        mouse_state = MOUSE_STATE_SCROLL_BAR_DRAG;
        scroll_drag_y = mouse_click.y - bar_offset;
        dragged_scroll_bar = &sbar;
    }
    else
    {
        sbar.offset_rows += sbar.visible_rows;
    }
    normalize_scroll_bar(sbar);
}

void GameState::normalize_scroll_bar(ScrollBar& sbar)
{
    if (sbar.total_rows == 0)
        sbar.total_rows = 1;
    if ((sbar.offset_rows + sbar.visible_rows) > sbar.total_rows)
        sbar.offset_rows = sbar.total_rows - sbar.visible_rows;
    if (sbar.offset_rows < 0)
        sbar.offset_rows = 0; 
}

void GameState::mouse_click_in_grid(unsigned clicks)
{
    if (mouse_state == MOUSE_STATE_ANIMATING)
        return;
    XYPos pos = (mouse - grid_offset) / scale;
    XYPos grid = pos / 32;
    if (pos.x < 0) grid.x--;
    if (pos.y < 0) grid.y--;

    if (!keyboard_ctrl && !keyboard_shift && clicks == 2)
    {
        if (grid.inside(XYPos(9,9)))
        {
            Circuit* sub_circuit = current_circuit->elements[grid.y][grid.x]->get_subcircuit();
            if (sub_circuit)
            {
                mouse_state = MOUSE_STATE_ANIMATING;
                push_in_animation = 1;
                push_in_animation_grid = grid;
                return;
            }
        }
    }


    {
        XYPos pos = mouse/scale;
        if (pos.inside(XYPos(32*11,32)))
        {
            unsigned i = pos.x / 32;
            
            if (i == 0)
            {
                inspection_stack.clear();
                current_circuit_is_read_only = current_level_set_is_inspected;
                current_circuit_is_inspected_subcircuit = false;
                current_circuit = current_level->circuit;
                selected_elements.clear();
                return;
            }
            else if (inspection_stack.size() > i)
            {
                inspection_stack.resize(i);
                i--;
                current_circuit = inspection_stack[i]->get_subcircuit();
                if (!current_level_set_is_inspected)
                    current_circuit_is_read_only = false;
                for (auto &sub : inspection_stack)
                    if (!sub->get_custom() || sub->get_read_only())
                        current_circuit_is_read_only = true;
                selected_elements.clear();
                return;
            }
            else if (i == 10 && current_circuit_is_inspected_subcircuit && !current_level_set_is_inspected)
            {
                Circuit* prev = current_level->circuit;
                for (auto &sub : inspection_stack)
                {
                    if (sub->get_read_only())
                        break;
                    if (!sub->get_custom())
                    {
                        if (sub == inspection_stack.back())
                        {
                            prev->ammend();
                            sub->set_custom();
                            current_circuit_is_read_only = false;
                        }
                        break;
                    }
                    prev = sub->get_subcircuit();
                }
                return;
            }
            else if (i == 10 && current_level_set_is_inspected)
            {
                current_level_set_is_inspected = false;
                std::string level_name = current_level->name;
                if (free_level_set_on_return)
                {
                    delete level_set;
                    free_level_set_on_return = false;
                }
                deletable_level_set = NULL;
                set_level_set(edited_level_set);
                set_level(level_name);
                return;
            }
            else if (i == 10 && (editing_level))
            {
                editing_level = false;
                return;
            }
            else if (i == 9 && (editing_level) && !current_circuit_is_inspected_subcircuit)
            {
                show_confirm = true;
                confirm_what = CONFIRM_DELETE_LEVEL;
                confirm_box_pos = XYPos(32*9 - 16, 32);
                return;
            }
            else if (i == 10 && (current_level->global) && !current_circuit_is_inspected_subcircuit && !current_level_set_is_inspected && (current_level_index >= LEVEL_COUNT))
            {
                show_confirm = true;
                confirm_what = CONFIRM_DELETE_LEVEL;
                confirm_box_pos = XYPos(32*10 - 16, 32);
                return;
            }
            else if (i == 10 && (next_dialogue_level > 24 && !current_level->global) && (current_level_index >= LEVEL_COUNT))
            {
                editing_level = true;
                pixel_colour = 0;
                editing_icon_index = -1;
                icon_rotate = true;
                current_level->current_simpoint = current_level->tests[current_level->test_index].sim_points[current_level->sim_point_index];
                return;
            }
            else if (i == 9 && current_level_set_is_inspected && !current_circuit_is_inspected_subcircuit)
            {
                if (current_level_index >= LEVEL_COUNT)
                {
                    int found = edited_level_set->find_custom_by_name(current_level->name);
                    if (found < 0)
                        return;
                    current_level_index = found;
                }
                edited_level_set->levels[current_level_index]->circuit->ammend();

                if (keyboard_shift)
                {
                    Circuit tmp_circuit(*current_circuit);
                    tmp_circuit.elaborate(level_set);
                    tmp_circuit.set_custom(true);
                    edited_level_set->levels[current_level_index]->circuit->copy_elements(tmp_circuit);
                }
                else
                {
                    edited_level_set->levels[current_level_index]->circuit->copy_elements(*current_circuit);
                }
                edited_level_set->remove_circles(current_level_index);
                edited_level_set->levels[current_level_index]->circuit->elaborate(edited_level_set);
                edited_level_set->touch(current_level_index);
                current_level_set_is_inspected = false;
                if (free_level_set_on_return)
                {
                    delete level_set;
                    free_level_set_on_return = false;
                }
                deletable_level_set = NULL;
                set_level_set(edited_level_set);
                set_level(current_level_index);
                return;
            }
            else if (i == 8 && current_level_set_is_inspected && !current_circuit_is_inspected_subcircuit && deletable_level_set)
            {
                show_confirm = true;
                confirm_what = CONFIRM_DELETE;
                confirm_box_pos = XYPos(32*8 - 16, 32);
                return;
            }
            else if (i == 7 && current_level_set_is_inspected && !current_circuit_is_inspected_subcircuit && current_level_index >= LEVEL_COUNT)
            {
                unsigned new_index = edited_level_set->import_level(level_set, current_level_index);
                current_level_set_is_inspected = false;
                if (free_level_set_on_return)
                {
                    delete level_set;
                    free_level_set_on_return = false;
                }
                deletable_level_set = NULL;
                set_level_set(edited_level_set);
                set_level(new_index);
                return;
            }
        }
    }

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
    
    if (current_circuit_is_read_only)
        return;

    if ((next_dialogue_level > 1) && (panel_state == PANEL_STATE_LEVEL_SELECT))
        panel_state = PANEL_STATE_EDITOR;

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
            if (mouse_rel.y < 0 && (pipe_start_grid_pos.y == 0 || current_circuit->is_blocked(pipe_start_grid_pos + XYPos(0,-1))))
                mouse_rel.y = -mouse_rel.y;
            if (mouse_rel.y >= 0 && (pipe_start_grid_pos.y == 9 || current_circuit->is_blocked(pipe_start_grid_pos)))
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
            if (mouse_rel.x < 0 && (pipe_start_grid_pos.x == 0 || current_circuit->is_blocked(pipe_start_grid_pos + XYPos(-1,0))))
                mouse_rel.x = -mouse_rel.x;
            if (mouse_rel.x >= 0 && (pipe_start_grid_pos.x == 9 || current_circuit->is_blocked(pipe_start_grid_pos)))
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
        level_set->touch(current_level_index);
    }
    else if (mouse_state == MOUSE_STATE_PLACING_VALVE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            current_circuit->set_element_valve(mouse_grid, dir_flip);
            level_set->touch(current_level_index);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SOURCE)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            current_circuit->set_element_source(mouse_grid, dir_flip.get_n());
            level_set->touch(current_level_index);
        }
    }
    else if (mouse_state == MOUSE_STATE_PLACING_SUBCIRCUIT)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)) && !current_circuit->is_blocked(mouse_grid))
        {
            XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
            current_circuit->set_element_subcircuit(mouse_grid, dir_flip, placing_subcircuit_level, edited_level_set);
            level_set->remove_circles(current_level_index);
            level_set->touch(current_level_index);
        }
    }
    else if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        
        mouse_grid.x = std::max(std::min(mouse_grid.x, 9 - clipboard.size().x), 0);
        mouse_grid.y = std::max(std::min(mouse_grid.y, 9 - clipboard.size().y), 0);
        current_circuit->paste(clipboard, mouse_grid, edited_level_set);
        level_set->touch(current_level_index);

    }
    else if (mouse_state == MOUSE_STATE_PLACING_SIGN)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale);
        if (mouse_grid.inside(XYPos(9*32,9*32)))
        {
            Sign sign(mouse_grid, dir_flip.get_n(), "");
            current_circuit->add_sign(sign);
        }
    }
    else if (mouse_state == MOUSE_STATE_DELETING)
    {
        current_circuit->undo(level_set);
        selected_elements.clear();
        first_deletion = true;
    }
    else if (mouse_state == MOUSE_STATE_LOCKING_BLOCKS)
    {
        XYPos mouse_grid = ((mouse - grid_offset) / scale) / 32;
        if (mouse_grid.inside(XYPos(9,9)))
        {
            bool blocked = !current_circuit->blocked[mouse_grid.y][mouse_grid.x];
            current_circuit->blocked[mouse_grid.y][mouse_grid.x] = blocked;
            current_circuit->elements[mouse_grid.y][mouse_grid.x]->set_read_only(blocked);
            level_set->touch(current_level_index);
        }
    }
    else
    {
        assert(0);
    }
}

void GameState::mouse_click_in_panel(unsigned clicks)
{
    if (mouse_state == MOUSE_STATE_ANIMATING)
        return;
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
                    if (!(next_dialogue_level > 1) || current_circuit_is_read_only)
                        break;
                    panel_state = PANEL_STATE_EDITOR;
                    flash_editor_menu = false;
                    break;
                case 2:
                    if (!(next_dialogue_level > 4))
                        break;
                    panel_state = PANEL_STATE_MONITOR;
                    break;
                case 3:
                    if (!(next_dialogue_level > 6))
                        break;
                    panel_state = PANEL_STATE_TEST;
                    break;
                case 4:
                    panel_state = PANEL_STATE_SCORES;
                    show_server_levels = (clicks > 1);
                    break;
                case 5:
                case 6:
                {
                    if (next_dialogue_level > 4)
                        watch_slider(panel_offset.x + (5 * 32 + 8) * scale, DIRECTION_E, 49,  &game_speed);
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

    if (panel_state == PANEL_STATE_LEVEL_SELECT && !editing_level)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        XYPos button_pos = panel_pos - XYPos(0,144);
        if (!level_select_requirements_visible)
            button_pos.y -= 128;
        
        int level_index = panel_grid_pos.x + panel_grid_pos.y * 8 + level_select_scroll.offset_rows * 8;

        if (panel_grid_pos.x >= 8)
        {
            click_scroll_bar(level_select_scroll, panel_pos - XYPos(8*32, 0));
        }
        else if ((panel_grid_pos.y < (level_select_requirements_visible ? 4 : 8)) && level_set->is_playable(level_index, highest_level))
        {
            set_level(level_index);
        }
        else if (button_pos.y > 176)
        {
            show_hint = true;
        }
        else if ((button_pos).inside(XYPos(32,32)) && (current_level_index < LEVEL_COUNT))   // Blah
        {
            show_dialogue = true;
            
        }
        else if ((button_pos - XYPos(32,0)).inside(XYPos(32,32)) && (current_level_index < LEVEL_COUNT))   // Hint
        {
            show_dialogue_hint = true;
            dialogue_index = 0;
            
        }
        else if ((button_pos - XYPos(64,0)).inside(XYPos(32,32)) && (current_level_index < LEVEL_COUNT)
                 && tip_revealed >= current_level_index
                 && edited_level_set->levels[current_level_index]->help_design)   // Help
        {
            if (free_level_set_on_return)
                delete level_set;
            deletable_level_set = NULL;
            set_level_set(edited_level_set->levels[current_level_index]->help_design);
            free_level_set_on_return = false;
            set_current_circuit_read_only();
            current_level_set_is_inspected = true;
            set_level(current_level_index);
        }
        else if ((button_pos - XYPos(96,0)).inside(XYPos(32,32)))   // Requirements
        {
            level_select_requirements_visible = !level_select_requirements_visible;
        }
        else if ((button_pos - XYPos(6*32,0)).inside(XYPos(32,32)) && next_dialogue_level > 32)  // Scoring mode
        {
            scoring_mode_dialogue = true;
        }
        else if ((button_pos - XYPos(7*32,0)).inside(XYPos(32,32)) && next_dialogue_level > 24 && !current_circuit_is_read_only)  // New
        {
            set_level(edited_level_set->new_user_level());
            set_level_set(edited_level_set);
        }
        return;
    } else if (panel_state == PANEL_STATE_LEVEL_SELECT && editing_level)
    {
        if (panel_pos.y < 32)
        {
            mouse_state = MOUSE_STATE_ENTERING_TEXT;
            text_entry_target = TEXT_TARGET_LEVEL_NAME;
            text_entry_string = &current_level->name;
            text_entry_offset = current_level->name.size();

            SDL_StartTextInput();
            return;
        }
        
        XYPos palette_pos = panel_pos - XYPos(12 * 16 + 16 + 8, 32 + 8);
        if (palette_pos.inside(XYPos(16, 12*16)))
        {
            int index = palette_pos.y / 16;
            if (index < 8)
            {
                pixel_colour = index;
            }

            if (index == 10)
                icon_rotate = !icon_rotate;
            return;
        }

        XYPos pixel_pos = panel_pos - XYPos(8, 32 + 8);
        if (pixel_pos.inside(XYPos(24*8, 24*8)))
        {
            pixel_pos /= 8;
            if (keyboard_ctrl)
            {
                int new_pixel_colour;
                if (editing_icon_index == -1)
                    new_pixel_colour = level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x];
                else
                    new_pixel_colour = level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24];
                if (new_pixel_colour != 8)
                    pixel_colour = new_pixel_colour;
                return;
            }
            if (editing_icon_index == -1)
            {
                bool changed = false;
                for (int i = 0; i < 8; i++)
                {
                    XYPos npos = icon_rotate ? DirFlip(i).trans(pixel_pos, 24) : pixel_pos;
                    if (pixel_colour != level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24])
                    {
                        changed = true;
                        level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24] = pixel_colour;
                    }
                }
                if (changed)
                    set_level_set(level_set);
            }
            else
            {
                if (level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] != pixel_colour)
                {
                    level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] = pixel_colour;
                    set_level_set(level_set);
                }
            }
            return;
        }
        XYPos direction_pos = panel_pos - XYPos(4, 4 + 32 + 12*16+16);
        if (direction_pos.inside(XYPos(8*24, 24)))
        {
            int new_dir = direction_pos.x / 24;
            if (new_dir == editing_icon_index)
                editing_icon_index = -1;
            else
                editing_icon_index = new_dir;
            return;
        }
        if ((panel_pos - XYPos(0, 32 + 32 + 12*16+16)).inside(XYPos(32,32)))
        {
            mouse_state = MOUSE_STATE_ENTERING_TEXT;
            text_entry_string = &level_set->levels[current_level_index]->description;
            text_entry_offset = text_entry_string->size();
            text_entry_target = TEXT_TARGET_LEVEL_DESCRIPTION;
            SDL_StartTextInput();
        }

        if ((panel_pos - XYPos(32, 32 + 32 + 12*16+16)).inside(XYPos(32,32)))
        {
            mouse_state = MOUSE_STATE_ENTERING_TEXT;
            text_entry_string = &level_set->levels[current_level_index]->description;
            text_entry_offset = text_entry_string->size();
            text_entry_target = TEXT_TARGET_LEVEL_DIALOGUE;
            SDL_StartTextInput();
        }


    } else if (panel_state == PANEL_STATE_EDITOR && !current_circuit_is_read_only)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        if (panel_grid_pos.y == 0)
        {
            if (panel_grid_pos.x == 0 && (next_dialogue_level > 2))
                mouse_state = MOUSE_STATE_PLACING_VALVE;
            else if (panel_grid_pos.x == 1)
                mouse_state = MOUSE_STATE_PLACING_SOURCE;
            else if (panel_grid_pos.x == 2 && next_dialogue_level > 1)
            {
                dir_flip = dir_flip.rotate(false);
                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                    clipboard.rotate(false);
                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                    dragged_sign.rotate(false);
                if (!selected_elements.empty())
                {
                    current_circuit->rotate_selected_elements(selected_elements, false);
                    level_set->touch(current_level_index);
                }
            }
            else if (panel_grid_pos.x == 3 && next_dialogue_level > 1)
            {
                dir_flip = dir_flip.rotate(true);
                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                    clipboard.rotate(true);
                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                    dragged_sign.rotate(true);
                if (!selected_elements.empty())
                {
                    current_circuit->rotate_selected_elements(selected_elements, true);
                    level_set->touch(current_level_index);
                }
            }
            else if (panel_grid_pos.x == 4 && next_dialogue_level > 6)
            {
                dir_flip = dir_flip.flip(true);
                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                    clipboard.flip(true);
                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                    dragged_sign.flip(true);
                if (!selected_elements.empty())
                {
                    current_circuit->flip_selected_elements(selected_elements, true);
                    level_set->touch(current_level_index);
                }
            }
            else if (panel_grid_pos.x == 5 && next_dialogue_level > 6)
            {
                dir_flip = dir_flip.flip(false);
                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                    clipboard.flip(false);
                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                    dragged_sign.flip(true);
                if (!selected_elements.empty())
                {
                    current_circuit->flip_selected_elements(selected_elements, false);
                    level_set->touch(current_level_index);
                }
            }
            else if (panel_grid_pos.x == 6 && editing_level)
                mouse_state = MOUSE_STATE_LOCKING_BLOCKS;
            else if (panel_grid_pos.x == 6 && !editing_level)
                editor_requirements_visible = !editor_requirements_visible;
            else if (panel_grid_pos.x == 7 && next_dialogue_level > 8)
                mouse_state = MOUSE_STATE_PLACING_SIGN;
            return;
        }
        panel_grid_pos = (panel_pos - XYPos(0, 32 + 8)) / 32;
        if (panel_grid_pos.x >= 8)
        {
            click_scroll_bar(editor_scroll, panel_pos - XYPos(8 * 32, 32 + 8));
        }
        else if (panel_grid_pos.y < (editor_requirements_visible ? 4 : 8))
        {
            int level_index = panel_grid_pos.x + panel_grid_pos.y * 8 + editor_scroll.offset_rows * 8;

            if ((level_index != current_level_index) && edited_level_set->is_playable(level_index, highest_level) && !edited_level_set->levels[level_index]->circuit->contains_subcircuit_level(current_level_index, edited_level_set))
            {
                mouse_state = MOUSE_STATE_PLACING_SUBCIRCUIT;
                placing_subcircuit_level = level_index;
            }
        }
        return;
    } else if (panel_state == PANEL_STATE_MONITOR)
    {
        XYPos panel_grid_pos = panel_pos / 32;
        if (panel_grid_pos.y == 0)
        {
            if (panel_grid_pos.x == 0)
            {
                current_level->set_monitor_state(MONITOR_STATE_PAUSE);
                level_set->touch(current_level_index);

            }
            else if (panel_grid_pos.x == 1)
            {
                current_level->set_monitor_state(MONITOR_STATE_PLAY_1);
                level_set->touch(current_level_index);

            }
            else if (panel_grid_pos.x == 2)
            {
                current_level->set_monitor_state(MONITOR_STATE_PLAY_ALL);
            }

            else if ((next_dialogue_level > 8) && panel_grid_pos.x == 4 && !current_level_set_is_inspected)
            {
                SaveObjectMap* omap = new SaveObjectMap;
                omap->add_num("level_index", current_level_index);
                omap->add_item("levels", edited_level_set->save_one(current_level_index));
                std::ostringstream stream;
                if (keyboard_shift)
                    omap->pretty_print(stream, 0);
                else
                    omap->save(stream);
                delete omap;
                std::string reply;
                reply = "ComPressure Level ";
                reply += std::to_string(current_level_index + 1);
                reply += ": \"";
                reply += level_set->levels[current_level_index]->name;
                reply += "\" (";
                reply += level_set->levels[current_level_index]->score_set ? std::to_string(pressure_as_percent(level_set->levels[current_level_index]->last_score)) : "Err";
                reply += "%)\n";
                
                if (keyboard_shift)
                {
                    reply += stream.str();
                    reply += "\n";
                }
                else
                {
                    std::string comp;
                    if (keyboard_ctrl)
                        comp = compress_string_zlib(stream.str());
                    else
                        comp = compress_string_zstd(stream.str());
                    std::u32string s32;

                    s32 += 0x1F682;                 // steam engine
                    unsigned spaces = 2;
                    for(char& c : comp)
                    {
                        if (spaces >= 80)
                        {
                            s32 += '\n';
                            spaces = 0;
                        }
                        spaces++;
                        s32 += uint32_t(0x2800 + (unsigned char)(c));

                    } 
                    s32 += 0x1F6D1;                 // stop sign

                    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
                    reply += conv.to_bytes(s32);
                    reply += "\n";
                }

                SDL_SetClipboardText(reply.c_str());
            }
            else if ((next_dialogue_level > 8) && panel_grid_pos.x == 5 && !current_level_set_is_inspected && clipboard_level_set)
            {
                set_level_set(clipboard_level_set);
                deletable_level_set = NULL;
                free_level_set_on_return = true;
                clipboard_level_set = NULL;
                if (last_clip)
                {
                    SDL_free(last_clip);
                    last_clip = NULL;
                }
                set_current_circuit_read_only();
                current_level_set_is_inspected = true;
                set_level(clipboard_level_index);
            }

            panel_grid_pos = (panel_pos - XYPos(32 * 5 + 16, 0)) / 16;
            if ((next_dialogue_level > 8) && !current_level_set_is_inspected)
            {
                if (panel_grid_pos.y == 0)
                {
                    int index = panel_grid_pos.x - 1;
                    if (index >= 0 && index < 4)
                    {
                        if (current_level->saved_designs[index])
                        {
                            std::string level_name = current_level->name;
                            deletable_level_set = &current_level->saved_designs[index];
                            set_level_set(current_level->saved_designs[index]);
                            set_current_circuit_read_only();
                            current_level_set_is_inspected = true;
                            set_level(level_name);
                        }
                    }
                }
                if (panel_grid_pos.y == 1)
                {
                    int index = panel_grid_pos.x - 1;
                    if (index >= 0 && index < 4)
                    {
                        if (edited_level_set->levels[current_level_index]->saved_designs[index])
                        {
                            show_confirm = true;
                            confirm_what = CONFIRM_SAVE_OVERWRITE;
                            confirm_box_pos = XYPos(552, 80);
                            confirm_save_index = index;
                        }
                        else
                        {
                            edited_level_set->save_design(current_level_index, index);
                        }
                    }
                }
            }
        }

        panel_grid_pos = (panel_pos - XYPos(0, 8)) / 16;
        if (panel_grid_pos.y == 2)
        {
            unsigned test_count = current_level->tests.size();
            unsigned t = panel_grid_pos.x - 1;
            if (t < test_count)
            {
                current_level->select_test(t);
                level_set->touch(current_level_index);
            }
        }
        if (editing_level && (panel_grid_pos == XYPos(current_level->tests.size() + 1, 2)) && current_level->tests.size() < 14)
        {
            current_level->tests.insert(current_level->tests.begin() + current_level->test_index, current_level->tests[current_level->test_index]);
            current_level->test_index++;
        }
        if (editing_level && (panel_grid_pos == XYPos(current_level->tests.size() + 1, 3)))
        {
            if (current_level->tests.size() > 1)
            {
                current_level->tests.erase(current_level->tests.begin() + current_level->test_index);

                if (current_level->test_index >= current_level->tests.size())
                {
                    current_level->test_index = current_level->tests.size() - 1;
                    current_level->sim_point_index = 0;
                }
            }
        }
        if (panel_grid_pos == XYPos(0, 3) && current_level->best_design)
        {
            std::string level_name = current_level->name;
            deletable_level_set = &current_level->best_design;
            set_level_set(current_level->best_design);
            set_current_circuit_read_only();
            current_level_set_is_inspected = true;
            set_level(level_name);
        }
        XYPos subtest_pos = panel_pos - XYPos(8 + 16, 32 + 32 + 16);
        if (editing_level && (subtest_pos.y >= 0) && (subtest_pos.y < 16))
        {
            if (panel_pos.x >= 116 && panel_pos.x < 124)
            {
                const int s_steps[] = {200, 500, 1000, 2000, 5000, 7500, 10000, 15000, 20000, 30000, 40000, 50000, 60000, 80000, 100000, 200000, 500000, 1000000};
                int c = current_level->substep_count;
                int s_index = 0;
                if (c > 1000000)
                    c = 1000000;
                while (c > s_steps[s_index])
                    s_index++;
                if (subtest_pos.y < 8)
                {
                    if (c < 1000000)
                        s_index++;
                }
                else
                {
                    if (s_index)
                        s_index--;
                }
                current_level->substep_count = s_steps[s_index];
            }
            if (panel_pos.x >= 132 && panel_pos.x < 148)
            {
                current_level->tests[current_level->test_index].first_simpoint = current_level->sim_point_index;
            }
            if (panel_pos.x >= 156 && panel_pos.x < 172)
            {
                current_level->tests[current_level->test_index].reset = TestResetType((current_level->tests[current_level->test_index].reset + 1) % 3);
            }
            for (int i = 0 ; i < 4; i++)
            {
                int x = panel_pos.x - (i * 16 + 180);
                if (x >= 0 && x < 16)
                {
                    bool s = (current_level->connection_mask >> i) & 1;
                    if (s)
                    {
                        if (popcount(current_level->connection_mask) <= 1)
                            break;
                        
                        int p = 0;
                        while (current_level->pin_order[p] != i)
                            p++;
                        while (p < 3)
                        {
                            current_level->pin_order[p] = current_level->pin_order[p+1];
                            p++;
                        }
                        current_level->pin_order[3] = -1;
                        current_level->connection_mask &= ~(1 << i);
                        for (Test& test: current_level->tests)
                        {
                            if (test.tested_direction == i)
                                test.tested_direction = Direction(current_level->pin_order[0]);
                        }

                    }
                    else
                    {
                        int p = 0;
                        while (current_level->pin_order[p] >= 0)
                            p++;
                        current_level->pin_order[p] = i;
                        current_level->connection_mask |= 1 << i;
                    }
                    set_level_set(level_set);
                }
            }
        }

        if (editing_level && subtest_pos.x < 0)
        {
            int offset = 0;
            for (int i = 0 ; i < 4; i++)
            {
                int pin_index = current_level->pin_order[i];
                if (pin_index < 0)
                    break;

                if (pin_index == current_level->tests[current_level->test_index].tested_direction)
                    continue;
                offset += 16;
                
                if ((subtest_pos.y >= offset) && (subtest_pos.y < (offset + 16)))
                {
                    current_level->tests[current_level->test_index].tested_direction = Direction(pin_index);
                    break;
                }
            }
        }
        else if (subtest_pos.y >= 16 && subtest_pos.y < (popcount(current_level->connection_mask) * 16 + 32))
        {
            int sim_point_count = current_level->tests[current_level->test_index].sim_points.size();
            int sim_point_index = current_level->sim_point_index;
            int sim_point_offset = std::max(std::min(sim_point_count - 12, sim_point_index - 6), 0);
            
            skip_to_subtest_index = subtest_pos.x / 16 + sim_point_offset;
            if (skip_to_subtest_index < current_level->tests[current_level->test_index].sim_points.size())
            {
                skip_to_next_subtest = true;
                current_level->set_monitor_state(MONITOR_STATE_PLAY_1);
            }
            else if (editing_level && (skip_to_subtest_index == current_level->tests[current_level->test_index].sim_points.size()))
            {
                if (subtest_pos.y >= 16 && subtest_pos.y < 32)
                {
                    std::vector<SimPoint> &sim_points = current_level->tests[current_level->test_index].sim_points;
                    sim_points.insert(sim_points.begin() + current_level->sim_point_index, sim_points[current_level->sim_point_index]);
                    current_level->sim_point_index++;
                }
                else if (subtest_pos.y >= 32 && subtest_pos.y < 48)
                {
                    if (current_level->tests[current_level->test_index].sim_points.size() > 1)
                    {
                        std::vector<SimPoint> &sim_points = current_level->tests[current_level->test_index].sim_points;
                        sim_points.erase(sim_points.begin() + current_level->sim_point_index);
                        
                        if (current_level->sim_point_index >= current_level->tests[current_level->test_index].sim_points.size())
                            current_level->sim_point_index = current_level->tests[current_level->test_index].sim_points.size() - 1;
                        if (current_level->tests[current_level->test_index].first_simpoint >= current_level->tests[current_level->test_index].sim_points.size())
                            current_level->tests[current_level->test_index].first_simpoint = current_level->tests[current_level->test_index].sim_points.size() - 1;
                        current_level->current_simpoint = current_level->tests[current_level->test_index].sim_points[current_level->sim_point_index];

                    }
                
                }

            
            }
            
        }
        
    } else if (panel_state == PANEL_STATE_TEST)
    {
        int port_index = panel_pos.x / 48;
        if (panel_pos.y <= (101 + 16))
        {
            if (port_index > 4)
                return;
            current_level->set_monitor_state(MONITOR_STATE_PAUSE);
            level_set->touch(current_level_index);

            watch_slider(panel_offset.y + (101 + 8) * scale, DIRECTION_N, 100, &current_level->current_simpoint.values[port_index]);
            return;
        }
        else if (panel_pos.y <= (101 + 16 + 7 + 16))
        {
            if (port_index > 4)
                return;
            current_level->set_monitor_state(MONITOR_STATE_PAUSE);
            level_set->touch(current_level_index);

            watch_slider(panel_offset.x + (port_index * 48 + 8) * scale, DIRECTION_E, 33, &current_level->current_simpoint.force[port_index], 100);
            return;
        }
        else if ((panel_pos - XYPos(256-32-16, 32 + 32 + 8 + 112)).inside(XYPos(48, 120)))
        {
            watch_slider(panel_offset.y + (32 + 32 + 16 + 112) * scale, DIRECTION_S, 100, &current_level->test_pressure_histroy_speed, 100);
        }
        return;
    } else if (panel_state == PANEL_STATE_SCORES)
    {
        if ((panel_pos - XYPos(8 * 32, 0)).inside(XYPos(16, 11 * 16)))
        {
            click_scroll_bar(friend_score_scroll, panel_pos - XYPos(8*32, 0));
        }

        XYPos table_pos = mouse / scale - XYPos((8 + 32 * 11), (8 + 8 + 32));
        if (table_pos.inside(XYPos(16,11*16)))
        {
            int i = table_pos.y / 16;
            if (show_server_levels)
            {
                if ((friend_score_scroll.offset_rows + i) < server_levels.size())
                {
                    server_level_fetch(server_levels[friend_score_scroll.offset_rows + i].name);
                }
            }
            else
            {
                Level* level;
                if (current_level_index < LEVEL_COUNT)
                    level = edited_level_set->levels[current_level_index];
                else
                {
                    int i = edited_level_set->find_custom_by_name(current_level->name);
                    if (i < 0)
                        level = current_level;
                    else
                        level = edited_level_set->levels[i];
                }
                if ((friend_score_scroll.offset_rows + i) < level->friend_scores.size())
                {
                    Level::FriendScore& score = level->friend_scores[friend_score_scroll.offset_rows + i];
                    if (score.visible)
                    {
                        if (current_level_index <  LEVEL_COUNT)
                            design_fetch(score.steam_id, current_level_index);
                        else
                            design_fetch(score.steam_id, level->name);
                    }
                }
            }
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
        return;
    }

    if (mouse_state == MOUSE_STATE_DELETING)
    {
        if (panel_state == PANEL_STATE_LEVEL_SELECT && editing_level)
        {
            XYPos pixel_pos = ((mouse - panel_offset) / scale) - XYPos(8, 32 + 8);
            if (pixel_pos.inside(XYPos(24*8, 24*8)))
            {
                pixel_pos /= 8;
                if (editing_icon_index == -1)
                {
                    bool changed = false;
                    for (int i = 0; i < 8; i++)
                    {
                        XYPos npos = icon_rotate ? DirFlip(i).trans(pixel_pos, 24) : pixel_pos;
                        if (level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24] != 8)
                        {
                            changed = true;
                            level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24] = 8;
                        }
                    }
                    if (changed)
                        set_level_set(level_set);
                }
                else
                {
                    if (level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] != 8)
                    {
                        level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] = 8;
                        set_level_set(level_set);
                    }
                }
                return;
            }
        }

        XYPos pos = ((mouse - grid_offset) / scale);
        for (auto it = current_circuit->signs.begin(); it != current_circuit->signs.end(); it++)
        {
            Sign& sign = *it;
            if ((pos - sign.get_pos()).inside(sign.get_size()))
            {
                current_circuit->remove_sign(it, !first_deletion);
                if ((next_dialogue_level > 1) && (panel_state == PANEL_STATE_LEVEL_SELECT))
                    panel_state = PANEL_STATE_EDITOR;
                first_deletion = false;
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

        if ((next_dialogue_level > 1) && (panel_state == PANEL_STATE_LEVEL_SELECT))
            panel_state = PANEL_STATE_EDITOR;

        current_circuit->set_element_empty(grid, !first_deletion);
        first_deletion = false;
        level_set->touch(current_level_index);
        return;
    }

    if (mouse_state == MOUSE_STATE_SCROLL_BAR_DRAG)
    {
        int pos = mouse.y / scale - dragged_scroll_bar->pos.y - scroll_drag_y - 16;
        pos = (pos * dragged_scroll_bar->total_rows + (dragged_scroll_bar->height - 32) / 2) / (dragged_scroll_bar->height - 32);
        dragged_scroll_bar->offset_rows = pos;
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
        if (slider_value_max)
            vol = (vol * slider_value_max) / slider_max;
        *slider_value_tgt = vol;

        if (editing_level)
        {
            current_level->tests[current_level->test_index].sim_points[current_level->sim_point_index] = current_level->current_simpoint;
        }
        return;
    }
    
    
    if (panel_state == PANEL_STATE_LEVEL_SELECT && editing_level && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
    {
        XYPos pixel_pos = ((mouse - panel_offset) / scale) - XYPos(8, 32 + 8);
        if (pixel_pos.inside(XYPos(24*8, 24*8)))
        {
            pixel_pos /= 8;
            if (editing_icon_index == -1)
            {
                bool changed = false;
                for (int i = 0; i < 8; i++)
                {
                    XYPos npos = icon_rotate ? DirFlip(i).trans(pixel_pos, 24) : pixel_pos;
                    if (pixel_colour != level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24])
                    {
                        changed = true;
                        level_set->levels[current_level_index]->icon_pixels[npos.y][npos.x + i * 24] = pixel_colour;
                    }
                }
                if (changed)
                    set_level_set(level_set);
            }
            else
            {
                if (level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] != pixel_colour)
                {
                    level_set->levels[current_level_index]->icon_pixels[pixel_pos.y][pixel_pos.x + editing_icon_index * 24] = pixel_colour;
                    set_level_set(level_set);
                }
            }
            return;
        }
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
                        display_about = false;
                        mouse_state = MOUSE_STATE_NONE;
                        break;
                    case SDL_SCANCODE_1:
                        if (!SDL_IsTextInputActive() && next_dialogue_level > 4)
                        {
                            current_level->set_monitor_state(MONITOR_STATE_PAUSE);
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_2:
                        if (!SDL_IsTextInputActive() && next_dialogue_level > 4)
                        {
                            current_level->set_monitor_state(MONITOR_STATE_PLAY_1);
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_3:
                        if (!SDL_IsTextInputActive() && next_dialogue_level > 4)
                            current_level->set_monitor_state(MONITOR_STATE_PLAY_ALL);
                        break;
                    case SDL_SCANCODE_TAB:
                    {
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                                mouse_state = MOUSE_STATE_PLACING_SOURCE;
                            else
                                mouse_state = MOUSE_STATE_PLACING_VALVE;
                        }
                        break;
                    }
                    case SDL_SCANCODE_Q:
                        if (next_dialogue_level <= 1)
                            break;
                        if (!SDL_IsTextInputActive())
                            dir_flip = dir_flip.rotate(false);
                        if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                            clipboard.rotate(false);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(false);
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            current_circuit->rotate_selected_elements(selected_elements, false);
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_E:
                        if (next_dialogue_level <= 1)
                            break;
                        if (!SDL_IsTextInputActive())
                            dir_flip = dir_flip.rotate(true);
                        if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                            clipboard.rotate(true);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(true);
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            current_circuit->rotate_selected_elements(selected_elements, true);
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_W:
                        if (next_dialogue_level <= 3)
                            break;
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (selected_elements.empty())
                            {
                                dir_flip = dir_flip.flip(true);
                                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                                    clipboard.flip(true);
                                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                                    dragged_sign.flip(true);
                            }
                            else
                            {
                                if (!keyboard_shift)
                                    current_circuit->move_selected_elements(selected_elements, DIRECTION_N);
                                else
                                    current_circuit->flip_selected_elements(selected_elements, true);

                                level_set->touch(current_level_index);
                            }
                        }
                        break;
                    case SDL_SCANCODE_A:
                        if (next_dialogue_level <= 3)
                            break;
                        if (!SDL_IsTextInputActive() && keyboard_ctrl)
                        {
                            XYPos pos;
                            for (pos.y = 0; pos.y < 9; pos.y++)
                            for (pos.x = 0; pos.x < 9; pos.x++)
                            {
                                if (!current_circuit->elements[pos.y][pos.x]->is_empty())
                                    selected_elements.insert(pos);
                            }
                            break;
                        }
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (selected_elements.empty())
                            {
                                dir_flip = dir_flip.flip(false);
                                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                                    clipboard.flip(false);
                                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                                    dragged_sign.flip(false);
                            }
                            else
                            {
                                if (!keyboard_shift)
                                    current_circuit->move_selected_elements(selected_elements, DIRECTION_W);
                                else
                                    current_circuit->flip_selected_elements(selected_elements, false);
                                level_set->touch(current_level_index);
                            }
                        }
                        break;
                    case SDL_SCANCODE_S:
                        if (next_dialogue_level <= 3)
                            break;
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (selected_elements.empty())
                            {
                                dir_flip = dir_flip.flip(true);
                                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                                    clipboard.flip(true);
                                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                                    dragged_sign.flip(true);
                            }
                            else
                            {
                                if (!keyboard_shift)
                                    current_circuit->move_selected_elements(selected_elements, DIRECTION_S);
                                else
                                    current_circuit->flip_selected_elements(selected_elements, true);
                                level_set->touch(current_level_index);
                            }
                        }
                        break;
                    case SDL_SCANCODE_D:
                        if (next_dialogue_level <= 3)
                            break;
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (selected_elements.empty())
                            {
                                dir_flip = dir_flip.flip(false);
                                if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                                    clipboard.flip(false);
                                if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                                    dragged_sign.flip(false);
                            }
                            else
                            {
                                if (!keyboard_shift)
                                    current_circuit->move_selected_elements(selected_elements, DIRECTION_E);
                                else
                                    current_circuit->flip_selected_elements(selected_elements, false);
                                level_set->touch(current_level_index);
                            }
                        }
                        break;
                    case SDL_SCANCODE_X:
                        if (!SDL_IsTextInputActive())
                        {
                            clipboard.copy(selected_elements, *current_circuit);
                            if(!current_circuit_is_read_only)
                            {
                                current_circuit->delete_selected_elements(selected_elements);
                                selected_elements.clear();
                                level_set->touch(current_level_index);
                            }
                        }
                        break;
                    case SDL_SCANCODE_C:
                        if (!SDL_IsTextInputActive())
                            clipboard.copy(selected_elements, *current_circuit);
                        if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                            mouse_state = MOUSE_STATE_NONE;
                        break;
                    case SDL_SCANCODE_V:
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (!clipboard.elements.empty())
                                mouse_state = MOUSE_STATE_PASTING_CLIPBOARD;
                            else
                                mouse_state = MOUSE_STATE_NONE;
                            selected_elements.clear();
                        }
                        break;
                    case SDL_SCANCODE_DELETE:
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            current_circuit->delete_selected_elements(selected_elements);
                            selected_elements.clear();
                            level_set->touch(current_level_index);
                        }
                        else if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            if (text_entry_offset < text_entry_string->size())
                            {
                                text_entry_string->erase(text_entry_offset, 1);
                                while (true)
                                {
                                    if (text_entry_offset >= text_entry_string->size())
                                        break;
                                    if (is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                        break;
                                    text_entry_string->erase(text_entry_offset, 1);
                                }
                            }
                        }
                        break;
                    case SDL_SCANCODE_BACKSPACE:
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            current_circuit->delete_selected_elements(selected_elements);
                            selected_elements.clear();
                            level_set->touch(current_level_index);
                        }
                        else if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            if (text_entry_offset)
                            {
                                while (true)
                                {
                                    text_entry_offset--;
                                    if (!text_entry_offset)
                                        break;
                                    if (is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                        break;
                                    text_entry_string->erase(text_entry_offset, 1);
                                }
                                text_entry_string->erase(text_entry_offset, 1);
                            }
                        }
                        break;
                    case SDL_SCANCODE_RETURN:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT && text_entry_target != TEXT_TARGET_LEVEL_NAME)
                        {
                            std::string new_text("\n");
                            text_entry_string->insert(text_entry_offset, new_text);
                            text_entry_offset += new_text.size();
                        }
                        break;
                    case SDL_SCANCODE_LEFT:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            if (text_entry_offset)
                            {
                                while (true)
                                {
                                    text_entry_offset--;
                                    if (!text_entry_offset)
                                        break;
                                    if (is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                        break;
                                }
                            }
                            
                        }
                        break;
                    case SDL_SCANCODE_RIGHT:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            if (text_entry_offset < text_entry_string->size())
                            {
                                while (true)
                                {
                                    text_entry_offset++;
                                    if (text_entry_offset >= text_entry_string->size())
                                        break;
                                    if (is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                        break;
                                }
                            }

                        }
                        break;
                    case SDL_SCANCODE_UP:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            int char_count = 0;
                            while (text_entry_offset)
                            {
                                char_count++;
                                text_entry_offset--;
                                while (text_entry_offset && !is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                    text_entry_offset--;
                                if ((*text_entry_string)[text_entry_offset] == '\n')
                                    break;
                            }
                            if (!text_entry_offset)
                                break;
                            text_entry_offset--;
                            while (text_entry_offset && (*text_entry_string)[text_entry_offset] != '\n')
                                text_entry_offset--;
                            if ((*text_entry_string)[text_entry_offset] == '\n')
                                text_entry_offset++;
                            while (--char_count > 0)
                            {
                                if ((*text_entry_string)[text_entry_offset] == '\n')
                                    break;
                                text_entry_offset++;
                                while (!is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                    text_entry_offset++;
                            }
                        }
                        break;
                    case SDL_SCANCODE_DOWN:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            int char_count = 0;
                            while (text_entry_offset)
                            {
                                char_count++;
                                text_entry_offset--;
                                if ((*text_entry_string)[text_entry_offset] == '\n')
                                    break;
                                while (text_entry_offset && !is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                    text_entry_offset--;
                            }
                            if (text_entry_offset)
                                text_entry_offset++;
                            else
                                char_count++;

                            while ((*text_entry_string)[text_entry_offset] != '\n' && text_entry_offset < text_entry_string->size())
                                text_entry_offset++;
                            if (text_entry_offset < text_entry_string->size())
                                text_entry_offset++;
                            while (--char_count > 0)
                            {
                                if (text_entry_offset >= text_entry_string->size())
                                    break;
                                if ((*text_entry_string)[text_entry_offset] == '\n')
                                    break;
                                text_entry_offset++;
                                while (!is_leading_utf8_byte((*text_entry_string)[text_entry_offset]))
                                    text_entry_offset++;
                            }
                        }
                        break;
                    case SDL_SCANCODE_HOME:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            while (text_entry_offset)
                            {
                                text_entry_offset--;
                                if ((*text_entry_string)[text_entry_offset] == '\n')
                                {
                                    text_entry_offset++;
                                    break;
                                }
                            }
                        }
                        break;
                    case SDL_SCANCODE_END:
                        if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                        {
                            while (text_entry_offset < text_entry_string->size() && (*text_entry_string)[text_entry_offset] != '\n')
                                text_entry_offset++;
                        }
                        break;
                    case SDL_SCANCODE_Z:
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            if (!keyboard_shift)
                                current_circuit->undo(level_set);
                            else
                                current_circuit->redo(level_set);
                            selected_elements.clear();
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_Y:
                        if (!SDL_IsTextInputActive() && !current_circuit_is_read_only)
                        {
                            current_circuit->redo(level_set);
                            selected_elements.clear();
                            level_set->touch(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_PAGEDOWN:
                        if (show_dialogue || show_dialogue_hint)
                        {
                            dialogue_index++;
                        }
                        else if (!SDL_IsTextInputActive())
                        {
                            while (true)
                            {
                                current_level_index++;
                                if (current_level_index >= 1000)
                                    current_level_index = 0;
                                if (level_set->is_playable(current_level_index, highest_level))
                                    break;
                            }
                            set_level(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_PAGEUP:
                        if (show_dialogue || show_dialogue_hint)
                        {
                            if (dialogue_index)
                                dialogue_index--;
                        }
                        else if (!SDL_IsTextInputActive())
                        {
                            while (true)
                            {
                                if (!current_level_index)
                                    current_level_index = 1000;
                                current_level_index--;
                                if (level_set->is_playable(current_level_index, highest_level))
                                    break;
                            }
                            set_level(current_level_index);
                        }
                        break;
                    case SDL_SCANCODE_F1:
                        show_help = !show_help;
                        break;
                    case SDL_SCANCODE_F5:
                        show_debug = !show_debug;
                        break;
                    case SDL_SCANCODE_F7:
                    {
                        SaveObjectMap* omap = new SaveObjectMap;
                        omap->add_num("level_index", current_level_index);
                        omap->add_item("levels", edited_level_set->save_one(current_level_index));
                        std::ostringstream stream;
                        omap->save(stream);
                        delete omap;
                        std::string reply = stream.str();
                        SDL_SetClipboardText(reply.c_str());
                        break;
                    }
                    case SDL_SCANCODE_F8:
                    {
                        SaveObject* sav = current_circuit->save();
                        sav->save(std::cout);
                        std::cout << "\n";
                        
                        delete sav;
                        break;
                    }
                   case SDL_SCANCODE_F11:
                        full_screen = !full_screen;
                        SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        SDL_SetWindowBordered(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                        SDL_SetWindowResizable(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                        SDL_SetWindowInputFocus(sdl_window);
                        break;
                    case SDL_SCANCODE_F12:
                        if (keyboard_ctrl)
                        {
                            if (keyboard_shift)
                                global_design_submit(current_level_index);
                            else
                                current_level->global = !current_level->global;
                        }
                        break;
                    case SDL_SCANCODE_LSHIFT:
                        keyboard_shift |= 1;
                        break;
                    case SDL_SCANCODE_RSHIFT:
                        keyboard_shift |= 2;
                        break;
                    case SDL_SCANCODE_LCTRL:
                        keyboard_ctrl |= 1;
                        break;
                    case SDL_SCANCODE_RCTRL:
                        keyboard_ctrl |= 2;
                        break;
                    case SDL_SCANCODE_SPACE:
                        if (next_dialogue_level > 4)
                        {
                            if (!SDL_IsTextInputActive() && current_level->monitor_state != MONITOR_STATE_PAUSE)
                            {
                                skip_to_next_subtest = true;
                                skip_to_subtest_index = -1;
                            }
                        }
                        break;
                    default:
//                        printf("Uncaught key: %d\n", e.key.keysym.scancode);
                        break;
                }
                break;
            }
            case SDL_KEYUP:
                switch (e.key.keysym.scancode)
                {
                    case SDL_SCANCODE_LSHIFT:
                        keyboard_shift &= ~1;
                        break;
                    case SDL_SCANCODE_RSHIFT:
                        keyboard_shift &= ~2;
                        break;
                    case SDL_SCANCODE_LCTRL:
                        keyboard_ctrl &= ~1;
                        break;
                    case SDL_SCANCODE_RCTRL:
                        keyboard_ctrl &= ~2;
                        break;
                }
                break;
            case SDL_TEXTINPUT:
            {
                if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                {
                    std::string new_text(e.text.text);
                    text_entry_string->insert(text_entry_offset, new_text);
                    text_entry_offset += new_text.size();
                }
                break;
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
                    if (mouse_state == MOUSE_STATE_SCROLL_BAR_DRAG)
                        mouse_state = MOUSE_STATE_NONE;
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
                        level_set->touch(current_level_index);
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
                            if (pos.x >= tl.x && pos.x <= br.x && pos.y >= tl.y && pos.y <= br.y && !current_circuit->elements[pos.y][pos.x]->is_empty())
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
                            mouse_state = MOUSE_STATE_ENTERING_TEXT;
                            text_entry_target = TEXT_TARGET_SIGN;
                            text_entry_string = &current_circuit->signs.front().text;
                            text_entry_offset = current_circuit->signs.front().text.size();

                            SDL_StartTextInput();
                            {
                                SDL_Rect dst_rect = {0, 0, 600, 600};
                                SDL_SetTextInputRect(&dst_rect);
                            }
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
                if (mouse_state == MOUSE_STATE_ENTERING_TEXT)
                {
                    mouse_state = MOUSE_STATE_NONE;
                    break;
                }
                mouse.x = e.button.x;
                mouse.y = e.button.y;
                mouse -= screen_offset;
                if (e.button.button == SDL_BUTTON_LEFT)
                {
                    if (display_language_dialogue)
                    {
                        int i = 0;
                        int col = 0;
                        for (std::map<std::string, SaveObject*>::iterator it = languages->omap.begin(); it != languages->omap.end(); it++)
                        {
                            if ((mouse / scale - XYPos((160 + 32 + col * 160), (90 + 16 + i * 24))).inside(XYPos(160 - 64, 24)))
                            {
                                language_name = it->first;
                                load_lang();
                                current_language = languages->get_item(language_name)->get_map();
                                break;
                            }
                            i++;
                            if (i >= 7)
                            {
                                col++;
                                i = 0;
                            }
                        }
                        display_language_dialogue = false;
                    }
                    else if (show_main_menu)
                    {
                        if (display_about)
                        {
                            display_about = false;
                            break;
                        }
                        
                        XYPos pos = (mouse / scale) - XYPos((160 + 32), (90 + 32));
                        if (pos.inside(XYPos(32, 32)))
                            return true;
                        if ((pos - XYPos(0, 48)).inside(XYPos(32, 32)))
                        {
                            discord_joined = true;
                            DisplayWebsite("https://discord.gg/7ZVZgA7gkS");
                        }
                        if ((pos - XYPos(0, 96)).inside(XYPos(32, 32)))
                        {
                            display_language_dialogue = true;
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 32)))
                        {
                            full_screen = !full_screen;
                            SDL_SetWindowFullscreen(sdl_window, full_screen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                            SDL_SetWindowBordered(sdl_window, full_screen ? SDL_FALSE : SDL_TRUE);
                            SDL_SetWindowInputFocus(sdl_window);
                        }
                        if ((pos - XYPos(0, 48)).inside(XYPos(32, 32)))
                        {
                            display_about = true;
                        }
                        if ((pos - XYPos(0, 96)).inside(XYPos(32, 32)) && highest_level >= 20)
                        {
                            number_high_precision = !number_high_precision;
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 128)))
                        {
                            watch_slider((90 + 32 + 100 + 8) * scale, DIRECTION_N, 100, &sound_volume);
                        }
                        pos.x -= 64;
                        if (pos.inside(XYPos(32, 128)))
                        {
                            watch_slider((90 + 32 + 100 + 8) * scale, DIRECTION_N, 100, &music_volume);
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
                            if (x < max_help_page(next_dialogue_level))
                                show_help_page = x;
                            break;
                        }
                        show_help_page = (show_help_page + 1) % max_help_page(next_dialogue_level);
                        break;
                    }
                    else if (show_confirm)
                    {
                        if (((mouse / scale) - confirm_box_pos).inside(XYPos(32,32)))
                        {
                            if (confirm_what == CONFIRM_DELETE)
                            {
                                current_level_set_is_inspected = false;
                                if (deletable_level_set)
                                {
                                    delete *deletable_level_set;
                                    *deletable_level_set = NULL;
                                }
                                else if (free_level_set_on_return)
                                {
                                    delete level_set;
                                    free_level_set_on_return = false;
                                }
                                set_level_set(edited_level_set);
                                set_level(current_level_index);
                            }
                            else if (confirm_what == CONFIRM_SAVE_OVERWRITE)
                            {
                                edited_level_set->save_design(current_level_index, confirm_save_index);
                            }
                            else if (confirm_what == CONFIRM_DELETE_LEVEL)
                            {
                                edited_level_set->delete_level(current_level_index);
                                editing_level = false;
                                set_level(current_level_index - 1);
                            }
                        }
                        show_confirm = false;
                        
                    }
                    else if (show_dialogue_discord_prompt)
                    {
                        show_dialogue_discord_prompt = false;
                        break;
                    }
                    else if (scoring_mode_dialogue)
                    {
                        bool swi = false;
                        if (((mouse - panel_offset) / scale - XYPos(5*32, 144)).inside(XYPos(32, 32)))
                        {
                            test_mode = TEST_MODE_ACCURACY;
                            edited_level_set = level_set_accuracy;
                            swi = true;
                        }
                        if (((mouse - panel_offset) / scale - XYPos(6*32, 144)).inside(XYPos(32, 32)))
                        {
                            test_mode = TEST_MODE_PRICE;
                            edited_level_set = level_set_price;
                            swi = true;
                        }
                        if (((mouse - panel_offset) / scale - XYPos(7*32, 144)).inside(XYPos(32, 32)))
                        {
                            test_mode = TEST_MODE_STEAM;
                            edited_level_set = level_set_steam;
                            swi = true;
                        }
                        if (swi && !current_level_set_is_inspected)
                        {
                            set_level_set(edited_level_set);
                            set_level(current_level_index);
                            swi = true;
                        }
                        scoring_mode_dialogue = false;
                        break;
                    }
                    else if (show_dialogue || show_dialogue_hint)
                    {
                        dialogue_index++;
                        break;
                    }
                    else if (mouse.x < panel_offset.x)
                    {
                        mouse_click_in_grid(e.button.clicks);
                    }
                    else
                    {
                        mouse_click_in_panel(e.button.clicks);
                    }
                }
                else if (e.button.button == SDL_BUTTON_RIGHT)
                {
                    if (mouse_state == MOUSE_STATE_ANIMATING)
                        break;
                    if (show_dialogue_discord_prompt)
                    {
                        show_dialogue_discord_prompt = false;
                        break;
                    }
                    if (show_dialogue)
                    {
                        show_dialogue = false;
                        break;
                    }
                    if (show_dialogue_hint)
                    {
                        show_dialogue_hint = false;
                        break;
                    }
                    if (!current_circuit_is_read_only)
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
                    else
                        mouse_state = MOUSE_STATE_NONE;
                }
                else if (e.button.button == SDL_BUTTON_MIDDLE)
                {
                    if (!current_circuit_is_read_only)
                    {
                        if (mouse_state == MOUSE_STATE_PLACING_VALVE)
                            mouse_state = MOUSE_STATE_PLACING_SOURCE;
                        else
                            mouse_state = MOUSE_STATE_PLACING_VALVE;
                    }
                }
                break;
            }
            case SDL_MOUSEWHEEL:
            {
                if (show_dialogue || show_dialogue_hint)
                {
                    if(e.wheel.y > 0)
                    {
                        if (dialogue_index)
                            dialogue_index--;
                    }
                    else
                    {
                        dialogue_index++;
                    }
                }
                else if (mouse.x < panel_offset.x)
                {
                    if(e.wheel.y > 0)
                    {
                        dir_flip = dir_flip.rotate(true);
                        if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                            clipboard.rotate(false);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(false);
                    }

                    if(e.wheel.y < 0)
                    {
                        dir_flip = dir_flip.rotate(false);
                        if (mouse_state == MOUSE_STATE_PASTING_CLIPBOARD)
                            clipboard.rotate(true);
                        if (mouse_state == MOUSE_STATE_DRAGGING_SIGN)
                            dragged_sign.rotate(true);
                    }
                }
                else
                {
                    ScrollBar* selected_scroll = NULL;
                    if (panel_state == PANEL_STATE_LEVEL_SELECT && !editing_level)
                        selected_scroll = &level_select_scroll;
                    if (panel_state == PANEL_STATE_EDITOR)
                        selected_scroll = &editor_scroll;
                    if (panel_state == PANEL_STATE_SCORES)
                        selected_scroll = &friend_score_scroll;

                    if (selected_scroll)
                    {
                        if(e.wheel.y > 0)
                            selected_scroll->offset_rows--;

                        if(e.wheel.y < 0)
                            selected_scroll->offset_rows++;
                        normalize_scroll_bar(*selected_scroll);
                    }
                }

                break;
            }
            case SDL_RENDER_TARGETS_RESET:
            {
                set_level_set(level_set);
                break;
            }
            default:
            {
//                printf("event:0x%x\n", e.type);
            
            
            }
        }
    }
    if (SDL_IsTextInputActive() && mouse_state != MOUSE_STATE_ENTERING_TEXT)
        SDL_StopTextInput();

    return false;
}

void GameState::watch_slider(unsigned slider_pos_, Direction slider_direction_, unsigned slider_max_, unsigned* slider_value_tgt_, unsigned slider_value_max_)
{
    mouse_state = MOUSE_STATE_SPEED_SLIDER;
    slider_pos = slider_pos_;
    slider_direction = slider_direction_;
    slider_max = slider_max_;
    slider_value_tgt = slider_value_tgt_;
    slider_value_max = slider_value_max_;
    mouse_motion();
}

void GameState::set_current_circuit_read_only()
{
    selected_elements.clear();
    current_circuit_is_read_only = true;
    if (panel_state == PANEL_STATE_EDITOR)
        panel_state = PANEL_STATE_MONITOR;
    mouse_state = MOUSE_STATE_NONE;
}

void GameState::check_clipboard()
{
    char* new_clip = SDL_GetClipboardText();
    if (!new_clip)
        return;
    if (last_clip && (strcmp(new_clip, last_clip) == 0))
    {
        SDL_free(new_clip);
        return;
    }
    if (last_clip)
        SDL_free(last_clip);
    last_clip = new_clip;
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
    std::u32string s32 = conv.from_bytes(std::string(new_clip));
    std::string comp;
    for(uint32_t c : s32)
    {
        if ((c & 0xFF00) == 0x2800)
        {
            char asc = c & 0xFF;
            comp += asc;
        }
    }
    
    delete clipboard_level_set;
    clipboard_level_set = NULL;
 
    SaveObjectMap* omap = NULL;
    try 
    {
        if (!comp.empty())
        {
            std::string decomp = decompress_string(comp);
            std::istringstream decomp_stream(decomp);
            omap = SaveObject::load(decomp_stream)->get_map();
        }
        else
        {
            std::string decomp(new_clip);
            while (!decomp.empty() && decomp[0] != '{')
                decomp.erase(0, 1);
            if (decomp == "")
                return;
            std::istringstream decomp_stream(decomp);
            omap = SaveObject::load(decomp_stream)->get_map();
        }
        clipboard_level_index = omap->get_num("level_index");
        LevelSet* new_set = new LevelSet(omap->get_item("levels"), true);
        clipboard_level_set = new_set;
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << "\n";
    }
    delete omap;
}

void GameState::deal_with_scores()
{
    if (scores_from_server.done && !scores_from_server.error)
    {
        try 
        {
            Level* level;
            SaveObjectMap* omap = scores_from_server.resp->get_map();
            if (omap->has_key("level"))
                level = edited_level_set->levels[omap->get_num("level")];
            else
            {
                std::string name = omap->get_string("name");
                int level_i = LEVEL_COUNT;
                while (true)
                {
                    if (level_i >= edited_level_set->levels.size())
                    {
                        if (name == current_level->name)
                        {
                            level = current_level;
                            break;
                        }
                        else
                            throw(std::runtime_error("level name not found"));
                    }
                    if (edited_level_set->levels[level_i]->name == name)
                    {
                        level = edited_level_set->levels[level_i];
                        break;
                    }
                    level_i++;
                }
            }
            
            
            level->global_fetched_score = omap->get_num("score");
            SaveObjectList* glist = omap->get_item("graph")->get_list();
            for (unsigned i = 0; i < 200; i++)
            {
                level->global_score_graph[i] = glist->get_num(i);
            }
            level->global_score_graph_set = true;
            level->friend_scores.clear();
            
            glist = omap->get_item("friend_scores")->get_list();
            for (unsigned i = 0; i < glist->get_count(); i++)
            {
                SaveObjectMap* fmap = glist->get_item(i)->get_map();
                level->friend_scores.push_back(Level::FriendScore{fmap->get_string("steam_username"), uint64_t(fmap->get_num("steam_id")), Pressure(fmap->get_num("score")), bool(Pressure(fmap->get_num("visible")))});
            }
            
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
}

void GameState::deal_with_server_levels_from_server()
{
    if (server_levels_from_server.done && !server_levels_from_server.error)
    {
        try 
        {
            server_levels.clear();
            SaveObjectList* glist = server_levels_from_server.resp->get_list();
            for (unsigned i = 0; i < glist->get_count(); i++)
            {
                SaveObjectMap* fmap = glist->get_item(i)->get_map();
                std::string name = fmap->get_string("name");
                server_levels.push_back(ServerLevel());
                server_levels.back().name = name;
            }
            
        }
        catch (const std::runtime_error& error)
        {
            std::cerr << error.what() << "\n";
        }
        if (server_levels_from_server.resp)
            delete server_levels_from_server.resp;
        server_levels_from_server.resp = NULL;
        server_levels_from_server.done = false;
    }
}

void GameState::deal_with_design_fetch()
{
    if (design_from_server.done && !design_from_server.error)
    {
        try 
        {
            SaveObjectMap* omap = design_from_server.resp->get_map();
            
            if (free_level_set_on_return)
                delete level_set;
            deletable_level_set = NULL;
            set_level_set(new LevelSet(omap->get_item("levels"), true));
            free_level_set_on_return = true;

            set_current_circuit_read_only();
            current_level_set_is_inspected = true;
            if (omap->has_key("level_name"))
                set_level(omap->get_string("level_name"));
            else
                set_level(omap->get_num("level_index"));
        }
        catch (const std::runtime_error& error)
        {
            std::cerr << error.what() << "\n";
        }
        delete design_from_server.resp;
        design_from_server.resp = NULL;
        design_from_server.done = false;
    }
}

void GameState::set_level_set(LevelSet* new_level_set)
{
    level_set = new_level_set;
    for (int i = LEVEL_COUNT; i < level_set->levels.size(); i++)
    {
        SDL_Texture* my_canvas = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 192, 24);
        SDL_SetRenderTarget(sdl_renderer, my_canvas);

        SDL_SetTextureBlendMode(my_canvas, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(sdl_renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(sdl_renderer);
        SDL_SetTextureBlendMode(my_canvas, SDL_BLENDMODE_BLEND);
        
//         {
//             SDL_Rect src_rect = {192, 800 + 4 * 24, 192, 24};
//             SDL_Rect dst_rect = {0, 0, 192, 24};
//             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//         }
//         for (int a = 0; a < 4; a++)
//         {
//             if ((level_set->levels[i]->connection_mask >> a) & 1)
//             {
//                 SDL_Rect src_rect = {192, 800 + a * 24, 192, 24};
//                 SDL_Rect dst_rect = {0, 0, 192, 24};
//                 SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//             }
//         }
//         {
//             SDL_Rect src_rect = {192, 800 + 5 * 24, 192, 24};
//             SDL_Rect dst_rect = {0, 0, 192, 24};
//             SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//         }

        for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24*8; x++)
        {
            for (int r = 0; r < 8; r++)
            {
                int colour = level_set->levels[i]->icon_pixels[y][x];
                if (colour != 8)
                {
                    SDL_Rect src_rect = {503, 80 + colour, 1, 1};
                    SDL_Rect dst_rect = {x, y, 1, 1};
                    SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
                }
            }
        }
//         for (int y = 0; y < 24; y++)
//         for (int x = 0; x < 24; x++)
//         {
//             int colour = level_set->levels[i]->icon_pixels_fg[y][x];
//             SDL_Rect src_rect = {503, 80 + colour, 1, 1};
//             for (int r = 0; r < 8; r++)
//             {
//                 SDL_Rect dst_rect = {x + r * 24, y, 1, 1};
//                 SDL_RenderCopy(sdl_renderer, sdl_texture, &src_rect, &dst_rect);
//             }
//         }
        SDL_SetRenderTarget(sdl_renderer, NULL);
        level_set->levels[i]->setimage_fg_texture(new GameStateWrappedTexture(my_canvas));
    }
}

GameStateWrappedTexture::~GameStateWrappedTexture()
{
    SDL_DestroyTexture(sdl_texture);
}

SDL_Texture* GameStateWrappedTexture::get_texture ()
{
    return sdl_texture;
}
