# How to package nautilus-dropbox

This is a work-in-progress guide on how to build Ubuntu, Debian, and
Fedora packages for nautilus-dropbox. It assumes you're building from
an Ubuntu 22.10 machine for the `.deb` build and a Fedora 37 machine for the `.rpm` build.

1. Obtain the signing tarball and extract all of the files into your `~/.gnupg`
   directory. If producing an RPM build, also import it into RPM with
   `sudo rpm --import /path/to/key`.

2. On a Debian/Ubuntu machine:
   1. Install dependencies:

      ```
      sudo apt-get install pbuilder debootstrap devscripts libnautilus-extension-dev cdbs gnome-common debian-archive-keyring python3-docutils
      ```

   2. Copy `.pbuilderrc` to `~/.pbuilderrc`

   3. Create chroots for each of the Debian or Ubuntu builds you'll be doing.

   ```
   sudo DIST=trusty ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=trusty ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=jessie ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
   sudo DIST=jessie ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
   ```

   4. Build all the packages with: `python3 build_packages.py deb`

3. On a Fedora machine:
   1. Install dependencies:

      ```
      sudo dnf install devscripts libnautilus-extension-dev mock rpm rpm-sign rpmdevtools rpmlint expect createrepo cdbs gnome-common python3-docutils rpm-build gawk nautilus-devel
      ```

   2. Add yourself to the mock group (create it if it doesn't exist), and
      then logout and login for the group to take affect.
      `build_packages.py` will use `sudo` for a few of the commands, so add both
      your user account and root to the group.

   3. Copy `rpm_resources/fedora-*.cfg` to `/etc/mock`.

      If you need to add new configs when we start targetting a new Fedora
      version, you can find all the configs here:
      https://github.com/rpm-software-management/mock.

      The configs in that repo are templatized, so you'll also have to manually
      copy the template contents into your copy.

   4. Copy `rpm_resources/.rpmmacros` to `~/.rpmmacros`

   5. Build all the packages with: `python3 build_packages.py rpm`

4. Gather the built outputs from both machines, and put them in the repo root
   (this step can be done on either machine). Run `python3 build_packages.py package`.

5. Update the ChangeLog with all the changes in the release.

6. Tag the release and push the tag to GitHub.
