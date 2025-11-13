# HelloWorld CommonAPI-D-Bus Example
This project demonstrates a simple client-server application using GENIVI CommonAPI(Link) and CommonAPI-DBus(Link) for inter-process communication over D-Bus.

# Features
> Minimal client and service using CommonAPI and D-Bus
> Portable CMake build system
> Supports custom/patched D-Bus installations without hardcoded paths

# Prerequisites
C++11 compiler (e.g., GCC 5+, Clang 3.5+)
CMake ≥ 3.13(Link)
Boost ≥ 1.54 (system, thread, log)(Link)
CommonAPI Core Runtime(Link)
CommonAPI-DBus Runtime(Link)
D-Bus (system or custom/patched)(Link)
pkg-config(Link)

# Installation References
The installation of CommonAPI Core Runtime and CommonAPI-DBus Runtime in this project follows the official instructions provided in their respective GENIVI repositories:

GENIVI CommonAPI Core Runtime README(Link)
GENIVI CommonAPI-DBus Runtime README(Link)
Please refer to those READMEs for detailed, up-to-date installation steps and requirements for each component.
For D-Bus, the approach described in the CommonAPI-DBus README was used, including patching and building a custom D-Bus if required for compatibility.

# Custom/Patched D-Bus Setup
$ wget http://dbus.freedesktop.org/releases/dbus/dbus-<VERSION>.tar.gz
$ tar -xzf dbus-<VERSION>.tar.gz
$ cd dbus-<VERSION>
$ patch -p1 < </path/to/CommonAPI-D-Bus/src/dbus-patches/patch-names>.patch
    example: for patch in capicxx-dbus-runtime/src/dbus-patches/*.patch; do patch -d dbus-1.12.16/ -p1 <"$patch"; done
$ ./configure --prefix=</path to your preferred installation folder for patched libdbus>
    example: ./configure --prefix=$HOME/dbus-patched
$ make -C dbus 
$ make -C dbus install
 make install-pkgconfigDATA

# Set environment variables before building and running
export LD_LIBRARY_PATH=/path/to/install_folder_libdbus_1.12.16/lib:/usr/local/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=/path/to/install_folder_libdbus_1.12.16/lib/pkgconfig:$PKG_CONFIG_PATH

# Building the Project

Clone and generate code (if needed):
git clone <this-repo>
cd <this-repo>
  Generate code with CommonAPI generators if not already present
  Example: <.>/project$ ./cgen/commonapi_core_generator/commonapi-core-generator-linux-x86_64 -d src-gen/core -sk ./fidl/HelloWorld.fidl
<.>/project$ ./cgen/commonapi_dbus_generator/commonapi-dbus-generator-linux-x86_64 -d src-gen/dbus ./fidl/HelloWorld.fidl

## Configure the build:
mkdir build
cd build
cmake ..

## Build
make

## Run the services

./HelloWorldService

./HelloWorldClient

# Project Structure
.
├── fidl/                # Source files (Franca IDL)
├── src/                # Source files (client, service, stub impl)
├── src-gen/            # Generated code (CommonAPI, D-Bus)
├── CMakeLists.txt      # Build configuration
├── cmake/              # Custom CMake modules (e.g., FindDBus1.cmake)
└── README.md           # This file