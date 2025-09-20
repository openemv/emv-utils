# Copyright 1999-2025 Gentoo Authors
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
IUSE="+mbedtls openssl +pcsc-lite qt5 qt6 doc test"
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
	qt5? (
		dev-qt/qtcore:5
		dev-qt/qtgui:5
		dev-qt/qtwidgets:5
	)
	qt6? (
		dev-qt/qtbase:6[gui,widgets]
	)
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
	# https://wiki.gentoo.org/wiki/Project:Qt/Policies states that when an
	# application optionally supports one of two Qt versions, it is allowed for
	# both qt5 and qt6 to be enabled and, if so, qt5 should be preferred.
	local mycmakeargs=(
		$(cmake_use_find_package mbedtls MbedTLS)
		$(cmake_use_find_package openssl OpenSSL)
		$(cmake_use_find_package pcsc-lite PCSCLite)
		-DBUILD_EMV_TOOL=$(usex pcsc-lite)
		$(cmake_use_find_package qt5 Qt5)
		$(cmake_use_find_package qt6 Qt6)
		-DBUILD_DOCS=$(usex doc)
		-DBUILD_TESTING=$(usex test)
	)
	if use qt5 || use qt6; then
		mycmakeargs+=( -DBUILD_EMV_VIEWER=YES )
	fi

	cmake_src_configure
}

src_test() {
	cmake_src_test
}

DOCS=( README.md LICENSE )
