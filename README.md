# Dropbox Nautilus Extension

This is the Dropbox extension for Nautilus.

Check us out at https://www.dropbox.com/

<3,
The Dropbox Team

## Compatibility

The Dropbox Nautilus package will work on the operating systems it's provided
for. However, the Dropbox desktop app only officially supports Ubuntu 14.04 or
higher, and Fedora 21 or higher. If your device does not meet these requirements,
you are still able to use the Dropbox desktop application. However your results
may vary.

For more details, see: https://www.dropbox.com/help/desktop-web/system-requirements#linux

## Building Dropbox Nautilus Extension From Source Tarball

You will need to install the following dependencies (instructions are Ubuntu-specific):

```
$ sudo apt-get install -y gnome-common libnautilus-extension-dev python-gtk2-dev python-docutils
```

Then run the following to build and install nautilus-dropbox:
```
$ ./autogen.sh
$ make
$ sudo make install
```

After installing the package you must restart Nautilus. You can do that by issuing the following command (note: if you're running compiz, doing so may lock up your computer - log out and log back in instead):

```
$ killall nautilus
```
