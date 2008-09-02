#!/bin/sh

if [ $(basename $(pwd)) != 'nautilus-dropbox' ]; then
    echo "This script must be run from the nautilus-dropbox folder"
    exit -1
fi

# creating an RPM is easier than creating a debian package, surprisingly
set -e

# get version
if [ -x $(which gawk) ]; then
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
EOF

# clean old package build
rm -rf nautilus-dropbox{-,_}*
rm -rf rpmbuild

# build directory tree
mkdir rpmbuild
for I in BUILD RPMS SOURCES SPECS SRPMS; do
    mkdir rpmbuild/$I
done;

# generate package
make dist

cp nautilus-dropbox-$CURVER.tar.bz2 rpmbuild/SOURCES/

cat <<EOF > rpmbuild/SPECS/nautilus-dropbox.spec
%define glib_version 2.16.0
%define nautilus_version 2.20.0
%define libnotify_version 0.4.4
%define libgnome_version 2.20.0

Name:		nautilus-dropbox
Version:	0.4.0
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

BuildRequires:	nautilus-devel >= %{nautilus_version}
BuildRequires:	libnotify-devel >= %{libnotify_version}
BuildRequires:	glib2-devel >= %{glib_version}


%description
Nautilus Dropbox is an extension that integrates
the Dropbox web service with your GNOME Desktop.

Check out http://www.getdropbox.com/

%prep
%setup -q


%build
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


%changelog
* Mon Sep 1 2008 Rian Hunter <rian@getdropbox.com> - 0.4.0
- Initial Package
EOF

cd rpmbuild
rpmbuild -ba SPECS/nautilus-dropbox.spec

# restore old macros file
if [ -e $HOME/.rpmmacros.old ]; then
    mv $HOME/.rpmmacros.old $HOME/.rpmmacros
fi
