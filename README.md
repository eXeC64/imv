imv - Image Viewer
==================

`imv` is a command line image viewer intended for use with tiling window managers.

Currently `imv` is in **alpha**. Command line flags and shortcuts may change
without notice.

Features
--------

###Wayland support

`imv` supports Wayland out of the box.

###Animated gif support

`imv` will display animated gifs in all their silent movie era glory

###Support for many file formats

 * BMP
 * Dr. Halo CUT
 * DDS
 * EXR
 * Raw
 * GIF
 * HDR
 * ICO
 * IFF
 * JBIG
 * JNG
 * JPEG/JIF
 * JPEG-2000
 * JPEG-XR
 * KOALA
 * Kodak PhotoCD
 * MNG
 * PCX
 * PBM/PGM/PPM
 * PFM
 * PNG
 * Macintosh PICT
 * Photoshop PSD
 * RAW
 * Sun RAS
 * SGI
 * TARGA
 * TIFF
 * WBMP
 * WebP
 * XBM
 * XPM

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

License
-------
`imv` is published under the [MIT](LICENSE) license.
