AC_INIT([ComPressure], [0.0.1])
AM_INIT_AUTOMAKE
AC_LANG(C++)
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])

AM_PATH_SDL2(2.0,:,AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!]))
CXXFLAGS="$CXXFLAGS $SDL_CFLAGS -I/home/brejc8/sdk/public/"
LIBS="$LIBS -lSDL2_image -lSDL2_mixer $SDL_LIBS libsteam_api.so "


AC_OUTPUT

