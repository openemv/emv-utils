#!/bin/bash

# Let script show exact commands
set -x

# NOTE: this script depends on Imagemagick. Given that Imagemagick may
# delegate processing of SVGs to various different backends that may have
# different behaviour regarding the alpha channel, it's best to use a PNG icon
# with the correct alpha channel as the input icon.
#
# NOTE: this script also depends on png2icns (see
# https://sourceforge.net/projects/icns/)

function help {
	printf "Usage: %s [icons-dir] [input-icon]\n" "$(basename "$0")"
	exit 1
}

if [ $# -ne 2 ]; then
	help
fi

icons_dir="$1"
input_icon="$2"

if [ ! -f "${input_icon}" ]; then
	printf "${input_icon} does not exist!\n"
	exit 1
fi

# Trim and center the input icon before creating the various resized outputs
# NOTE: 57px offset ensures that the card image is in the center
magick "${input_icon}" -trim -resize 1024x1024 -background none -gravity center -extent 1024x1024+0-57 "${icons_dir}/openemv_emv_utils_1024x1024.png"

# Output various icon sizes for use in the menu, app window, taskbar and installer
magick "${icons_dir}/openemv_emv_utils_1024x1024.png" -resize 512x512 -background none -gravity center -extent 512x512 "${icons_dir}/openemv_emv_utils_512x512.png"
magick "${icons_dir}/openemv_emv_utils_1024x1024.png" -resize 256x256 -background none -gravity center -extent 256x256 "${icons_dir}/openemv_emv_utils_256x256.png"
magick convert "${icons_dir}/openemv_emv_utils_256x256.png" "${icons_dir}/openemv_emv_utils.ico"

# Output icon for MacOS
png2icns "${icons_dir}/openemv_emv_utils.icns" "${icons_dir}/openemv_emv_utils_1024x1024.png"

# Trim and relocate the input icon for Windows NSIS installer (150x57 geometry)
magick "${input_icon}" -trim -resize 150x53 -background white -gravity center -extent 150x53 - | magick - -resize 150x57 -background white -gravity south -extent 150x57 "${icons_dir}/openemv_emv_utils_150x57.png"
magick convert "${icons_dir}/openemv_emv_utils_150x57.png" "BMP3:${icons_dir}/openemv_emv_utils.bmp"
