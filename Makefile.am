bin_PROGRAMS = pressure
pressure_SOURCES =  GameState.cpp GameState.h \
                    main.cpp \
                    Misc.cpp Misc.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h

pressure_LDADD = texture_png.o

CLEANFILES = texture_png.o

texture_png.o: texture.png
	objcopy --input binary --output elf64-x86-64 --binary-architecture i386 texture.png texture_png.o --prefix-symbols embedded_data


AM_CXXFLAGS = -std=gnu++11
