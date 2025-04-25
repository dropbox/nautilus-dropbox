#!/bin/bash

apt-get update
apt-get install -y gnome-common libgtk-4-dev libnautilus-extension-dev python3-gi python3-docutils \
    debootstrap devscripts libnautilus-extension-dev cdbs debian-archive-keyring debhelper \
    libgdk-pixbuf-2.0-0 gir1.2-gdkpixbuf-2.0 libgdk-pixbuf2.0-dev \
    python3-gi-cairo python3-cairo gir1.2-gtk-3.0 gir1.2-glib-2.0