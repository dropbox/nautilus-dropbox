import sys
import gtk

pixbuf64 = gtk.gdk.pixbuf_new_from_file("data/icons/hicolor/64x64/apps/dropbox.png")
pixbuf16 = gtk.gdk.pixbuf_new_from_file("data/icons/hicolor/16x16/apps/dropbox.png")
sys.stdout.write(sys.stdin.read().replace\
                     ('@IMAGEDATA64@', "gtk.gdk.pixbuf_new_from_data(%r, gtk.gdk.COLORSPACE_RGB, %r, %r, %r, %r, %r)" % \
                          (pixbuf64.get_pixels(), pixbuf64.get_has_alpha(), pixbuf64.get_bits_per_sample(),
                           pixbuf64.get_width(), pixbuf64.get_height(), pixbuf64.get_rowstride()))\
                     .replace('@IMAGEDATA16@', "gtk.gdk.pixbuf_new_from_data(%r, gtk.gdk.COLORSPACE_RGB, %r, %r, %r, %r, %r)" % \
                                  (pixbuf16.get_pixels(), pixbuf16.get_has_alpha(), pixbuf16.get_bits_per_sample(),
                                   pixbuf16.get_width(), pixbuf16.get_height(), pixbuf16.get_rowstride())))

