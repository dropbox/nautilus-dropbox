#!/bin/bash

set -euxo pipefail

. ./distro-info.sh

DISTRO=ubuntu DISTS="$UBUNTU_CODENAMES" ./create-ubuntu-repo.sh
DISTRO=debian DISTS="$DEBIAN_CODENAMES" ./create-ubuntu-repo.sh

docker run -u "$(id -u):$(id -g)" -e DISTS="$FEDORA_CODENAMES" -v `pwd`:/src nautilus-dropbox/fedora ./create-yum-repo.sh