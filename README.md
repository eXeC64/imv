imv - X11/Wayland Image Viewer
==============================

`imv` is a command line image viewer intended for use with tiling window managers.

Features
--------

* Wayland Support
* Support for over 30 different image file formats including:
  * Photoshop PSD files
  * Animated GIFS
  * Various RAW formats

Usage
-----

### Opening images
    imv image1.png another_image.jpeg yet_another.TIFF

### Opening images via stdin
    find . "*.png" | imv -i

### Autoscale images to fit the window
    imv -s *.gif

### Open an image fullscreen (and scale to fit screen)
    imv -fs image.jpeg

### Sorting images
    find . "*.png" | sort | imv -i

### Shuffling images
    find . "*.png" | shuf | imv -i

Installation
------------

    make
    make install

License
-------
`imv` is published under the [GPLv2](LICENSE) license.
