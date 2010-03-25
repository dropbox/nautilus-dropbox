#!/bin/sh

# you should run this script on fedora
# you need to have the following packages:
# rpmdevtools, rpm-build, gawk, nautilus-devel, libnotify-devel, automake,
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
%define libnotify_version 0.4.4
%define libgnome_version 2.16.0
%define pygtk2_version 2.12

Name:		nautilus-dropbox
Version:	$CURVER
Release:	1%{?dist}
Summary:	Dropbox integration for Nautilus
Group:		User Interface/Desktops
License:	GPL
URL:		http://www.getdropbox.com/
Source0:	http://dl.getdropbox.com/u/17/%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Requires:	nautilus >= %{nautilus_version}
Requires:	libnotify >= %{libnotify_version}
Requires:	glib2 >= %{glib_version}
Requires:	libgnome >= %{gnome_version}
Requires:	pygtk2 >= %{pygtk2_version}

BuildRequires:	nautilus-devel >= %{nautilus_version}
BuildRequires:	libnotify-devel >= %{libnotify_version}
BuildRequires:	glib2-devel >= %{glib_version}
BuildRequires:	python-docutils

%description
Nautilus Dropbox is an extension that integrates
the Dropbox web service with your GNOME Desktop.

Check us out at http://www.getdropbox.com/

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

zenity --info --timeout=5 --text='Dropbox installation successfully completed! Please log out and log back in to complete the integration with your desktop. You can start Dropbox from your applications menu.' > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo
  echo 'Dropbox installation successfully completed! Please log out and log back in to complete the integration with your desktop. You can start Dropbox from your applications menu.'
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
%{_bindir}/dropbox
%{_datadir}/applications/dropbox.desktop
%{_datadir}/man/man1/dropbox.1.gz

%changelog
* $(date +'%a %b %d %Y')  Rian Hunter <rian@getdropbox.com> - $CURVER
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
