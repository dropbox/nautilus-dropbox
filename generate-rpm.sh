#!/bin/sh

# you should run this script on fedora
# you need to have the following packages:
# rpmdevtools, rpm-build, gawk, nautilus-devel, automake,
# autoconf, gnome-common, libtool, gcc

if [ $(basename $(pwd)) != 'nautilus-dropbox' ]; then
    echo "This script must be run from the nautilus-dropbox folder"
    exit -1
fi

BUILD=1
while [ $# != 0 ]; do
    flag="$1"
    case "$flag" in
        -n)
	    BUILD=0
            ;;
    esac
    shift
done

# creating an RPM is easier than creating a debian package, surprisingly
# I call bullcrap on this statement.  They are exactly the same.
set -e

# get version
if which gawk; then
    CURVER=$(gawk '/^AC_INIT/{sub("AC_INIT\\(\\[nautilus-dropbox\\],", ""); sub("\\)", ""); print $0}' configure.in)
else
    CURVER=$(awk '/^AC_INIT/{sub("AC_INIT\(\[nautilus-dropbox\],", ""); sub("\)", ""); print $0}' configure.in)    
fi

# backup old rpmmacros file
if [ -e $HOME/.rpmmacros ] && [ ! -e $HOME/.rpmmacros.old ]; then
    mv $HOME/.rpmmacros $HOME/.rpmmacros.old
fi

cat <<EOF > $HOME/.rpmmacros
%_topdir      $(pwd)/rpmbuild
%_tmppath              $(pwd)/rpmbuild
%_smp_mflags  -j3
%_signature gpg
%_gpg_name 3565780E
%_gpgbin /usr/bin/gpg
EOF

# clean old package build
rm -rf nautilus-dropbox{-,_}*
rm -rf rpmbuild

# build directory tree
mkdir rpmbuild
for I in BUILD RPMS SOURCES SPECS SRPMS; do
    mkdir rpmbuild/$I
done;

if [ ! -x configure ]; then
    ./autogen.sh
fi

if [ ! -e Makefile ]; then
    ./configure
fi

# generate package
make dist

cp nautilus-dropbox-$CURVER.tar.bz2 rpmbuild/SOURCES/

cat <<EOF > rpmbuild/SPECS/nautilus-dropbox.spec
%define glib_version 2.14.0
%define nautilus_version 2.16.0
%define libgnome_version 2.16.0
%define pygtk2_version 2.12

Name:		nautilus-dropbox
Version:	$CURVER
Release:	1%{?dist}
Summary:	Dropbox integration for Nautilus
Group:		User Interface/Desktops
License:	GPL
URL:		http://www.dropbox.com/
Source0:	http://dl.dropbox.com/u/17/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id} -u -n)

Requires:	nautilus-extensions >= %{nautilus_version}
Requires:	glib2 >= %{glib_version}
Requires:	libgnome >= %{gnome_version}
Requires:	pygtk2 >= %{pygtk2_version}

BuildRequires:	nautilus-devel >= %{nautilus_version}
BuildRequires:	glib2-devel >= %{glib_version}
BuildRequires:	python-docutils
BuildRequires:  cairo-devel
BuildRequires:  gtk2-devel
BuildRequires:  atk-devel
BuildRequires:  pango-devel

%description
Nautilus Dropbox is an extension that integrates
the Dropbox web service with your GNOME Desktop.

Check us out at http://www.dropbox.com/

%prep
%setup -q


%build
export DISPLAY=$DISPLAY
%configure
make %{?_smp_mflags}

%install
rm -rf \$RPM_BUILD_ROOT
make install DESTDIR=\$RPM_BUILD_ROOT

