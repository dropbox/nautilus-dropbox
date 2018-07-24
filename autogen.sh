#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="nautilus-dropbox"
REQUIRED_AUTOMAKE_VERSION=1.7

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
    echo "You need to install gnome-common"
    exit 1
}
USE_GNOME2_MACROS=1 REQUIRED_AUTOMAKE_VERSION=1.7 . gnome-autogen.sh
