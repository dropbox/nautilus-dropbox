import sys
import re
import os
import base64

def replace_many(src2dest, buf):
    src_re = re.compile('|'.join(re.escape(word) for word in src2dest))

    def replace_repl(mo):
        return src2dest[mo.group()]
    return src_re.sub(replace_repl, buf)

def get_base64_image_data(filepath):
    # Load image and convert to base64 string
    if not os.path.exists(filepath):
        raise FileNotFoundError(f"Image not found: {filepath}")

    with open(filepath, "rb") as f:
        data = f.read()
        # Encode image binary to base64 string literal
        return base64.b64encode(data).decode('ascii')

if __name__ == '__main__':
    try:
        print(f"Current working directory: {os.getcwd()}", file=sys.stderr)

        icon64_path = "data/icons/hicolor/64x64/apps/dropbox.png"
        icon16_path = "data/icons/hicolor/16x16/apps/dropbox.png"

        image64_data = get_base64_image_data(icon64_path)
        image16_data = get_base64_image_data(icon16_path)

        # Create simple placeholder strings that the app consuming this must decode if needed
        src2dest = {
            '@PACKAGE_VERSION@': sys.argv[1],
            '@DESKTOP_FILE_DIR@': sys.argv[2],
            '@IMAGEDATA64@': repr(image64_data),  # Use repr() to properly escape the string
            '@IMAGEDATA16@': repr(image16_data),  # Use repr() to properly escape the string
        }

        buf = sys.stdin.read()
        sys.stdout.write(replace_many(src2dest, buf))
    except Exception as e:
        print(f"Error in build_dropbox.py: {str(e)}", file=sys.stderr)
        sys.exit(1)