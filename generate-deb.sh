#!/bin/sh

# you will need dh-make, dpkg-dev, fakeroot, cdbs

set -e

if [ $(basename $(pwd)) != 'nautilus-dropbox' ]; then
    echo "This script must be run from the nautilus-dropbox folder"
    exit -1
fi

# get version
CURVER=$(mawk '/^AC_INIT/{sub("AC_INIT\(\[nautilus-dropbox\],", ""); sub("\)", ""); print $0}' configure.ac)

# clean old package build
rm -rf nautilus-dropbox{-,_}*

. ./distro-info.sh

if [ ! -x configure ]; then
    ./autogen.sh
fi

if [ ! -e Makefile ]; then
    ./configure
fi

# first generate package binary
make dist

# untar package
tar xjf nautilus-dropbox-$CURVER.tar.bz2

# go into package dir
cd nautilus-dropbox-$CURVER

# make debian dir
mkdir debian

# add our files
cat > debian/rules <<EOF
#!/usr/bin/make -f

include /usr/share/cdbs/1/rules/debhelper.mk
include /usr/share/cdbs/1/class/autotools.mk

# Avoid postinst-has-useless-call-to-ldconfig and pkg-has-shlibs-control-file-but-no-actual-shared-libs
DEB_DH_MAKESHLIBS_ARGS=-Xnautilus-dropbox

debian/tmp/usr/lib/nautilus/extensions-2.0/libnautilus-dropbox.so:
	mkdir -p debian/tmp/usr/lib/nautilus/extensions-2.0
	ln -s ../extensions-3.0/libnautilus-dropbox.so debian/tmp/usr/lib/nautilus/extensions-2.0/

debian/tmp/usr/lib/nautilus/extensions-3.0/libnautilus-dropbox.so:
	mkdir -p debian/tmp/usr/lib/nautilus/extensions-3.0
	ln -s ../extensions-2.0/libnautilus-dropbox.so debian/tmp/usr/lib/nautilus/extensions-3.0/

