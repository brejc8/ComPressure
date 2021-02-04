#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <set>
#include <map>
#include <stdio.h>

#include "GameState.h"

#define STEAM

#ifdef STEAM
#include "steam/steam_api.h"
#endif


void mainloop()
{
    GameState* game_state = new GameState("compressure.save");
	while(true)
	{
        unsigned oldtime = SDL_GetTicks();
        game_state->advance();
		if (game_state->events())
            break;
        game_state->audio();
#ifdef STEAM
        SteamGameServer_RunCallbacks();
#endif
        game_state->render();
        unsigned newtime = SDL_GetTicks();
        if ((newtime - oldtime) < 10)
            SDL_Delay(10 - (newtime - oldtime));

	}
    game_state->save("compressure.save");
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
    Mix_Init(0);
    mainloop();
    Mix_Quit();
	IMG_Quit();
	SDL_Quit();

	return 0;
}
