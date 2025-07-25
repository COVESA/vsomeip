### vSomeIP

##### Copyright
Copyright (C) 2015-2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

##### License

This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

##### Contributing Guidelines

For comprehensive details on how to contribute effectively to the project, please refer to our [CONTRIBUTING.md](./CONTRIBUTING.md) file.

##### vSomeIP Overview
----------------
The vSomeIP stack implements the http://some-ip.com/ (Scalable service-Oriented MiddlewarE over IP (SOME/IP)) Protocol.
The stack consists out of:

* a shared library for SOME/IP (`libvsomeip3.so`)
* a shared library for SOME/IP's configuration module (`libvsomeip3-cfg.so`)
* a shared library for SOME/IP's service discovery (`libvsomeip3-sd.so`)
* a shared library for SOME/IP's E2E protection module (`libvsomeip3-e2e.so`)

Optional:

* a shared library for compatibility with vsomeip v2 (`libvsomeip.so`)

##### Build Instructions for Linux

###### Dependencies

- A C++17 enabled compiler is needed.
- vSomeIP uses CMake as buildsystem.
- vSomeIP uses Boost >= 1.66.0:

For the tests Google's test framework https://code.google.com/p/googletest/[gtest] is needed.
-- URL: https://googletest.googlecode.com/files/gtest-<version>.zip

To build the documentation doxygen and graphviz are needed:
--`sudo apt-get install doxygen graphviz`

###### Compilation

For compilation call:

```bash
mkdir build
cd build
cmake ..
make
```

To specify a installation directory (like `--prefix=` if you're used to autotools) call cmake like:
```bash
cmake -DCMAKE_INSTALL_PREFIX:PATH=$YOUR_PATH ..
make
make install
```

###### Compilation with predefined unicast and/or diagnosis address
To predefine the unicast address, call cmake like:
```bash
cmake -DUNICAST_ADDRESS=<YOUR IP ADDRESS> ..
```

To predefine the diagnosis address, call cmake like:
```bash
cmake -DDIAGNOSIS_ADDRESS=<YOUR DIAGNOSIS ADDRESS> ..
```
The diagnosis address is a single byte value.

###### Compilation with custom default configuration folder
To change the default configuration folder, call cmake like:
```bash
cmake -DDEFAULT_CONFIGURATION_FOLDER=<DEFAULT CONFIGURATION FOLDER> ..
```
The default configuration folder is `/etc/vsomeip`.

###### Compilation with custom default configuration file
To change the default configuration file, call cmake like:
```bash
cmake -DDEFAULT_CONFIGURATION_FILE=<DEFAULT CONFIGURATION FILE> ..
```
The default configuration file is `/etc/vsomeip.json`.

###### Compilation with signal handling

To compile vSomeIP with signal handling (SIGINT/SIGTERM) enabled, call cmake like:
```bash
cmake -DENABLE_SIGNAL_HANDLING=1 ..
```
In the default setting, the application has to take care of shutting down vSomeIP in case these signals are received.

###### Note on Ubuntu 24.04 Build Issues

If you encounter build issues on Ubuntu 24.04, consider using Ubuntu 22.04 as a temporary fix. This is due to the ongoing transition of the GitHub Actions runner to Ubuntu 24.04, which may cause compatibility issues.

##### Build Instructions for Android

###### Dependencies

- vSomeIP uses Boost >= 1.66. The boost libraries (system, thread and log) must be included in the Android source tree and integrated into the build process with an appropriate Android.bp file.

###### Compilation

In general for building the Android source tree the instructions found on the pages from the Android Open Source Project (AOSP) apply (https://source.android.com/setup/build/requirements).

To integrate the vSomeIP library into the build process, the source code together with the Android.bp file has to be inserted into the Android source tree (by simply copying or by fetching with a custom platform manifest).
When building the Android source tree, the Android.bp file is automatically found and considered by the build system.

In order that the vSomeIP library is also included in the Android image, the library has to be added to the PRODUCT_PACKAGES variable in one of a device/target specific makefile:

```
PRODUCT_PACKAGES += \
    libvsomeip \
    libvsomeip_cfg \
    libvsomeip_sd \
    libvsomeip_e2e \
```

##### Build Instructions for Windows

###### Setup

- Visual Studio Code
- Visual Studio Build Tools with:
    - Desktop development with C++
    - MSVC v143 - VS 2022 C++ x64/x86 build tools
    - Windows 10/11 SDK
    - CMake for Windows
- vSomeIP uses CMake as buildsystem.
- vSomeIP uses Boost >= 1.71.0:
- GIT

For the tests Google's test framework https://code.google.com/p/googletest/[gtest] is needed.
-- URL: https://googletest.googlecode.com/files/gtest-<version>.zip
or
-- git clone https://github.com/google/googletest.git

###### Compilation

For compilation call:

```bash
rmdir /s /q build
cd build
cmake .. -A x64 -DCMAKE_INSTALL_PREFIX:PATH=$YOUR_PATH
cmake --build . --config [Release|Debug]
cmake --build . --config [Release|Debug] --target install
```

For compilation outside vsomeip-lib folder call:

```bash
rmdir /s /q build
cmake -B "buildlib" -DCMAKE_BUILD_TYPE=[Release|Debug] -DCMAKE_INSTALL_PREFIX=$YOUR_PATH -A x64 vsomeip-lib
#vsomeip-lib compilation
cmake --build build --config [Release|Debug] --parallel 16 --target install
#examples compilation
cmake --build build --config [Release|Debug] --parallel 16 --parallel 16 --target examples
cmake --build build --config [Release|Debug] --parallel 16 --target install
#unit-tests compilation
cmake --build build/test --config [Release|Debug] --parallel 16 --parallel 16 --target build_unit_tests
#all tests compilation
cmake --build build/test --config [Release|Debug] --parallel 16 --parallel 16 --target all build_tests
```