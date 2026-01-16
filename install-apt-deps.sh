#!/bin/bash

apt-get update
apt-get install -y gnome-common libgtk-4-dev libnautilus-extension-dev python3-gi python3-docutils \
    debootstrap devscripts libnautilus-extension-dev cdbs debian-archive-keyring debhelper

if [[ "$(uname -p)" == "aarch64" ]]
then
    apt-get install box64
fi