install/dropbox:: debian/tmp/usr/lib/nautilus/extensions-3.0/libnautilus-dropbox.so  debian/tmp/usr/lib/nautilus/extensions-2.0/libnautilus-dropbox.so
	rm debian/tmp/usr/lib/nautilus/extensions-?.0/*.la
	rm debian/tmp/usr/lib/nautilus/extensions-?.0/*.a

EOF
chmod a+x debian/rules

cat > debian/changelog <<EOF
dropbox ($CURVER) stable; urgency=low

  * Initial Release, This package doesn't use a changelog

 -- Dropbox <support@dropbox.com>  $(date -R)
EOF

cat > debian/copyright <<EOF
This package was debianized by Dropbox <support@dropbox.com> on
$(date -R).

It was downloaded from https://www.dropbox.com/download?dl=packages/nautilus-dropbox-$CURVER.tar.bz2

Copyright: 

  Copyright Dropbox, Inc.

All images included in this package constitute data and are licensed under the
Creative Commons Attribution-No Derivative Works 3.0 Unported License [1].  This
includes the files:

data/icons/hicolor/16x16/apps/dropbox.png
data/icons/hicolor/22x22/apps/dropbox.png
data/icons/hicolor/24x24/apps/dropbox.png
data/icons/hicolor/32x32/apps/dropbox.png
data/icons/hicolor/48x48/apps/dropbox.png
data/icons/hicolor/64x64/apps/dropbox.png
data/icons/hicolor/64x64/emblems/emblem-dropbox-syncing.png
data/icons/hicolor/64x64/emblems/emblem-dropbox-uptodate.png
data/icons/hicolor/64x64/emblems/emblem-dropbox-unsyncable.png

[1] http://creativecommons.org/licenses/by-nd/3.0/

All program source in this package is released under the terms of the
GNU GPL version 3 below.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

On Debian systems, the complete text of the GNU General
Public License can be found in \`/usr/share/common-licenses/GPL-3'.

The Debian packaging is Copyright (C) Dropbox, Inc. and
is licensed under the GPL, see above.
EOF


cat > debian/dropbox.postinst<<EOF
#!/bin/sh
# postinst script for dropbox
#
# see: dh_installdeb(1)

# summary of how this script can be called:
#        * <postinst> \`configure' <most-recently-configured-version>
#        * <old-postinst> abort-upgrade' <new version>
#        * <conflictor's-postinst> \`abort-remove' \`in-favour' <package>
#          <new-version>
#        * <postinst> \`abort-remove'
#        * <deconfigured's-postinst> \`abort-deconfigure' \`in-favour'
#          <failed-install-package> <version> \`removing'
#          <conflicting-package> <version>
# for details, see http://www.debian.org/doc/debian-policy/ or
# the debian-policy package

DEFAULTS_FILE="/etc/default/dropbox-repo"
UBUNTU_CODENAMES="$UBUNTU_CODENAMES"
DEBIAN_CODENAMES="$DEBIAN_CODENAMES"

EOF

# We split this into two so that we can replace variables in the first part but
# not have to escape everything in the 2nd part.  NB: Apparently the 'EOF' vs
# EOF makes a difference.

cat >> debian/dropbox.postinst<<'EOF'
case "$1" in
    configure)
    	gtk-update-icon-cache /usr/share/icons/hicolor > /dev/null 2>&1

        DISTRO=`lsb_release -s -i`

        if [ "$DISTRO" = "Ubuntu" ]; then
          DISTS=$UBUNTU_CODENAMES
          DISTRO="ubuntu"
        elif [ "$DISTRO" = "Debian" ]; then
          DISTS=$DEBIAN_CODENAMES
          DISTRO="debian"
        else
          echo "You are not running Debian or Ubuntu.  Not adding Dropbox repository."
          DISTRO=""
        fi

        if [ -n "$DISTRO" -a ! -e "$DEFAULTS_FILE" ]; then
          # Add the Dropbox repository.
          # Copyright (c) 2009 The Chromium Authors. All rights reserved.
          # Use of this source code is governed by a BSD-style license.

          # Install the repository signing key (see also:
          # https://linux.dropbox.com/fedora/rpm-public-key.asc)
          install_key() {
            APT_KEY="`which apt-key 2> /dev/null`"
            if [ -x "$APT_KEY" ]; then
              "$APT_KEY" add - >/dev/null 2>&1 <<KEYDATA
-----BEGIN PGP PUBLIC KEY BLOCK-----
Version: GnuPG v1.4.9 (GNU/Linux)

mQENBEt0ibEBCACv4hZRPqwtpU6z8+BB5YZU1a3yjEvg2W68+a6hEwxtCa2U++4d
zQ+7EqaUq5ybQnwtbDdpFpsOi9x31J+PCpufPUfIG694/0rlEpmzl2GWzY8NqfdB
FGGm/SPSSwvKbeNcFMRLu5neo7W9kwvfMbGjHmvUbzBUVpCVKD0OEEf1q/Ii0Qce
kx9CMoLvWq7ZwNHEbNnij7ecnvwNlE2MxNsOSJj+hwZGK+tM19kuYGSKw4b5mR8I
yThlgiSLIfpSBh1n2KX+TDdk9GR+57TYvlRu6nTPu98P05IlrrCP+KF0hYZYOaMv
Qs9Rmc09tc/eoQlN0kkaBWw9Rv/dvLVc0aUXABEBAAG0MURyb3Bib3ggQXV0b21h
dGljIFNpZ25pbmcgS2V5IDxsaW51eEBkcm9wYm94LmNvbT6JATYEEwECACAFAkt0
ibECGwMGCwkIBwMCBBUCCAMEFgIDAQIeAQIXgAAKCRD8kYszUESRLi/zB/wMscEa
15rS+0mIpsORknD7kawKwyda+LHdtZc0hD/73QGFINR2P23UTol/R4nyAFEuYNsF
0C4IAD6y4pL49eZ72IktPrr4H27Q9eXhNZfJhD7BvQMBx75L0F5gSQwuC7GdYNlw
SlCD0AAhQbi70VBwzeIgITBkMQcJIhLvllYo/AKD7Gv9huy4RLaIoSeofp+2Q0zU
HNPl/7zymOqu+5Oxe1ltuJT/kd/8hU+N5WNxJTSaOK0sF1/wWFM6rWd6XQUP03Vy
NosAevX5tBo++iD1WY2/lFVUJkvAvge2WFk3c6tAwZT/tKxspFy4M/tNbDKeyvr6
85XKJw9ei6GcOGHD
=5rWG
-----END PGP PUBLIC KEY BLOCK-----
KEYDATA
            fi
          }

          DISTRIB_CODENAME=`lsb_release -s -c`

          for DIST in $DISTS; do
              REPO=$DIST
              if [ "$DIST" = "$DISTRIB_CODENAME" ]; then
                break
              fi
          done

          REPOCONFIG="deb [arch=i386,amd64] http://linux.dropbox.com/$DISTRO $REPO main"

          APT_GET="`which apt-get 2> /dev/null`"
          APT_CONFIG="`which apt-config 2> /dev/null`"

          # Parse apt configuration and return requested variable value.
          apt_config_val() {
            APTVAR="$1"
            if [ -x "$APT_CONFIG" ]; then
              "$APT_CONFIG" dump | sed -e "/^$APTVAR /"'!d' -e "s/^$APTVAR \"\(.*\)\".*/\1/"
            fi
          }

          # Set variables for the locations of the apt sources lists.
          find_apt_sources() {
            APTDIR=$(apt_config_val Dir)
            APTETC=$(apt_config_val 'Dir::Etc')
            APT_SOURCES="$APTDIR$APTETC$(apt_config_val 'Dir::Etc::sourcelist')"
            APT_SOURCESDIR="$APTDIR$APTETC$(apt_config_val 'Dir::Etc::sourceparts')"
          }

          # Add the Dropbox repository to the apt sources.
          # Returns:
          # 0 - no update necessary
          # 1 - sources were updated
          # 2 - error
          update_sources_lists() {
            if [ ! "$REPOCONFIG" ]; then
              return 0
            fi

            find_apt_sources

            if [ -d "$APT_SOURCESDIR" ]; then
              # Nothing to do if it's already there.
              SOURCELIST=$(grep -H "$REPOCONFIG" "$APT_SOURCESDIR/dropbox.list" \
                2>/dev/null | cut -d ':' -f 1)
              if [ -n "$SOURCELIST" ]; then
                return 0
              fi

              printf "$REPOCONFIG\n" > "$APT_SOURCESDIR/dropbox.list"
              if [ $? -eq 0 ]; then
                return 1
              fi
            fi
            return 2
          }

          install_key
          update_sources_lists

        fi

        for I in /home/*/.dropbox-dist; do
          # require a minimum version of 1.0.0
          DROPBOX_VERSION="$I/VERSION"
          if test -e "$DROPBOX_VERSION"; then
            VERSION=`cat "$DROPBOX_VERSION"`

            case "$VERSION" in
              2.[0-9].*|1.*.*|0.*.*)
                # Anything below 2.10 is deprecated!
                # stop dropbox
                pkill -xf $I/dropbox > /dev/null 2>&1
                sleep 0.5
                rm -rf "$I"
            esac
          fi
        done

        UPDATENOTIFIERDIR=/var/lib/update-notifier/user.d
        echo "Please restart all running instances of Nautilus, or you will experience problems. i.e. nautilus --quit"

        if [ -d $UPDATENOTIFIERDIR ] ; then
          # pgrep matches application names from /proc/<pid>/status which is
          # truncated according to sys/procfs.h definition. Problem is it's
          # platform dependent. Either 15 or 16 chars.
          if pgrep -x nautilus > /dev/null 2>&1 ;  then
            cat > $UPDATENOTIFIERDIR/dropbox-restart-required <<DROPBOXEND
Name: Nautilus Restart Required
Priority: High
Terminal: False
Command: nautilus -q
ButtonText: _Restart Nautilus
DontShowAfterReboot: True
DisplayIf: pgrep -x nautilus -U \$(id -u) > /dev/null
Description: Dropbox requires Nautilus to be restarted to function properly.
DROPBOXEND
            # sometimes this doesn't happen:
            touch /var/lib/update-notifier/dpkg-run-stamp
          else
            rm -f $UPDATENOTIFIERDIR/dropbox-restart-required
          fi
        fi

        echo 'Dropbox installation successfully completed! You can start Dropbox from your applications menu.'

        if [ -d $UPDATENOTIFIERDIR ] ; then
            cat > $UPDATENOTIFIERDIR/dropbox-start-required <<DROPBOXEND
Name: Dropbox Start Required
Priority: High
Command: dropbox start -i
Terminal: False
DontShowAfterReboot: True
DisplayIf: dropbox running > /dev/null
Description: Start Dropbox to finish installation.
ButtonText: _Start Dropbox
DROPBOXEND
          # sometimes this doesn't happen:
          touch /var/lib/update-notifier/dpkg-run-stamp
        fi

	;;

    abort-upgrade|abort-remove|abort-deconfigure)
    ;;

    *)
        echo "postinst called with unknown argument '$1'" >&2
        exit 1
    ;;
esac

set -e

# dh_installdeb will replace this with shell code automatically
# generated by other debhelper scripts.

#DEBHELPER#

exit 0
EOF

cat > debian/postrm <<'EOF'
#!/bin/sh
# Remove the Dropbox repository.
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license.

set -e
action="$1"

# Only do complete clean-up on purge.
if [ "$action" != "purge" ] ; then
  exit 0
fi

APT_GET="`which apt-get 2> /dev/null`"
APT_CONFIG="`which apt-config 2> /dev/null`"

# Parse apt configuration and return requested variable value.
apt_config_val() {
  APTVAR="$1"
  if [ -x "$APT_CONFIG" ]; then
    "$APT_CONFIG" dump | sed -e "/^$APTVAR /"'!d' -e "s/^$APTVAR \"\(.*\)\".*/\1/"
  fi
}

