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

## Building Dropbox Nautilus Extension

NOTE: It is strongly recommended that you download a pre-compiled
Dropbox Nautilus package from [the Dropbox
website](https://www.dropbox.com/install-linux). You should only build
Dropbox Nautilus yourself if you need a package for a distribution
that we don't support, or you want to develop on the Dropbox Nautilus
package.

### Dependencies

You will need to install the following dependencies:

#### Ubuntu

```
$ sudo apt-get install -y gnome-common libgtk-4-dev libnautilus-extension-dev python3-gi python3-docutils
```

If you're running on an ARM machine, you'll also have to run the following:

```
$ sudo apt-get install -y box64
```

> [!CAUTION]
> Note that ARM support is **_NO EFFORT ONLY_**. The tray icon doesn't work, we don't produce first-party packages for it, and other things might break in the future. This is just to make it a bit easier to install on SBCs or use in VMs hosted on Apple Silicon machines.
>
> IF YOU FILE AN ISSUE FOR THE ARM VERSION WE WILL CLOSE IT!

#### Fedora

```
$ sudo dnf install -y gnome-common nautilus-devel gtk4-devel python3-docutils
```

Due to difficulties with programmatically managing multiarch library installations in Fedora without breaking nonstandard setups, we don't even pretend to have an ARM version.

### Building and installing

Then run the following to build and install nautilus-dropbox:

```
$ ./autogen.sh
$ make
$ sudo make install
```

This creates a "binary" named `dropbox` in the repo.

### Running

To start out the installer GUI, make sure the Dropbox desktop client
isn't currently installed and run:

```
$ ./dropbox start -i
```

After installing the package you may run into issues unless you
restart Nautilus. You can do that by issuing the following command
(note: if you're running compiz, doing so may lock up your computer -
log out and log back in instead):

```
$ killall nautilus
```

## Creating a `.deb` or `.rpm` package

See [HOWTO_PACKAGE.md](HOWTO_PACKAGE.md)
