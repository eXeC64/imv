# Configure available backends:

# FreeImage http://freeimage.sourceforge.net
# provides: png, jpg, animated gif, raw, psd, bmp, tiff, webp, etc.
# depends: libjpeg, openexr, openjpeg2, libwebp, libraw, jxrlib
# license: FIPL v1.0
BACKEND_FREEIMAGE=yes

# libpng http://www.libpng.org/pub/png/libpng.html
# provides: png
# depends: zlib
# license: libpng license
BACKEND_LIBPNG=no

# libjpeg-turbo https://libjpeg-turbo.org/
# provides: jpeg
# depends: none
# license: modified bsd
BACKEND_LIBJPEG=no

# librsvg https://wiki.gnome.org/Projects/LibRsvg
# provides: svg
# depends: gdk-pixbuf2 pango libcroco
# license: LGPL
BACKEND_LIBRSVG=yes
