#!/bin/bash

function help {
	printf "Usage: %s [project] [tag/commit]\n" "$(basename "$0")"
	exit 1
}

if [ $# -ne 2 ]; then
	help
fi

prefix="$1-$2"
filename="${prefix}-src"
filename_tar="${filename}.tar"
filename_tar_gz="${filename_tar}.gz"

if [ -f "${filename_tar}" ] || [ -f "${filename_tar_gz}" ]; then
	printf "Output file already exists!\n"
	exit 1
fi

printf "Output file: %s\n" "${filename_tar_gz}"

git archive --prefix "${prefix}/" -o "${filename_tar}" $2
git submodule foreach --recursive "git archive --prefix=${prefix}/\$sm_path/ --output=\$name-\$sha1.tar \$sha1 && tar --concatenate --file=$(pwd)/${filename_tar} \$name-\$sha1.tar && rm \$name-\$sha1.tar"

gzip "${filename_tar}"
