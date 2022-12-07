# How to package nautilus-dropbox

This is a work-in-progress guide on how to build Ubuntu, Debian, and
Fedora packages for nautilus-dropbox. It assumes you're building from
a 16.04 Ubuntu machine.

1. Obtain the signing tarball and extract all of the files into your `~/.gnupg`
   directory. If producing an RPM build, also import it into RPM with
   `sudo rpm --import /path/to/key`.

2. Install the build dependencies.

This is a non-exhaustive list of what you'll need:

```
sudo apt-get install pbuilder debootstrap devscripts libnautilus-extension-dev mock rpm expect createrepo cdbs gnome-common debian-archive-keyring python3-docutils
```

3. On a Debian/Ubuntu machine:
   1. Copy .pbuilderrc to ~/.pbuilderrc

   2. Create chroots for each of the Debian or Ubuntu builds you'll be doing.

   ```
   sudo DIST=trusty ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=trusty ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=jessie ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=jessie ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
   ```

   3. Build all the packages with: `python3 build_packages.py deb`

4. On a Fedora machine:
   1. Add yourself to the mock group (create it if it doesn't exist), and
      then logout and login for the group to take affect.
      `build_packages.py` will use `sudo` for a few of the commands, so add both
      your user account and root to the group.

   2. Copy rpm_resources/fedora-*.cfg to /etc/mock.

      If you need to add new configs when we start targetting a new Fedora version,
      you can find all the configs here:  https://github.com/rpm-software-management/mock

   3. Copy rpm_resources/.rpmmacros to ~/.rpmmacros

   4. Build all the packages with: `python3 build_packages.py rpm`

5. Gather the built outputs from both machines, and put them in a TAR named nautilus-dropbox-release.tar.gz

6. Update the ChangeLog with all the changes in the release.

7. Tag the release and push the tag to GitHub.
