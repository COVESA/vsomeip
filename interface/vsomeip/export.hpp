// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(VSOMEIP_DLL)  // Building/using a DLL on Windows
    #if defined(VSOMEIP_EXPORTS)  // While building the vsomeip3 DLL itself
      #define VSOMEIP_API __declspec(dllexport)
    #else                    // While consuming the DLL
      #define VSOMEIP_API __declspec(dllimport)
    #endif
  #else
    #define VSOMEIP_API
  #endif
#else
  #define VSOMEIP_API
#endif

#ifdef _WIN32
  #if defined(VSOMEIP_DLL)  // Building/using a DLL on Windows
    #if defined(UDP_EXPORTS)  // While building the vsomeip3 DLL itself
      #define UDP_API __declspec(dllexport)
    #else                    // While consuming the DLL
      #define UDP_API __declspec(dllimport)
    #endif
  #else
    #define UDP_API
  #endif
#else
  #define UDP_API
#endif

#ifndef VSOMEIP_V3_EXPORT_HPP_
#define VSOMEIP_V3_EXPORT_HPP_

#if _WIN32
#define VSOMEIP_EXPORT __declspec(dllexport)
#define VSOMEIP_EXPORT_CLASS_EXPLICIT

#if VSOMEIP_DLL_COMPILATION
#define VSOMEIP_IMPORT_EXPORT __declspec(dllexport)
#else
#define VSOMEIP_IMPORT_EXPORT __declspec(dllimport)
#endif

#if VSOMEIP_DLL_COMPILATION_CONFIG
#define VSOMEIP_IMPORT_EXPORT_CONFIG __declspec(dllexport)
#else
#define VSOMEIP_IMPORT_EXPORT_CONFIG __declspec(dllimport)
#endif
#else
#define VSOMEIP_EXPORT
#define VSOMEIP_IMPORT_EXPORT
#define VSOMEIP_IMPORT_EXPORT_CONFIG
#endif

#endif // VSOMEIP_V3_EXPORT_HPP_
