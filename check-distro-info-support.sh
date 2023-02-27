#!/bin/bash
set -euo pipefail

current_version_codename=$1
current_version_display_name=$2
supported_version_codenames=$3

for supported_version in $supported_version_codenames; do
    if [[ "$current_version_codename" == "$supported_version" ]]; then
        printf "$current_version_display_name is supported."
        exit 0
    fi
done

printf "$current_version_display_name needs to be added to distro-info.sh in order to produce the correct repo."
exit 1
