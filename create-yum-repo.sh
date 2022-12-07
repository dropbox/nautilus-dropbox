#!/bin/sh

set -e

# DISTS must be defined.
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