rm \$RPM_BUILD_ROOT%{_libdir}/nautilus/extensions-2.0/*.la
rm \$RPM_BUILD_ROOT%{_libdir}/nautilus/extensions-2.0/*.a


%post
/sbin/ldconfig
update-desktop-database
touch --no-create %{_datadir}/icons/hicolor
if [ -x /usr/bin/gtk-update-icon-cache ]; then
  gtk-update-icon-cache -q %{_datadir}/icons/hicolor
fi
EOF

cat <<'EOF' >> rpmbuild/SPECS/nautilus-dropbox.spec
for I in /home/*/.dropbox-dist;
do
  # require a minimum version of 0.7.110
  DROPBOX_VERSION="$I/VERSION"
  if test -e "$DROPBOX_VERSION"; then
    DROPBOX_VERSION_MAJOR=`cat "$DROPBOX_VERSION" | cut -d . -f 1`
    DROPBOX_VERSION_MINOR=`cat "$DROPBOX_VERSION" | cut -d . -f 2`
    DROPBOX_VERSION_MICRO=`cat "$DROPBOX_VERSION" | cut -d . -f 3`

    if [ $DROPBOX_VERSION_MINOR -lt 7 ] || [ $DROPBOX_VERSION_MINOR -eq 7 ] && [ $DROPBOX_VERSION_MICRO -lt 110 ]
    then
      # stop dropbox
      dropbox stop > /dev/null 2>&1
      sleep 0.5
      killall dropbox > /dev/null 2>&1
      killall dropboxd > /dev/null 2>&1

      rm -rf "$I"
    fi
  fi
done

DEFAULTS_FILE="/etc/default/dropbox-repo"

if [ ! -e "$DEFAULTS_FILE" ]; then
    YUM_REPO_FILE="/etc/yum.repos.d/dropbox.repo"

    if ! rpm -q gpg-pubkey-5044912e-4b7489b1 > /dev/null 2>&1 ;  then
      rpm --import - <<KEYDATA
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
    if [ -d "/etc/yum.repos.d" ]; then
      cat > "$YUM_REPO_FILE" << REPOCONTENT
[Dropbox]
name=Dropbox Repository
baseurl=http://linux.dropbox.com/fedora/\$releasever/
gpgkey=http://linux.dropbox.com/fedora/rpm-public-key.asc
REPOCONTENT
    fi
fi

PROCS=`pgrep -x nautilus`

for PROC in $PROCS; do
  # Extract the display variable so that we can show a box.  
  # Hope they have xauth to localhost. 
  export `cat /proc/$PROC/environ | tr "\0" "\n" | grep DISPLAY` 

  zenity --question --timeout=30 --title=Dropbox --text='The Nautilus File Browser has to be restarted. Any open file browser windows will be closed in the process. Do this now?' > /dev/null 2>&1
  if [ $? -eq 0 ] ; then
    echo "Killing nautilus"
    kill $PROC
  fi
done

if ! pgrep -x dropbox > /dev/null 2>&1 ;  then
  zenity --info --timeout=5 --text='Dropbox installation successfully completed! You can start Dropbox from your applications menu.' > /dev/null 2>&1
  if [ $? -ne 0 ]; then
      echo
      echo 'Dropbox installation successfully completed! You can start Dropbox from your applications menu.'
  fi
fi
EOF

cat <<EOF >> rpmbuild/SPECS/nautilus-dropbox.spec
%postun
/sbin/ldconfig
update-desktop-database
touch --no-create %{_datadir}/icons/hicolor
if [ -x /usr/bin/gtk-update-icon-cache ]; then
  gtk-update-icon-cache -q %{_datadir}/icons/hicolor
fi

%clean
rm -rf \$RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
%{_libdir}/nautilus/extensions-2.0/*.so*
%{_datadir}/icons/hicolor/*
%{_datadir}/nautilus-dropbox/emblems/*
%{_bindir}/dropbox
%{_datadir}/applications/dropbox.desktop
%{_datadir}/man/man1/dropbox.1.gz

%changelog
* $(date +'%a %b %d %Y')  Rian Hunter <rian@dropbox.com> - $CURVER-1
- This package does not use a changelog
EOF

cd rpmbuild
if [ $BUILD -eq 1 ]; then
    rpmbuild -ba SPECS/nautilus-dropbox.spec

    # sign all rpms
    find . -name '*.rpm' | xargs rpm --addsign
else
    # Kind of silly but this is the easiest way to get this info the the build_slave.
    rpmbuild -bs SPECS/nautilus-dropbox.spec > ../buildme
fi

# restore old macros file
if [ -e $HOME/.rpmmacros.old ]; then
    mv $HOME/.rpmmacros.old $HOME/.rpmmacros
fi
