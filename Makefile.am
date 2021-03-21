bin_PROGRAMS = ComPressure ComPressureServer
ComPressure_SOURCES =  GameState.cpp GameState.h \
                    main.cpp \
                    Misc.cpp Misc.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h \
                    Dialogue.cpp Dialogue.h \
                    Compress.cpp Compress.h
                    
ComPressure_CXXFLAGS = @CXXFLAGS@ @SDL2_CFLAGS@ @SDL2_image_CFLAGS@ @SDL2_mixer_CFLAGS@ @SDL2_net_CFLAGS@ @SDL2_ttf_CFLAGS@ @ZLIB_CFLAGS@ -I.  
ComPressure_LDADD=@SDL2_LIBS@ @SDL2_image_LIBS@ @SDL2_mixer_LIBS@ @SDL2_net_LIBS@ @SDL2_ttf_LIBS@ @ZLIB_LIBS@ -lsteam_api
ComPressure_LDFLAGS=-L.

ComPressureServer_SOURCES =  ComPressureServer.cpp \
                    Compress.cpp Compress.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h

ComPressureServer_CXXFLAGS = @SDL2_CFLAGS@
ComPressureServer_LDADD= -lz 
ComPressureServer_LDFLAGS= -static

EXTRA_DIST = dialogue.txt

CLEANFILES = dialogue.string

dialogue.string: dialogue.txt fold.py
	./fold.py dialogue.txt > dialogue.string

CXXFLAGS = -std=c++20
