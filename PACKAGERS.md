# Dear Packager,

This document is a quick summary of all you need to know to package imv for
your favourite operating system.

## 1. Select window systems to support

Your options here are Wayland or X11, or both. By default both are included,
with a separate binary for each being built. `/usr/bin/imv` will be a script
that checks for a Wayland compositor before running the appropriate binary.

If you only care about one of these, you can specify to build only one of these
in [config.mk](config.mk), in which case only that binary shall be packaged
without the need for a launcher script to select between the two.

Alternatively, you could provide separate packages for X11 and Wayland that
act as alternatives to each other.

## 2. Select backends to include

imv supports multiple "backends" in a plugin style architecture. Each backend
provides support for different image formats using different underlying
libraries. The ones that are right for your operating system depend on which
formats you need support for, and your licensing requirements.

imv is published under the MIT license, but its backends may have different
licensing requirements.

You can configure the backends to use in [config.mk](config.mk). Sensible
defaults are pre-configured to provide maximum coverage with the least overlap
and fewest dependencies.

## 3. $ make && make install

Once your backends have been configured and you've confirmed the library
each backend uses is installed, you can simply follow the Installation section
of the [README](README.md) to build imv.

## 4. Package

Package the resulting binary and man pages in your operating system's native
package format.
