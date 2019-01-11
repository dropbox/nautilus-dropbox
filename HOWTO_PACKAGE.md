# How to package nautilus-dropbox

This is a work-in-progress guide on how to build Ubuntu, Debian, and
Fedora packages for nautilus-dropbox. It assumes you're building from
a 16.04 Ubuntu machine.

1. Obtain the signing key.

2. Install the build dependencies.

This is a non-exhaustive list of what you'll need:

```
sudo apt-get install pbuilder debootstrap devscripts libnautilus-extension-dev mock rpm expect createrepo
```

3. Copy .pbuilderrc to ~/.pbuilderrc

4. Create chroots for each of the Debian or Ubuntu builds you'll be doing.

```
sudo DIST=trusty ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
sudo DIST=trusty ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
sudo DIST=jessie ARCH=i386 pbuilder create --debootstrapopts --variant=buildd
sudo DIST=jessie ARCH=amd64 pbuilder create --debootstrapopts --variant=buildd
```

5. Add yourself to the mock group (create it if it doesn't exist), and
   then logout and login for the group to take affect.
   Probably you have to run `build_packages.py` command with `sudo` permission which 
   means you should add your root account to the mock group. Otherwise, you will see
   `The password you typed is invalid.` error.

6. Copy rpm_resources/fedora-*.cfg to /etc/mock.

   If you need to add new configs when we start targetting a new Fedora version,
   you can find all the configs here:  https://github.com/rpm-software-management/mock

7. Copy rpm_resources/.rpmmacros to ~/.rpmmacros

8. Build all the packages with: `python build_packages.py`

If the build is successful, a tar of the output will be in: /tmp/nautilus-dropbox-release.tar.gz
