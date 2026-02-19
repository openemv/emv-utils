Libraries and tools for EMV :registered: card data
==================================================

[![License: LGPL-2.1](https://img.shields.io/github/license/openemv/emv-utils)](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)<br/>
[![Ubuntu build](https://github.com/openemv/emv-utils/actions/workflows/ubuntu-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/ubuntu-build.yaml)<br/>
[![Fedora build](https://github.com/openemv/emv-utils/actions/workflows/fedora-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/fedora-build.yaml)<br/>
[![MacOS build](https://github.com/openemv/emv-utils/actions/workflows/macos-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/macos-build.yaml)<br/>
[![Windows build](https://github.com/openemv/emv-utils/actions/workflows/windows-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/windows-build.yaml)<br/>

> [!NOTE]
> EMV :registered: is a registered trademark in the U.S. and other countries
> and an unregistered trademark elsewhere. The EMV trademark is owned by
> EMVCo, LLC. This project refers to "EMV" only to indicate the specifications
> involved and does not imply any affiliation, endorsement or sponsorship by
> EMVCo in any way.

This project is a partial implementation of the EMVCo specifications for card
payment terminals. It is mostly intended for validation and debugging purposes
but may eventually grow into a full set of EMV kernels.

If you wish to use these libraries for a project that is not compatible with
the terms of the LGPL v2.1 license, please contact the author for alternative
licensing options.

Example output
--------------
![Example of emv-decoder usage](/emv-decoder-example.png)
![Example of emv-viewer usage](/emv-viewer-example.png)
See [usage](#usage) for more examples.

Installation
------------

* For Ubuntu 20.04 LTS (Focal), 22.04 LTS (Jammy), or 24.04 LTS (Noble) install
  the appropriate
  [release package](https://github.com/openemv/emv-utils/releases)
* For Fedora 41 or Fedora 42, install the appropriate Fedora
  [release package](https://github.com/openemv/emv-utils/releases)
* For Gentoo, use the
  [OpenEMV overlay](https://github.com/openemv/openemv-overlay), set the
  keywords and useflags as needed, and install using
  `emerge --verbose --ask emv-utils`
* For MacOS with [Homebrew](https://brew.sh/), use the
  [OpenEMV tap](https://github.com/openemv/homebrew-tap) and install using
  `brew install openemv/tap/emv-utils`
* For Windows, use the
  [installer](https://github.com/openemv/emv-utils/releases) or follow the
  build instructions below to build using [MSYS2](https://www.msys2.org/)
* For other platforms, architectures or configurations, follow the build
  instructions below

Dependencies
------------

* C11 and C++11 compilers such as GCC or Clang
* [CMake](https://cmake.org/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [iso-codes](https://salsa.debian.org/iso-codes-team/iso-codes)
* [json-c](https://github.com/json-c/json-c)
* [Boost.Locale](https://github.com/boostorg/locale) will be used by default
  for ISO 8859 support but is optional if a different implementation is
  selected; see [ISO/IEC 8859 support](#isoiec-8859-support).
* [iconv](https://www.gnu.org/software/libiconv/) can _optionally_ be selected
  for ISO 8859 support; see [ISO/IEC 8859 support](#isoiec-8859-support).
* `emv-decode` and `emv-tool` will be built by default and require `argp`
  (either via Glibc, a system-provided standalone, or downloaded during the
  build from [libargp](https://github.com/leonlynch/libargp); see
  [MacOS / Windows](#macos--windows)). Use the `BUILD_EMV_DECODE` and
  `BUILD_EMV_TOOL` options to prevent `emv-decode` and  `emv-tool` from being
  built and avoid the dependency on `argp`.
* `emv-tool` requires PC/SC, either provided by `WinSCard` on Windows, by
  PCSC.framework on MacOS, or by [PCSCLite](https://pcsclite.apdu.fr/) on
  Linux. Use the `BUILD_EMV_TOOL` option to prevent `emv-tool` from being built
  and avoid the dependency on PC/SC.
* `emv-viewer` can _optionally_ be built if [Qt](https://www.qt.io/) (see
  [Qt](#qt) for details) is available at build-time. If it is not available,
  `emv-viewer` will not be built. Use the `BUILD_EMV_VIEWER` option to ensure
  that `emv-viewer` will be built.
* [Doxygen](https://github.com/doxygen/doxygen) can _optionally_ be used to
  generate API documentation if it is available; see
  [Documentation](#documentation)
* [bash-completion](https://github.com/scop/bash-completion) can _optionally_
  be used to generate bash completion for `emv-decode` and `emv-tool`
* [NSIS](https://nsis.sourceforge.io) can _optionally_ be used to generate a
  Windows installer for this project

This project also makes use of sub-projects that must be provided as git
submodules using `git clone --recurse-submodules`:
* [OpenEMV common crypto abstraction](https://github.com/openemv/crypto)
* [mcc-codes](https://github.com/greggles/mcc-codes)

Build
-----

This project uses CMake and can be built using the usual CMake steps.

To generate the build system in the `build` directory, use:
```shell
cmake -B build
```

To build the project, use:
```shell
cmake --build build
```

Consult the CMake documentation regarding additional options that can be
specified in the above steps.

Testing
-------

The tests can be run using the `test` target of the generated build system.

To run the tests using CMake, do:
```shell
cmake --build build --target test
```

Alternatively, [ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
can be used directly which also allows actions such as `MemCheck` to be
performed or the number of jobs to be set, for example:
```shell
ctest --test-dir build -T MemCheck -j 10
```

Documentation
-------------

If Doxygen was found by CMake, then HTML documentation can be generated using
the `docs` target of the generated build system.

To generate the documentation using CMake, do:
```shell
cmake --build build --target docs
```

Alternatively, the `BUILD_DOCS` option can be specified when generating the
build system by adding `-DBUILD_DOCS=YES`.

Packaging
---------

If the required packaging tools were found (`dpkg` and/or `rpmbuild` on Linux)
by CMake, packages can be created using the `package` target of the generated
build system.

To generate the packages using CMake, do:
```shell
cmake --build build --target package
```

Alternatively, [cpack](https://cmake.org/cmake/help/latest/manual/cpack.1.html)
can be used directly from within the build directory (`build` in the above
[Build](#build) steps).

This is an example of how monolithic release packages can be built from
scratch on Ubuntu or Fedora:
```shell
rm -Rf build &&
cmake -B build -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=YES -DBUILD_DOCS=YES -DCPACK_COMPONENTS_GROUPING=ALL_COMPONENTS_IN_ONE &&
cmake --build build &&
cmake --build build --target package
```

ISO/IEC 8859 support
--------------------

This project contains multiple ISO 8859 implementations that can be selected
at build time using the CMake `ISO8859_IMPL` option. It allows these values:
* `boost` (default): Uses [Boost.Locale](https://github.com/boostorg/locale),
  is supported on most platforms, is forgiving of unassigned code points, but
  requires C++.
* `iconv`: Uses [iconv](https://www.gnu.org/software/libiconv/), is not
  supported on some platforms, is less forgiving of unassigned code points, but
  doesn't require C++.
* `simple`: Only supports ISO 8859-1, has no dependencies and doesn't require
  C++.

Qt
--

This project supports Qt 5.12.x, Qt 5.15.x, Qt 6.5.x and Qt 6.8.x (although it
may be possible to use other versions of Qt) when building the `emv-viewer`
application. However, on some platforms it may be necessary to use the `QT_DIR`
option (and not the `Qt5_DIR` nor `Qt6_DIR` options) or `CMAKE_PREFIX_PATH`
option to specify the exact Qt installation to be used. For Qt6 it may also be
necessary for the Qt tools to be available in the executable PATH regardless of
the `QT_DIR` option.

If the Qt installation does not provide universal binaries for MacOS, it will
not be possible to build `emv-viewer` as a universal binary for MacOS.

MacOS / Windows
---------------

On platforms such as MacOS or Windows where static linking is desirable and
dependencies such as `argp` may be unavailable, the `FETCH_ARGP` option can be
specified when generating the build system.

In addition, MacOS universal binaries can be built by specifying the desired
architectures using the `CMAKE_OSX_ARCHITECTURES` option.

This is an example of how a self-contained, static, universal binary can be
built from scratch for MacOS:
```shell
rm -Rf build &&
cmake -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DFETCH_ARGP=YES &&
cmake --build build
```

On MacOS, a bundle can also be built using the `BUILD_MACOSX_BUNDLE` option and
packaged as a DMG installer. Assuming `QT_DIR` is already appropriately set,
this is an example of how a self-contained, static, native bundle and installer
can be built from scratch for MacOS:
```shell
rm -Rf build &&
cmake -B build -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DFETCH_ARGP=YES -DBUILD_EMV_VIEWER=YES -DBUILD_MACOSX_BUNDLE=YES &&
cmake --build build --target package
```

On Windows, a standalone installation that includes external dependencies can
also be built using the `BUILD_WIN_STANDALONE` option and packaged using NSIS.
Assuming `QT_DIR` is already appropriately set to a Qt installation that can
deploy its own dependencies, this is an example of how a self-contained
installer can be built for Windows:
```shell
rm -Rf build &&
cmake -B build -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DBUILD_SHARED_LIBS=YES -DFETCH_ARGP=YES -DBUILD_EMV_VIEWER=YES -DBUILD_WIN_STANDALONE=YES &&
cmake --build build --target package
```

Usage
-----

### emv-decode

The available command line options of the `emv-decode` application can be
displayed using:
```shell
emv-decode --help
```

To decode ISO 7816-3 Answer-To-Reset (ATR) data, use the `--atr` option. For
example:
```shell
emv-decode --atr 3BDA18FF81B1FE751F030031F573C78A40009000B0
```

To decode EMV TLV data, use the `--tlv` option. For example:
```shell
emv-decode --tlv 701A9F390105571040123456789095D2512201197339300F82025900
```

Alternatively, binary (not ASCII-HEX) EMV TLV data can be read from stdin by
specifying `--tlv -`. For example:
```shell
echo "701A9F390105571040123456789095D2512201197339300F82025900" | xxd -r -p | emv-decoder --tlv -
```

To decode an EMV Data Object List (DOL), use the `--dol` option. For example:
```shell
emv-decode --dol 9F1A029F33039F4005
```

To decode an ISO 3166-1 country code, use the `--country` option. For example:
```shell
emv-decode --country 528
```

To decode an ISO 4217 currency code, use the `--currency` option. For example:
```shell
emv-decode --currency 978
```

To decode an ISO 639 language code, use the `--language` option. For example:
```shell
emv-decode --language fr
```

To decode an ISO 18245 Merchant Category Code (MCC), use the `--mcc` option.
For example:
```shell
emv-decode --mcc 5999
```

To decode ISO 8859 data and print as UTF-8, use the `--iso8859-x` option and
replace `x` with the desired ISO 8859 code page. For example:
```shell
emv-decode --iso8859-10 A1A2A3A4A5A6A7
```

The `emv-decode` application can also decode various other EMV structures and
fields. Use the `--help` option to display all available options.

### emv-viewer

The `emv-viewer` application can be launched via the desktop environment or it
can be launched via the command line. The `emv-viewer` application does not
support the decoding of individual fields like the `emv-decode` application
does, but does allow the decoding of EMV TLV data in the same manner as the
`emv-decode` application.

The available command line options of the `emv-viewer` application can be
displayed using:
```shell
emv-viewer --help
```

To decode EMV TLV data, use the `--tlv` option. For example:
```shell
emv-viewer --tlv 701A9F390105571040123456789095D2512201197339300F82025900
```

Alternatively, binary (not ASCII-HEX) EMV TLV data can be read from stdin by
specifying `--tlv -`. For example:
```shell
echo "701A9F390105571040123456789095D2512201197339300F82025900" | xxd -r -p | emv-viewer --tlv -
```

Roadmap
-------
* Document `emv-tool` usage
* Implement high level EMV processing API
* Implement country, currency, language and MCC searching
* Implement Qt plugin for EMV decoding

License
-------

Copyright 2021-2025 [Leon Lynch](https://github.com/leonlynch).

This project is licensed under the terms of the LGPL v2.1 license with the
exception of `emv-decode`, `emv-tool` and `emv-viewer` which are licensed under
the terms of the GPL v3 license.
See [LICENSE](https://github.com/openemv/emv-utils/blob/master/LICENSE) and
[LICENSE.gpl](https://github.com/openemv/emv-utils/blob/master/viewer/LICENSE.gpl)
files.

This project includes [crypto](https://github.com/openemv/crypto) as a git
submodule and it is licensed under the terms of the MIT license. See
[LICENSE](https://github.com/openemv/crypto/blob/master/LICENSE) file.

This project includes [mcc-codes](https://github.com/greggles/mcc-codes) as a
git submodule and it is licensed under the terms of The Unlicense license. See
[LICENSE](https://github.com/greggles/mcc-codes/blob/main/LICENSE.txt) file.

This project may download [libargp](https://github.com/leonlynch/libargp)
during the build and it is licensed under the terms of the LGPL v3 license. See
[LICENSE](https://github.com/leonlynch/libargp/blob/master/LICENSE) file.

> [!NOTE]
> EMV :registered: is a registered trademark in the U.S. and other countries
> and an unregistered trademark elsewhere. The EMV trademark is owned by
> EMVCo, LLC. This project refers to "EMV" only to indicate the specifications
> involved and does not imply any affiliation, endorsement or sponsorship by
> EMVCo in any way.
