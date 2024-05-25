# Copyright 1999-2024 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit cmake

DESCRIPTION="EMV libraries and tools"
HOMEPAGE="https://github.com/openemv/emv-utils"
if [[ "${PV}" == *9999 ]] ; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/openemv/emv-utils.git"
	EGIT_BRANCH="master"
else
	SRC_URI="https://github.com/openemv/emv-utils/releases/download/${PV}/${P}-src.tar.gz -> ${P}.tar.gz"
fi

LICENSE="LGPL-2.1"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="+pcsc-lite doc test"
RESTRICT="!test? ( test )"

BDEPEND="
	doc? ( app-text/doxygen )
"

RDEPEND="
	dev-libs/boost[icu]
	app-text/iso-codes
	dev-libs/json-c
	pcsc-lite? ( sys-apps/pcsc-lite )
"
DEPEND="
	${RDEPEND}
"

src_prepare() {
	cmake_src_prepare
}

src_configure() {
	local mycmakeargs=(
		$(cmake_use_find_package pcsc-lite PCSCLite)
		-DBUILD_EMV_TOOL=$(usex pcsc-lite)
		-DBUILD_DOCS=$(usex doc)
		-DBUILD_TESTING=$(usex test)
	)

	cmake_src_configure
}

src_test() {
	cmake_src_test
}

DOCS=( README.md LICENSE )