uninstall_key() {
  APT_KEY="`which apt-key 2> /dev/null`"
  if [ -x "$APT_KEY" ]; then
    # don't fail if the key wasn't found
    "$APT_KEY" rm 5044912E >/dev/null 2>&1 || true
  fi
}

# Set variables for the locations of the apt sources lists.
find_apt_sources() {
  APTDIR=$(apt_config_val Dir)
  APTETC=$(apt_config_val 'Dir::Etc')
  APT_SOURCES="$APTDIR$APTETC$(apt_config_val 'Dir::Etc::sourcelist')"
  APT_SOURCESDIR="$APTDIR$APTETC$(apt_config_val 'Dir::Etc::sourceparts')"
}

# Remove a repository from the apt sources.
# Returns:
# 0 - successfully removed, or not configured
# 1 - failed to remove
clean_sources_lists() {
  find_apt_sources

  if [ -d "$APT_SOURCESDIR" ]; then
    rm -f "$APT_SOURCESDIR/dropbox.list"
  fi

  return 0
}

uninstall_key
clean_sources_lists

rm -rf /var/lib/update-notifier/user.d/dropbox-restart-required
rm -rf /var/lib/update-notifier/user.d/dropbox-start-required

# dh_installdeb will replace this with shell code automatically
# generated by other debhelper scripts.

