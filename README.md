[![builds.sr.ht status](https://builds.sr.ht/~exec64/imv.svg)](https://builds.sr.ht/~exec64/imv?)
imv - X11/Wayland Image Viewer
==============================

`imv` is a command line image viewer intended for use with tiling window managers.

**The master branch of imv is currently UNSTABLE as it contains the in-progress
work for imv v4**

Features
--------

* Native Wayland and X11 support
* Support for over 30 different image file formats including:
  * Photoshop PSD files
  * Animated GIFS
  * Various RAW formats
  * SVG
* Configurable key bindings and behaviour
* Controllable via scripts using `imv-msg`

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

Through custom bindings, imv can be configured to perform almost any action
you like.

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

The `-x` switch can be used to exit imv after the last picture instead of
cycling through the list.

Installation
------------

`imv` depends on `pthreads`, `FontConfig`, `SDL2`, `SDL_TTF` and `asciidoc`.
Additional dependencies are determined by which backends are selected when
building `imv`. You can find a summary of which backends are available and
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

`imv` has a work-in-progress test suite. The test suite requires `cmocka`.

    $ make check

License
-------
`imv`'s source is published under the [MIT](LICENSE) license.
