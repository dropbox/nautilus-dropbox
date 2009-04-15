#!/bin/sh

# you should run this script on fedora
# you need to have the following packages:
# rpmdevtools, rpm-build, gawk, nautilus-devel, libnotify-devel, automake,
# autoconf, gnome-common, libtool, gcc

if [ $(basename $(pwd)) != 'nautilus-dropbox' ]; then
    echo "This script must be run from the nautilus-dropbox folder"
    exit -1
fi

# creating an RPM is easier than creating a debian package, surprisingly
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
%_tmppath              $(pwd)/r/pmbuild
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
%define wget_version 1.10.0
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
Requires:	wget >= %{wget_version}
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
killall nautilus > /dev/null 2>&1
EOF

cat <<'EOF' >> rpmbuild/SPECS/nautilus-dropbox.spec
# stop dropbox
dropbox stop

# kill all old installations 
for I in /home/*/.dropbox-dist; do
  rm -rf "$I"
done

START=$$
while [ $START -ne 1 ]; do
  TTY=$(ps ax | awk "\$1 ~ /$START/ { print \$2 }")
  if [ $TTY != "?" ]; then
    break
  fi
  
  START=$(cat /proc/$START/stat | awk '{print $4}')
done

if [ $TTY != "?" ]; then
  ESCTTY=$(echo $TTY | sed -e 's/\//\\\//')
  U=$(who | awk "\$2 ~ /$ESCTTY/ {print \$1}" | sort | uniq)
  if [ "$(whoami)" != "$U" ]; then
    if [ "$(whoami)" == "root" ]; then
      # kill all old installations, in case they they don't use /home
      su -c 'rm -rf ~/.dropbox-dist' $U
      su -c "dropbox start -i" $U &
    fi
  else
    # kill all old installations, in case they they don't use /home
    rm -rf ~/.dropbox-dist
    dropbox start -i &
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
killall nautilus > /dev/null 2>&1


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
* Mon Sep 1 2008 Rian Hunter <rian@getdropbox.com> - 0.4.1
- Bugfix release

* Mon Sep 1 2008 Rian Hunter <rian@getdropbox.com> - 0.4.0
- Initial Package
EOF

cd rpmbuild
rpmbuild -ba SPECS/nautilus-dropbox.spec

# sign all rpms
find . -name '*.rpm' | xargs rpm --addsign

# restore old macros file
if [ -e $HOME/.rpmmacros.old ]; then
    mv $HOME/.rpmmacros.old $HOME/.rpmmacros
fi
