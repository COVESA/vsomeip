#### Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
#### This Source Code Form is subject to the terms of the Mozilla Public
#### License, v. 2.0. If a copy of the MPL was not distributed with this
#### file, You can obtain one at http://mozilla.org/MPL/2.0/.

# vSomeIP Examples

To use the example applications you need two devices on the same network. The network addresses within
the configuration files need to be adapted to match the devices addresses.

## Linux

### Request/Response Example

To start the request/response-example from the build-directory do:
```bash
# HOST1:
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=client-sample ./request-sample
# HOST1:
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=service-sample ./response-sample
```

To start the subscribe/notify-example from the build-directory do:
```bash
# HOST1: 
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=client-sample ./subscribe-sample
# HOST1:
env VSOMEIP_CONFIGURATION=../../config/vsomeip-local.json VSOMEIP_APPLICATION_NAME=service-sample ./notify-sample
```

## Windows

Note: You may need to define the path of the DLLs in each terminal to run the examples
```bash
set "PATH=<root directory of vSomeIP-Lib build>\Release;<root directory of vSomeIP-Lib build>\test\common;<root directory of boost instalation>\lib64-msvc-14.2;%PATH%"
```

To start the request/response-example from the build-directory do:
```bash
#HOST1:
set "VSOMEIP_CONFIGURATION=<root directory of vSomeIP-Lib>\config\vsomeip-local.json"
set "VSOMEIP_APPLICATION_NAME=service-sample" 
cd /d "<root directory of vSomeIP-Lib>\buildlib\examples\Release"
response-sample.exe
#HOST1:
set "VSOMEIP_CONFIGURATION=<root directory of vSomeIP-Lib>\config\vsomeip-local.json"
set "VSOMEIP_APPLICATION_NAME=client-sample"
cd /d "<root directory of vSomeIP-Lib build>\examples\Release"
request-sample.exe
```

To start the subscribe/notify-example from the build-directory do:
```bash
#HOST1:
set "VSOMEIP_CONFIGURATION=<root directory of vSomeIP-Lib>\config\vsomeip-local.json"
set "VSOMEIP_APPLICATION_NAME=service-sample" 
cd /d "<root directory of vSomeIP-Lib>\buildlib\examples\Release"
notify-sample.exe
#HOST1:
set "VSOMEIP_CONFIGURATION=<root directory of vSomeIP-Lib>\config\vsomeip-local.json"
set "VSOMEIP_APPLICATION_NAME=client-sample"
cd /d "<root directory of vSomeIP-Lib build>\examples\Release"
subscribe-sample.exe
```

Note:
- Replace `<root directory of vSomeIP-Lib>` with your actual vsomeip-lib path
- Replace `<root directory of vSomeIP-Lib build>` with your actual vsomeip-lib build path
- Replace `$YOUR_PATH` with your actual installation path
- Replace "Release" with "Debug" for debug builds