#!/bin/bash

apt-get update
apt-get install -y gnome-common libgtk-4-dev libnautilus-extension-dev python3-gi python3-docutils \
    debootstrap devscripts libnautilus-extension-dev cdbs debian-archive-keyring debhelper \
    gir1.2-gdkpixbuf-2.0 gir1.2-gtk-4.0

# Ensure mkdir is available in /usr/sbin
ln -sf /bin/mkdir /usr/sbin/mkdir 