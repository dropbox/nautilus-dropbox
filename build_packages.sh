#!/bin/bash

set -euxo pipefail

rm -rf build

# Build deb packages.
for DISTRO in ubuntu debian debian_i386; do
    docker build -t nautilus-dropbox/$DISTRO -f Dockerfile.$DISTRO .
    docker run -u "$(id -u):$(id -g)" -v `pwd`:/src nautilus-dropbox/$DISTRO ./generate-deb.sh
done

# Build fedora rpm.
docker build -t nautilus-dropbox/fedora -f Dockerfile.fedora .
docker run -u "$(id -u):$(id -g)" -v `pwd`:/src nautilus-dropbox/fedora ./generate-rpm.sh

# Build the source tgz.
make dropbox dist
mkdir -p build/packages
cp dropbox build/packages/dropbox.py
cp nautilus-dropbox-*.tar.bz2 build/packages/