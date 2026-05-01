#!/bin/bash

apt-get update
apt-get install -fy \
    debhelper \
    debian-archive-keyring \
    debootstrap \
    devscripts \
    gnome-common \
    libgtk-4-dev \
    libnautilus-extension-dev \
    libnautilus-extension-dev \
    python3-docutils \
    python3-gi

case "${DBX_BUILD_ENV:=local}" in
    packaging)
        apt-get install -fy cdbs
        ;;

    *)
        ;;
esac