#!/bin/sh -e

#DISTS and DISTRO must be defined.
ARCHS="i386 amd64 source"

mkdir .cache

for DIST in $DISTS; do

mkdir -p dists/$DIST/main/binary-amd64
mkdir -p dists/$DIST/main/binary-i386
mkdir -p dists/$DIST/main/source

    cat > apt-ftparchive.conf <<EOF
Dir {
 ArchiveDir ".";
 CacheDir "./.cache";
};

Default {
 Packages::Compress ". gzip bzip2";
 Contents::Compress ". gzip bzip2";
};

TreeDefault {
 BinCacheDB "packages-\$(SECTION)-\$(ARCH).db";
 Directory "pool/\$(SECTION)";
 SrcDirectory "pool/\$(SECTION)";
 Packages "\$(DIST)/\$(SECTION)/binary-\$(ARCH)/Packages";
 Contents "\$(DIST)/Contents-\$(ARCH)";
};

Tree "dists/$DIST" {
 Sections "main";
 Architectures "i386 amd64 source";
}
EOF

    apt-ftparchive generate apt-ftparchive.conf

    cat > apt-release.conf <<EOF
APT::FTPArchive::Release::Codename "$DIST";
APT::FTPArchive::Release::Origin "Dropbox.com";
APT::FTPArchive::Release::Components "main";
APT::FTPArchive::Release::Label "Dropbox $DISTRO Repository";
APT::FTPArchive::Release::Architectures "$ARCHS";
APT::FTPArchive::Release::Suite "$DIST";
EOF

    apt-ftparchive -c apt-release.conf release dists/$DIST > dists/$DIST/Release
    gpg -abs -o dists/$DIST/Release.gpg dists/$DIST/Release

done
rm -rf apt-release.conf apt-ftparchive.conf

