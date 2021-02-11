bin_PROGRAMS = ComPressure
ComPressure_SOURCES =  GameState.cpp GameState.h \
                    main.cpp \
                    Misc.cpp Misc.h \
                    SaveState.cpp SaveState.h \
                    Circuit.cpp Circuit.h \
                    Level.cpp Level.h \
                    Dialogue.cpp Dialogue.h
EXTRA_DIST = dialogue.txt

CLEANFILES = dialogue.string

dialogue.string: dialogue.txt fold.py
	./fold.py dialogue.txt > dialogue.string

AM_CXXFLAGS = -std=gnu++11 
