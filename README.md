[![builds.sr.ht status](https://builds.sr.ht/~exec64/imv.svg)](https://builds.sr.ht/~exec64/imv?)
imv - X11/Wayland Image Viewer
==============================

`imv` is a command line image viewer intended for use with tiling window managers.

**The master branch of imv is currently UNSTABLE as it contains the in-progress
work for imv v4**

Features
--------

* Native Wayland and X11 support
* Support for dozens of image formats including:
  * Photoshop PSD files
  * Animated GIFs
  * Various RAW formats
  * SVG
* Configurable key bindings and behaviour
* Highly scriptable with IPC via imv-msg

Example Usage
-------------

The following examples are a quick illustration of how you can use imv.
For full documentation see the man page.

### Opening images
    imv image1.png another_image.jpeg a_directory

### Opening a directory recursively
    imv -r Photos

### Opening images via stdin
    find . "*.png" | imv

### Open an image fullscreen
    imv -f image.jpeg

### Viewing images in a random order
    find . "*.png" | shuf | imv

### Viewing images from stdin
    curl http://somesi.te/img.png | imv -

### Advanced use

imv can be used to select images in a pipeline by using the `p` hotkey to print
the current image's path to stdout. The `-l` flag can also be used to tell imv
to list the remaining paths on exit for a "open set of images, close unwanted
ones with `x`, then quit imv to pass the remaining images through" workflow.

Key bindings can be customised to run arbitrary shell commands. Environment
variables are exported to expose imv's state to scripts run by it. These
scripts can in turn modify imv's behaviour by invoking `imv-msg` with
`$imv_pid`.

For example:

    #!/usr/bin/bash
    imv "$@" &
    imv_pid = $!

    while true; do
      # Some custom logic
      # ...

      # Close all open files
      imv-msg $imv_pid close all
      # Open some new files
      imv-msg $imv_pid open ~/new_path

      # Run another script against the currently open file
      imv-msg $imv_pid exec another-script.sh $imv_current_file
    done


#### Deleting unwanted images
In your imv config:

    [binds]
    <Shift+X> = exec rm "$imv_current_file"; close

Then press 'X' within imv to delete the image and close it.

#### Rotate an image
In your imv config:

    [binds]
    <Shift+R> = exec mogrify -rotate 90 "$imv_current_file"

Then press 'R' within imv to rotate the image 90 degrees using imagemagick.

#### Tag images from imv using dmenu as a prompt
In your imv config:

    [binds]
    u = exec echo $imv_current_file >> ~/tags/$(ls ~/tags | dmenu -p "tag")

Then press 'u' within imv to tag the current image.

#### Viewing images from the web
    curl -Osw '%{filename_effective}\n' 'http://www.example.com/[1-10].jpg' | imv

### Slideshow

imv can be used to display slideshows. You can set the number of seconds to
show each image for with the `-t` option at start up, or you can configure it
at runtime using the `t` and `T` hotkeys to increase and decrease the image
display time, respectively.

To cycle through a folder of pictures, showing each one for 10 seconds:

    imv -t 10 ~/Pictures/London

Installation
------------

### Dependencies

| Library        |  Version |  Notes                                         |
|---------------:|:---------|------------------------------------------------|
| pthreads       |          | Required.                                      |
| xkbcommon      |          | Required.                                      |
| pangocairo     |          | Required.                                      |
| X11            |          | Optional. Required for X11 support.            |
| GLU            |          | Optional. Required for X11 support.            |
| xcb            |          | Optional. Required for X11 support.            |
| xkbcommon-x11  |          | Optional. Required for X11 support.            |
| wayland-client |          | Optional. Required for Wayland support.        |
| wayland-egl    |          | Optional. Required for Wayland support.        |
| FreeImage      |          | Optional. Provides PNG, JPEG, TIFF, GIF, etc.  |
| libtiff        |          | Optional. Provides TIFF support.               |
| libpng         |          | Optional. Provides PNG support.                |
| libjpeg        |          | Optional. Provides JPEG support.               |
| librsvg        | >=v2.44  | Optional. Provides SVG support.                |

Dependencies are determined by which backends and window systems are enabled
when building `imv`. You can find a summary of which backends are available and
control which ones `imv` is built with in [config.mk](config.mk)

    $ $EDITOR config.mk
    $ make
    # make install

Macro `PREFIX` controls installation prefix.  If more control over installation
paths is required, macros `BINPREFIX`, `MANPREFIX` and `DATAPREFIX` are
available.  Eg. to install `imv` to home directory, run:

    $ BINPREFIX=~/bin PREFIX=~/.local make install

In case something goes wrong during installation process you may use verbose
mode to inspect commands issued by make:

    $ V=1 make

Tests
-----

`imv` has an almost non-existent test suite. The test suite requires `cmocka`.

    $ make check

License
-------
`imv`'s source is published under the [MIT](LICENSE) license.
