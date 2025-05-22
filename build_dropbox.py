import sys
import os
import gi

# Set GDK_BACKEND to offscreen before importing GTK
os.environ['GDK_BACKEND'] = 'offscreen'

gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, Gdk

import re

def replace_many(src2dest, buf):
    src_re = re.compile('|'.join(re.escape(word) for word in src2dest))

    def replace_repl(mo):
        return src2dest[mo.group()]
    return src_re.sub(replace_repl, buf)

if __name__ == '__main__':
    # Initialize GTK
    Gtk.init()
    
    # Load images using GTK
    pixbuf64 = Gdk.Texture.new_from_filename("data/icons/hicolor/64x64/apps/dropbox.png")
    pixbuf16 = Gdk.Texture.new_from_filename("data/icons/hicolor/16x16/apps/dropbox.png")
    
    src2dest = {'@PACKAGE_VERSION@': sys.argv[1],
                '@DESKTOP_FILE_DIR@': sys.argv[2],
                '@IMAGEDATA64@': ("Gdk.Texture.new_from_pixels(%r, %r, %r, %r)" %
                                  (pixbuf64.get_pixels(), pixbuf64.get_width(), pixbuf64.get_height(), pixbuf64.get_format())),
                '@IMAGEDATA16@': ("Gdk.Texture.new_from_pixels(%r, %r, %r, %r)" %
                                  (pixbuf16.get_pixels(), pixbuf16.get_width(), pixbuf16.get_height(), pixbuf16.get_format())),
                }

    buf = sys.stdin.read()
    sys.stdout.write(replace_many(src2dest, buf))
