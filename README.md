EMV libraries and tools
=======================

[![License: LGPL-2.1](https://img.shields.io/github/license/openemv/emv-utils)](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)<br/>
[![Ubuntu build](https://github.com/openemv/emv-utils/actions/workflows/ubuntu-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/ubuntu-build.yaml)<br/>
[![Fedora build](https://github.com/openemv/emv-utils/actions/workflows/fedora-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/fedora-build.yaml)<br/>
[![MacOS build](https://github.com/openemv/emv-utils/actions/workflows/macos-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/macos-build.yaml)<br/>
[![Windows build](https://github.com/openemv/emv-utils/actions/workflows/windows-build.yaml/badge.svg)](https://github.com/openemv/emv-utils/actions/workflows/windows-build.yaml)<br/>

This project is a partial implementation of the EMVCo specifications for card
payment terminals. It is mostly intended for validation and debugging purposes
but may eventually grow into a full set of EMV kernels.

If you wish to use these libraries for a project that is not compatible with
the terms of the LGPL v2.1 license, please contact the author for alternative
licensing options.

Dependencies
------------

* C11 and C++11 compilers such as GCC or Clang
* [CMake](https://cmake.org/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* [iso-codes](https://salsa.debian.org/iso-codes-team/iso-codes)
* [json-c](https://github.com/json-c/json-c)
* `emv-decode` and `emv-tool` will be built by default and require `argp`
  (either via Glibc, a system-provided standalone or a downloaded
  implementation; see [MacOS / Windows](#macos--windows)). Use the
  `BUILD_EMV_DECODE` and `BUILD_EMV_TOOL` options to prevent `emv-decode` and
  `emv-tool` from being built and avoid the dependency on `argp`.
* `emv-tool` requires PC/SC, either provided by `WinSCard` on Windows or by
  [PCSCLite](https://pcsclite.apdu.fr/) on Linux/MacOS. Use the
  `BUILD_EMV_TOOL` option to prevent `emv-tool` from being built and avoid the
  dependency on PC/SC.
* [Doxygen](https://github.com/doxygen/doxygen) can _optionally_ be used to
  generate API documentation if it is available; see
  [Documentation](#documentation)

Build
-----

This project uses CMake and can be built using the usual CMake steps.

To generate the build system in the `build` directory, use:
```
cmake -B build
```

To build the project, use:
```
cmake --build build
```

Consult the CMake documentation regarding additional options that can be
specified in the above steps.

Testing
-------

The tests can be run using the `test` target of the generated build system.

To run the tests using CMake, do:
```
cmake --build build --target test
```

Alternatively, [ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
can be used directly from within the build directory (`build` in the above
[Build](#build) steps) which also allows actions such as `MemCheck` to be
performed or the number of jobs to be set, for example:
```
ctest -T MemCheck -j 10
```

Documentation
-------------

If Doxygen was found by CMake, then HTML documentation can be generated using
the `docs` target of the generated build system.

To generate the documentation using CMake, do:
```
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
```
cmake --build build --target package
```

Alternatively, [cpack](https://cmake.org/cmake/help/latest/manual/cpack.1.html)
can be used directly from within the build directory (`build` in the above
[Build](#build) steps).

This is an example of how monolithic release packages can be built from
scratch on Ubuntu or Fedora:
```
rm -Rf build &&
cmake -B build -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=YES -DBUILD_DOCS=YES -DCPACK_COMPONENTS_GROUPING=ALL_COMPONENTS_IN_ONE &&
cmake --build build &&
cmake --build build --target package
```

MacOS / Windows
---------------

On platforms such as MacOS or Windows where static linking is desirable and
dependencies such as `argp` may be unavailable, the `FETCH_ARGP` option can be
specified when generating the build system.

In addition, MacOS universal binaries can be built by specifying the desired
architectures using the `CMAKE_OSX_ARCHITECTURES` option.

This is an example of how a self-contained, static, universal binary can be
built from scratch for MacOS:
```
rm -Rf build &&
cmake -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DFETCH_ARGP=YES &&
cmake --build build
```

Usage
-----

The available command line options of the `emv-decode` application can be
displayed using:
```
emv-decode --help
```

To decode ISO 7816-3 Answer-To-Reset (ATR) data, use the `--atr` option. For
example:
```
emv-decode --atr 3BDA18FF81B1FE751F030031F573C78A40009000B0
```

To decode EMV TLV data, use the `--tlv` option. For example:
```
emv-decode --tlv 701A9F390105571040123456789095D2512201197339300F82025900
```

To decode an ISO 3166-1 country code, use the `--country` option. For example:
```
emv-decode --country 528
```

To decode an ISO 4217 currency code, use the `--currency` option. For example:
```
emv-decode --currency 978
```

To decode an ISO 639 language code, use the `--language` option. For example:
```
emv-decode --language fr
```

The `emv-decode` application can also decode various other EMV structures and
fields. Use the `--help` option to display all available options.

Roadmap
-------
* Document `emv-tool` usage
* Implement high level EMV processing API
* Implement ISO 8859 decoding and UTF-8 conversion
* Implement country, currency, language and MCC searching
* Implement context-specific EMV string decoding, such as ISO 8859 code pages
  for UTF-8 conversion and kernel-specific contactless fields
* Implement Qt plugin for EMV decoding

License
-------

Copyright (c) 2021, 2022, 2023 [Leon Lynch](https://github.com/leonlynch).

This project is licensed under the terms of the LGPL v2.1 license. See LICENSE file.
