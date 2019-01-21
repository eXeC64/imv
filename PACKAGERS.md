# Dear Packager,

This document is a quick summary of all you need to know to package imv for
your favourite operating system.

## 1. Select backends

imv supports multiple "backends" in a plugin style architecture. Each backend
provides support for different image formats using different underlying
libraries. The ones that are right for your operating system depend on which
formats you need support for, and your licensing requirements.

imv is published under the MIT license, but its backends may have different
licensing requirements.

You can configure the backends to use in [config.mk](config.mk). Sensible
defaults are pre-configured to provide maximum coverage with the least overlap
and fewest dependencies.

## 2. $ make && make install

Once your backends have been configured and you've confirmed the library
each backend uses is installed, you can simply follow the Installation section
of the [README](README.md) to build imv.

## 3. Package

Package the resulting binary and man pages in your operating system's native
package format.
