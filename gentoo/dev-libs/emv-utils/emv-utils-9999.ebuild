# Copyright 1999-2026 Gentoo Authors
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

LICENSE="LGPL-2.1+ tools? ( GPL-3+ ) gui? ( GPL-3+ )"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="+mbedtls openssl +pcsc-lite +tools gui doc test"
REQUIRED_USE="
	^^ ( mbedtls openssl )
"
RESTRICT="!test? ( test )"

BDEPEND="
	doc? ( app-text/doxygen )
"

RDEPEND="
	dev-libs/boost[icu]
	app-text/iso-codes
	dev-libs/json-c
	mbedtls? ( net-libs/mbedtls )
	openssl? ( dev-libs/openssl )
	pcsc-lite? ( sys-apps/pcsc-lite )
	gui? ( dev-qt/qtbase:6[gui,widgets] )
"
DEPEND="
	${RDEPEND}
"

src_prepare() {
	cmake_src_prepare

	# Remove dirty suffix because Gentoo modifies CMakeLists.txt
	sed -i -e 's/--dirty//' CMakeLists.txt || die "Failed to remove dirty suffix"
}

src_configure() {
	# NOTE:
	# https://wiki.gentoo.org/wiki/Project:Qt/Policies recommends that Qt5
	# support should be dropped and that USE=gui should be used instead.

	local mycmakeargs=(
		$(cmake_use_find_package mbedtls MbedTLS)
		$(cmake_use_find_package openssl OpenSSL)
		$(cmake_use_find_package pcsc-lite PCSCLite)
		-DBUILD_EMV_DECODE=$(usex tools)
		-DBUILD_EMV_TOOL=$(usex tools $(usex pcsc-lite YES NO) NO)
		-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=YES
		$(cmake_use_find_package gui Qt6)
		-DBUILD_EMV_VIEWER=$(usex gui)
		-DBUILD_DOCS=$(usex doc)
		-DBUILD_TESTING=$(usex test)
	)

	cmake_src_configure
}

src_test() {
	cmake_src_test
}

DOCS=( README.md LICENSE viewer/LICENSE.gpl )
