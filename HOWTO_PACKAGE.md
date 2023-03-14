# How to package nautilus-dropbox

This is a work-in-progress guide on how to build Ubuntu, Debian, and
Fedora packages for nautilus-dropbox. It assumes you're building from
an Ubuntu 22.10 machine. The individual deb and rpm packages are built
inside Docker containers.

1. Install Docker:

   ```
   sudo apt install docker.io
   ```

2. Build the deb and RPM packages. They will build inside Docker containers:

   ```
   ./build_packages.sh
   ```

3. (optional) Sign the *.changes files and RPMs (this happens internally in the build system at Dropbox).

   ```
   debsign -k FC918B335044912E build/ubuntu/pool/main/*.changes
   debsign -k FC918B335044912E build/debian/pool/main/*.changes
   rpmsign --addsign --key-id FC918B335044912E build/fedora/pool/*.rpm
   ```

4. Build the deb and yum repos:

   ```
   ./build_repos.sh
   ```

5. (optional) Sign the repo files (this happens internally in the build system at Dropbox).

   ```
   gpg -abs --digest-algo SHA256 -o build/ubuntu/dists/$DIST/Release.gpg build/ubuntu/dists/$DIST/Release
   ```

6. Update the ChangeLog with all the changes in the release.

7. Tag the release and push the tag to GitHub.