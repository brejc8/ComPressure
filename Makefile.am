bin_PROGRAMS = ComPressure ComPressureServer
ComPressure_SOURCES =  GameState.cpp GameState.h \
                    main.cpp \
                    Misc.cpp Misc.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h \
                    Dialogue.cpp Dialogue.h \
                    Compress.cpp Compress.h
ComPressure_LDADD=@SDL_LIBS@ -lSDL2_image -lSDL2_mixer -lSDL2_net -lz \
                    /home/brejc8/sdk/redistributable_bin/linux64/libsteam_api.so
 
ComPressureServer_SOURCES =  ComPressureServer.cpp \
                    Compress.cpp Compress.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h

ComPressureServer_LDADD= -lz 
ComPressureServer_LDFLAGS= #-static

EXTRA_DIST = dialogue.txt

CLEANFILES = dialogue.string

dialogue.string: dialogue.txt fold.py
	./fold.py dialogue.txt > dialogue.string

AM_CXXFLAGS = -std=gnu++11 
