# HelloWorld CommonAPI-SomeIP Example
This project demonstrates a simple client-server application using GENIVI CommonAPI(Link) and CommonAPI-SomeIP(Link) for inter-process communication over SomeIP.

# Features
> Minimal client and service using CommonAPI and SomeIP
> Portable CMake build system

# Prerequisites
C++11 compiler (e.g., GCC 5+, Clang 3.5+)
CMake ≥ 3.13(Link)
Boost ≥ 1.54 (system, thread, log)(Link)
CommonAPI Core Runtime(Link)
CommonAPI-SomeIP Runtime(Link)
pkg-config(Link)

# Installation References
The installation of CommonAPI Core Runtime and CommonAPI-SomeIP Runtime in this project follows the official instructions provided in their respective GENIVI repositories:

GENIVI CommonAPI Core Runtime README(Link)
GENIVI CommonAPI-SomeIP Runtime README(Link)
Please refer to those READMEs for detailed, up-to-date installation steps and requirements for each component.

# Set environment variables before building and running
export LD_LIBRARY_PATH=$(pwd)/build:$LD_LIBRARY_PATH

# Building the Project

Clone and generate code (if needed):
git clone <this-repo>
cd <this-repo>
  Generate code with CommonAPI generators if not already present
  example: <.>/project$ ./cgen/commonapi_core_generator/commonapi-core-generator-linux-x86_64 -d src-gen/core -sk ./fidl/HelloWorld.fidl
<.>/project$ ./cgen/commonapi_someip_generator/commonapi-someip-generator-linux-x86_64 -d src-gen/someip ./fidl/HelloWorld.fdepl

## Configure the build:
mkdir build
cd build
cmake ..

## Build
make

## Run the services
### Runing with default vsomeip configuration

 export COMMONAPI_CONFIG=commonapi4someip.ini
./build/WorldService_v0

 export COMMONAPI_CONFIG=commonapi4someip.ini
./build/WorldClient_v0

### Runing with custome vsomeip configuration

export COMMONAPI_CONFIG=commonapi4someip.ini 
export VSOMEIP_CONFIGURATION=config/vsomeip-local.json
./build/WorldService_v0

 export COMMONAPI_CONFIG=commonapi4someip.ini 
 export VSOMEIP_CONFIGURATION=config/vsomeip-local.json
./build/WorldClient_v0

### Runing with custome vsomeip security configuration

export COMMONAPI_CONFIG=config/commonapi4someip.ini 
export VSOMEIP_CONFIGURATION=config/vsomeip-local-security.json
./build/WorldService_v1

 export COMMONAPI_CONFIG=config/commonapi4someip.ini 
 export VSOMEIP_CONFIGURATION=config/vsomeip-local-security.json
./build/WorldClient_v1

### Runing with custome vsomeip security configuration  and events and fields

export COMMONAPI_CONFIG=config/commonapi4someip.ini
export VSOMEIP_CONFIGURATION=config/vsomeip-local-security.json
 ./build/WorldService_v1
 
export COMMONAPI_CONFIG=config/commonapi4someip.ini 
export VSOMEIP_CONFIGURATION=config/vsomeip-local-security.json
 ./build/WorldClient_v1
 
# Project Structure
.
├── fidl/                # Source files (Franca IDL)
├── src/                # Source files (client, service, stub impl)
├── src-gen/            # Generated code (CommonAPI, SomeIP)
├── CMakeLists.txt      # Build configuration
├── commonapi4someip.ini/  # Custom configuration file
└── README.md           # This file