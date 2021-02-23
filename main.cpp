#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <SDL_net.h>

#include <assert.h>
#include <stdio.h>
#include <string>
#include <set>
#include <map>
#include <stdio.h>
#include <iostream>
#include <fstream>

#include "GameState.h"

//#define STEAM

#ifdef STEAM
#include "steam/steam_api.h"
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
    game_state->set_steam_user(SteamUser()->GetSteamID()->ConvertToUint64(), SteamFriends()->GetPersonaName());
#endif
    int frame = 0;
    
	while(true)
	{
        unsigned oldtime = SDL_GetTicks();
        const char* username = "non-steam-user";
		if (game_state->events())
            break;
        game_state->advance();
        game_state->audio();
#ifdef STEAM
        SteamGameServer_RunCallbacks();
#endif
        game_state->render();
        unsigned newtime = SDL_GetTicks();
        frame++;
        if (frame > 100 * 60)
        {
            game_state->save(save_filename.c_str());
            game_state->post_to_server();
            frame = 0;
        }
        if ((newtime - oldtime) < 10)
            SDL_Delay(10 - (newtime - oldtime));
	}
    game_state->save(save_filename.c_str());
    game_state->post_to_server();
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
    Mix_Init(0);

    mainloop();

    Mix_Quit();
    SDLNet_Quit();
	IMG_Quit();
	SDL_Quit();


#ifdef STEAM
    SteamAPI_Shutdown();
#endif

	return 0;
}