#DEBHELPER#

exit 0
EOF

echo "6" > debian/compat

cat > debian/dropbox.install <<EOF
debian/tmp/usr/*

EOF

# NOTE: When updating dependencies, make sure to test the package on
# the minimum and maximum supported versions for each distro. In
# particular, debian version ordering can be tricky. E.g.
# "1.3.0~somestring" is considered lower than "1.3.0" since usually
# the "~" is appended to denote a beta version.
#
# For more info, see: https://askubuntu.com/a/229749

cat > debian/control <<EOF
Source: dropbox
Section: gnome
Priority: optional
Maintainer: Dropbox <support@dropbox.com>
Build-Depends: cdbs, debhelper (>= 9), build-essential, libnautilus-extension-dev (>= 3.10.1), libglib2.0-dev (>= 2.40), python-gi (>= 3.12), python-docutils
Standards-Version: 3.9.4.0

Package: dropbox
Replaces: nautilus-dropbox
Breaks: nautilus-dropbox
Provides: nautilus-dropbox
Architecture: any
Depends: procps, python-gi (>= 3.12), python (>= 2.7), \${python:Depends}, \${misc:Depends}, libatk1.0-0 (>= 2.10), libc6 (>= 2.19), libcairo2 (>= 1.13), libglib2.0-0 (>= 2.40), libgtk-3-0 (>= 3.10.8), libpango1.0-0 (>= 1.36.3), lsb-release, gir1.2-gdkpixbuf-2.0 (>= 2.30.7), gir1.2-glib-2.0 (>= 1.40.0), gir1.2-gtk-3.0 (>= 3.10.8), gir1.2-pango-1.0 (>= 1.36.3)
Suggests: nautilus (>= 3.10.1), python-gpg (>= 1.8.0)
Homepage: https://www.dropbox.com/
Description: cloud synchronization engine - CLI and Nautilus extension
 Dropbox is a free service that lets you bring your photos, docs, and videos
 anywhere and share them easily.
 .
 This package provides a command-line tool and a Nautilus extension that
 integrates the Dropbox web service with your GNOME Desktop.

Package: nautilus-dropbox
Depends: dropbox, \${misc:Depends}
Architecture: all
Section: oldlibs
Description: transitional dummy package for dropbox
 This is a transitional dummy package. It can safely be removed.

EOF

# Kind of silly but this is the easiest way to communicate this info
# to build_packages.py.
echo nautilus-dropbox-$CURVER > ../buildme
