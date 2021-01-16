#include <SDL.h>
#include <SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <set>
#include <map>
#include <unistd.h>
#include <stdio.h>

#include "GameState.h"

void mainloop()
{
    GameState* game_state = new GameState("pressure.save");
	while(true)
	{
        game_state->advance();
		if (game_state->events())
            break;
        game_state->render();
	}
    game_state->save("pressure.save");
    delete game_state;
}
int main( int argc, char* argv[] )
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    mainloop();
	IMG_Quit();
	SDL_Quit();

	return 0;
}
