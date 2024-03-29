AC_INIT([ComPressure], [0.0.1])
AM_INIT_AUTOMAKE([foreign])
AC_LANG(C++)
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AC_CANONICAL_HOST
AC_PROG_OBJCXX

PKG_CHECK_MODULES([SDL2], [sdl2])
PKG_CHECK_MODULES([SDL2_image], [SDL2_image])
PKG_CHECK_MODULES([SDL2_mixer], [SDL2_mixer])
PKG_CHECK_MODULES([SDL2_net], [SDL2_net])
PKG_CHECK_MODULES([SDL2_ttf], [SDL2_ttf])
PKG_CHECK_MODULES([ZLIB], [zlib])
PKG_CHECK_MODULES([ZSTD], [libzstd])

build_linux=no
build_mac=no

# Detect the target system
case "${host_os}" in
    linux*)
        build_linux=yes
        PKG_CHECK_MODULES([XCB], [xcb])
        ;;
    darwin*)
        build_mac=yes
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

AC_ARG_ENABLE([steam],
[  --disable-steam    Turn off Steam],
[case "${enableval}" in
  yes) steam=true ;;
  no)  steam=false ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --enable-steam]) ;;
esac],[steam=true])

AC_ARG_WITH(charles-id,
            AS_HELP_STRING([--with-charles-id=N],
                           [Secret server ID (default=0)]),
            , with_charles_id=0)



AM_CONDITIONAL([NO_STEAM], [test x$steam = xfalse])
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])
AM_CONDITIONAL([MACOS], [test "$build_mac" = "yes"])
AC_DEFINE_UNQUOTED([CHARLES_ID], [$with_charles_id])

AC_OUTPUT
