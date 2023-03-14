#!/bin/sh

set -e

# DISTS must be defined.
cd build/fedora
ARCHS="x86_64"

for DIST in $DISTS; do

  for ARCH in $ARCHS; do
    mkdir -p $DIST/$ARCH
    cd  $DIST/$ARCH
    ln -s ../../pool/*$ARCH*.rpm .
    cd ../..

  done

  createrepo $DIST
done

# Go back up to top-level directory to leave the caller in the same dir they started.
cd ../..