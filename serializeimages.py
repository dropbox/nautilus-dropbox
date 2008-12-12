import sys

import gtk

start = "#### SERIALIZED IMAGES ####\n"
end = "#### END SERIALIZED IMAGES ####\n"

if __name__ == "__main__":
    f = open("dropbox", "r")
    script = f.readlines()
    f.close()

    try:
        starti, endi = script.index(start), script.index(end)
    except:
        print >>sys.stderr, "Malformed dropbox script"
        sys.exit(-1)

    pixbuf = gtk.gdk.pixbuf_new_from_file("48x48.png")

    f = open("dropbox", "w")
    for line in script[:starti]:
        f.write(line)
    f.write(start)
    f.write("def load_serialized_images():\n")
    f.write("    import gtk\n")
    f.write("    box_logo_pixbuf = gtk.gdk.pixbuf_new_from_data(\n")
    f.write("        %r,\n" % pixbuf.get_pixels())
    f.write("        gtk.gdk.COLORSPACE_RGB,\n")
    f.write("        %r,\n" % pixbuf.get_has_alpha())
    f.write("        %r,\n" % pixbuf.get_bits_per_sample())
    f.write("        %r,\n" % pixbuf.get_width())
    f.write("        %r,\n" % pixbuf.get_height())
    f.write("        %r)\n" % pixbuf.get_rowstride())
    f.write("    globals().update(locals())\n\n")
    for line in script[endi:]:
        f.write(line)
    f.close()
