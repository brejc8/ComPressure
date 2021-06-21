bin_PROGRAMS = ComPressure ComPressureServer
ComPressure_SOURCES =  GameState.cpp GameState.h \
                    main.cpp \
                    Misc.cpp Misc.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h \
                    Compress.cpp Compress.h \
                    english.string japanese.string
                    
ComPressure_CXXFLAGS = @CXXFLAGS@ @SDL2_CFLAGS@ @SDL2_image_CFLAGS@ @SDL2_mixer_CFLAGS@ @SDL2_net_CFLAGS@ @SDL2_ttf_CFLAGS@ @ZLIB_CFLAGS@ @ZSTD_CFLAGS@ -I.  
ComPressure_LDADD=@SDL2_LIBS@ @SDL2_image_LIBS@ @SDL2_mixer_LIBS@ @SDL2_net_LIBS@ @SDL2_ttf_LIBS@ @ZLIB_LIBS@ @ZSTD_LIBS@ -lsteam_api
ComPressure_LDFLAGS=-L.

ComPressureServer_SOURCES =  ComPressureServer.cpp \
                    Compress.cpp Compress.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h \
                    Misc.cpp Misc.h

ComPressureServer_CXXFLAGS = @CXXFLAGS@ @SDL2_CFLAGS@ 
ComPressureServer_LDADD= -lz @ZSTD_LIBS@ -lpthread
ComPressureServer_LDFLAGS= -static

Level.string: Level.json stringify.py
	./stringify.py Level.json > Level.string

CXXFLAGS = -std=c++2a
