#!/bin/bash

dnf install -y gnome-common nautilus-devel gtk4-devel python3-docutils python3-gobject \
    devscripts mock rpm rpm-sign rpmdevtools rpmlint expect createrepo cdbs gnome-common \
    python3-docutils rpm-build gawk which atk-devel

# Set Python 3.9 as the default python3
update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.9 1