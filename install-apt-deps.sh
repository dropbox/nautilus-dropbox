#!/bin/bash

apt-get update
apt-get install -y gnome-common libgtk-3-dev libnautilus-extension-dev python3-gi python3-docutils \
    debootstrap devscripts libnautilus-extension-dev cdbs debian-archive-keyring debhelper \
    gdk-pixbuf-2.0 gir1.2-gdkpixbuf-2.0 python3-gi-cairo gir1.2-gtk-3.0 \
    libgdk-pixbuf-xlib-2.0-dev libglib2.0-dev