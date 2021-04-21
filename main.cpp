#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <SDL_net.h>
#include <SDL_ttf.h>

#include <assert.h>
#include <stdio.h>
#include <string>
#include <set>
#include <map>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "GameState.h"

#define STEAM

#ifdef STEAM
#include "steam/steam_api.h"
#endif


#ifdef STEAM

static const char* const achievement_names[] = { "LEVEL_6", NULL};

class SteamGameManager
{
private:
	STEAM_CALLBACK( SteamGameManager, OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived);
	STEAM_CALLBACK( SteamGameManager, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
    ISteamUserStats *m_pSteamUserStats;
    bool stats_ready = false;
    bool achievement_got[10] = {false};
    bool needs_send = false;

public:
    SteamGameManager():
    	m_CallbackUserStatsReceived( this, &SteamGameManager::OnUserStatsReceived ),
	    m_CallbackGameOverlayActivated( this, &SteamGameManager::OnGameOverlayActivated )
    {
        m_pSteamUserStats = SteamUserStats();
        m_pSteamUserStats->RequestCurrentStats();
    };
    void set_achievements(unsigned index)
    {
        if (achievement_got[index])
            return;
        achievement_got[index] = true;
        m_pSteamUserStats->SetAchievement(achievement_names[index]);
        needs_send = true;
    }
    void update_achievements(GameState* game_state);
};

void SteamGameManager::OnUserStatsReceived( UserStatsReceived_t *pCallback )
{
    stats_ready = true;
    for (int i = 0; achievement_names[i]; i++)
        m_pSteamUserStats->GetAchievement( achievement_names[i], &achievement_got[i]);
}

void SteamGameManager::OnGameOverlayActivated( GameOverlayActivated_t* pCallback )
{
}

void SteamGameManager::update_achievements(GameState* game_state)
{
    if (game_state->highest_level >= 6)
        set_achievements(0);

    if (needs_send)
    {
        m_pSteamUserStats->StoreStats();
        needs_send = false;
    }
}

#endif

void mainloop()
{
    char* save_path = SDL_GetPrefPath("CharlieBrej", "ComPressure");
    std::string save_filename = std::string(save_path) + "compressure.save";
    SDL_free(save_path);
    const char* load_filename = save_filename.c_str();

    
    {
        std::ifstream test_loadfile(save_filename);
        if (test_loadfile.fail() || test_loadfile.eof())
            load_filename = "compressure.save";
        test_loadfile.close();
    }

    GameState* game_state = new GameState(load_filename);
#ifdef STEAM
    SteamGameManager steam_manager;
    game_state->set_steam_user(SteamUser()->GetSteamID().CSteamID::ConvertToUint64(), SteamFriends()->GetPersonaName());

    int friend_count = SteamFriends()->GetFriendCount( k_EFriendFlagImmediate );
    for (int i = 0; i < friend_count; ++i)
    {
	    CSteamID friend_id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagImmediate);
        game_state->add_friend(friend_id.ConvertToUint64());
    }

#endif
    int frame = 0;
    
	while(true)
	{
        unsigned oldtime = SDL_GetTicks();
		if (game_state->events())
            break;
        game_state->advance();
        game_state->audio();
#ifdef STEAM
        steam_manager.update_achievements(game_state);
        SteamAPI_RunCallbacks();
#endif
        frame++;
        if (frame > 100 * 60)
        {
            game_state->render(true);
            game_state->save(save_filename.c_str());
            game_state->save_to_server();
            frame = 0;
        }
        else
        {
            game_state->render();
        }
        
        unsigned newtime = SDL_GetTicks();
        if ((newtime - oldtime) < 10)
            SDL_Delay(10 - (newtime - oldtime));
	}
    game_state->save(save_filename.c_str());
    game_state->save_to_server(true);
    delete game_state;
}

int main( int argc, char* argv[] )
{
#ifdef STEAM
	if (SteamAPI_RestartAppIfNecessary(1528120))
		return 1;
	if (!SteamAPI_Init())
		return 1;
#endif

    SDL_Init(SDL_INIT_VIDEO| SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);
    SDLNet_Init();
    TTF_Init();
    Mix_Init(0);
    
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    mainloop();

    Mix_Quit();
    TTF_Quit();
    SDLNet_Quit();
	IMG_Quit();
	SDL_Quit();


#ifdef STEAM
    SteamAPI_Shutdown();
#endif

	return 0;
}
