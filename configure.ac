# Initialization

AC_INIT([nautilus-dropbox], 2025.05.20)

AM_INIT_AUTOMAKE([foreign])

AC_PREFIX_DEFAULT(/usr)

AM_CONFIG_HEADER(config.h)

# not ready for i18n yet
#AC_PROG_INTLTOOL([0.29])

# Dependency checks
NAUTILUS_REQUIRED=43.rc
GLIB_REQUIRED=2.14.0

# Used programs
AC_PROG_CC
AM_PROG_LIBTOOL

# Check for pkg-config
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(NAUTILUS, libnautilus-extension-4 >= $NAUTILUS_REQUIRED)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED)
PKG_CHECK_MODULES(GTK, gtk4)

AC_SUBST(GTK_CFLAGS)

AC_PATH_PROG([PYTHON3], [python3])

AC_PATH_PROG([RST2MAN], [rst2man], [python3 rst2man.py])
AC_SUBST(RST2MAN)

# define module checking macro
AC_DEFUN([PYTHON3_CHECK_MODULE], [
AC_MSG_CHECKING([for $1 on python3])

cat <<EOF | python3
try:
 import $2
except:
 exit(1)
else:
 exit(0)
EOF

if test $? -ne 0; then
   AC_MSG_RESULT([no])
   AC_MSG_ERROR([couldn't find $1])
else
   AC_MSG_RESULT([yes])
fi
])

PYTHON3_CHECK_MODULE(docutils, docutils)

PYTHON3_CHECK_MODULE(gi, gi)

# Make dependency CFLAGS and LIBS available
AC_SUBST(NAUTILUS_CFLAGS)
AC_SUBST(NAUTILUS_LIBS)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# lol stolen from the automake manual
AC_ARG_ENABLE([debug],
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
yes) debug=true ;;
no)  debug=false ;;
*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_ARG_WITH(nautilus-extension-dir,
              [AS_HELP_STRING([--with-nautilus-extension-dir],
                    [specify the nautilus extension directory])])

# Get nautilus extensions directory
AC_MSG_CHECKING([for nautilus extension directory])
if test -n "$with_nautilus_extension_dir"; then
    NAUTILUS_EXTENSION_DIR=$with_nautilus_extension_dir
else
    NAUTILUS_EXTENSION_DIR=`$PKG_CONFIG --variable=extensiondir libnautilus-extension-4`
fi
if test -z "$NAUTILUS_EXTENSION_DIR"; then
    NAUTILUS_EXTENSION_DIR='${exec_prefix}/lib/nautilus/extension-4'
fi

AC_MSG_RESULT([${NAUTILUS_EXTENSION_DIR}])
AC_SUBST(NAUTILUS_EXTENSION_DIR)

NAUTILUS_VERSION=`$PKG_CONFIG --modversion libnautilus-extension`
if test -n "$NAUTILUS_VERSION"; then
    NAUTILUS_VERSION_MAJOR=`echo -n $NAUTILUS_VERSION | cut -d . -f 1`
    NAUTILUS_VERSION_MINOR=`echo -n $NAUTILUS_VERSION | cut -d . -f 2`
    case "$NAUTILUS_VERSION_MINOR" in
      *[[a-zA-Z]]* )
          NAUTILUS_VERSION_MINOR=0
          ;;
    esac
    NAUTILUS_VERSION_MICRO=`echo -n $NAUTILUS_VERSION | cut -d . -f 3`
fi

AC_DEFINE_UNQUOTED(NAUTILUS_VERSION_MAJOR, [$NAUTILUS_VERSION_MAJOR], [ nautilus version major])
AC_DEFINE_UNQUOTED(NAUTILUS_VERSION_MINOR, [$NAUTILUS_VERSION_MINOR], [ nautilus version minor])
AC_DEFINE_UNQUOTED(NAUTILUS_VERSION_MICRO, [$NAUTILUS_VERSION_MICRO], [ nautilus version micro])

EMBLEM_DIR='${datadir}/nautilus-dropbox/emblems'
AC_SUBST(EMBLEM_DIR)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	data/Makefile
	data/icons/Makefile
	data/icons/hicolor/Makefile
	data/icons/hicolor/16x16/Makefile
	data/icons/hicolor/16x16/apps/Makefile
	data/icons/hicolor/22x22/Makefile
	data/icons/hicolor/22x22/apps/Makefile
	data/icons/hicolor/24x24/Makefile
	data/icons/hicolor/24x24/apps/Makefile
	data/icons/hicolor/32x32/Makefile
	data/icons/hicolor/32x32/apps/Makefile
	data/icons/hicolor/48x48/Makefile
	data/icons/hicolor/48x48/apps/Makefile
	data/icons/hicolor/64x64/Makefile
	data/icons/hicolor/64x64/apps/Makefile
	data/icons/hicolor/256x256/Makefile
	data/icons/hicolor/256x256/apps/Makefile
	data/emblems/Makefile])

AC_SUBST([MKDIR_P], ['/bin/mkdir -p'])

AC_OUTPUT
