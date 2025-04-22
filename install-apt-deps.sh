#!/bin/bash

apt-get update
apt-get install -y gnome-common libgtk-4-dev libnautilus-extension-dev python3-gi python3-docutils \
    debootstrap devscripts libnautilus-extension-dev cdbs debian-archive-keyring debhelper \
    gir1.2-gdkpixbuf-2.0 python3-gi-cairo libgdk-pixbuf2.0-dev libgdk-pixbuf2.0-0 \
    gir1.2-gtk-3.0 gir1.2-glib-2.0 python3-cairo