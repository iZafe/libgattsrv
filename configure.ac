AC_INIT([ggk], [1.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Check for libraries
AC_CHECK_LIB([pthread], pthread_create, [], [AC_MSG_ERROR([pthread library not found])])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.0], [], [AC_MSG_ERROR([glib-2.0 library not found])])
PKG_CHECK_MODULES([GIO], [gio-2.0 >= 2.0], [], [AC_MSG_ERROR([gio-2.0 library not found])])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 >= 2.0], [], [AC_MSG_ERROR([gobject-2.0 library not found])])

# Use pkg-config for CFLAGS and LIBS
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

AX_CXX_COMPILE_STDCXX(17)
AC_PROG_RANLIB
AC_PROG_CXX
AM_PROG_AR

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
AC_OUTPUT
