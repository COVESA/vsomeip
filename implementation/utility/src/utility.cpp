// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
#include <Windows.h>
#include <iostream>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#endif

#include <sys/stat.h>

#ifndef WIN32
#include <fcntl.h>
#endif

#include <vsomeip/defines.hpp>

#include "../include/byteorder.hpp"
#include "../include/utility.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {

uint32_t utility::get_message_size(const byte_t *_data, uint32_t _size) {
    uint32_t its_size(0);
    if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
        its_size = VSOMEIP_SOMEIP_HEADER_SIZE
                + VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7]);
    }
    return (its_size);
}

uint32_t utility::get_payload_size(const byte_t *_data, uint32_t _size) {
    uint32_t its_size(0);
    if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
        its_size = VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7])
                - VSOMEIP_SOMEIP_HEADER_SIZE;
    }
    return (its_size);
}

void * utility::load_library(const std::string &_path,
        const std::string &_symbol) {
    void * its_symbol = 0;

#ifdef WIN32
    std::string path = _path.substr(0, _path.length() - 5).substr(3) + ".dll";

    HINSTANCE hDLL = LoadLibrary(path.c_str());
    if (hDLL != NULL) {
        //loadedLibraries_.insert(itsLibrary);
        std::cout << "Loading interface library \"" << path << "\" succeeded." << std::endl;

        typedef UINT(CALLBACK* LPFNDLLFUNC1)(DWORD, UINT);

        LPFNDLLFUNC1 lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(hDLL, _symbol.c_str());
        if (!lpfnDllFunc1)
        {
            FreeLibrary(hDLL);
            std::cerr << "Loading symbol \"" << _symbol << "\" failed (" << GetLastError() << ")" << std::endl;
        }
        else
        {
            its_symbol = lpfnDllFunc1;
        }

    }
    else {
        std::cerr << "Loading interface library \"" << path << "\" failed (" << GetLastError() << ")" << std::endl;
    }
#else
    void *handle = dlopen(_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (0 != handle) {
        its_symbol = dlsym(handle, _symbol.c_str());
    } else {
        VSOMEIP_ERROR << "Loading failed: (" << dlerror() << ")";
    }
#endif
    return (its_symbol);
}

bool utility::exists(const std::string &_path) {
    struct stat its_stat;
    return (stat(_path.c_str(), &its_stat) == 0);
}

bool utility::is_file(const std::string &_path) {
    struct stat its_stat;
    if (stat(_path.c_str(), &its_stat) == 0) {
        if (its_stat.st_mode & S_IFREG)
            return true;
    }
    return false;
}

bool utility::is_folder(const std::string &_path) {
    struct stat its_stat;
    if (stat(_path.c_str(), &its_stat) == 0) {
        if (its_stat.st_mode & S_IFDIR)
            return true;
    }
    return false;
}

configuration_data_t *utility::the_configuration_data__(nullptr);
bool utility::is_routing_manager_host__(false);

bool utility::auto_configuration_init() {
#ifndef WIN32
    int its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR|O_CREAT|O_EXCL, 0660);
    if (its_descriptor > -1) {
        if (0 == ftruncate(its_descriptor, sizeof(configuration_data_t))) {
            void *its_segment = mmap(0, sizeof(configuration_data_t),
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     its_descriptor, 0);
            the_configuration_data__
                = reinterpret_cast<configuration_data_t *>(its_segment);
            if (the_configuration_data__ != nullptr) {
                std::lock_guard<std::mutex> its_lock(the_configuration_data__->mutex_);
                the_configuration_data__->ref_ = 0;
                the_configuration_data__->next_client_id_ = (VSOMEIP_DIAGNOSIS_ADDRESS << 8);
                is_routing_manager_host__ = true;
            }
        } else {
            // TODO: an error message
        }
    } else {
        its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR, 0660);
        if (its_descriptor > -1) {
            void *its_segment = mmap(0, sizeof(configuration_data_t),
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     its_descriptor, 0);
            the_configuration_data__
                = reinterpret_cast<configuration_data_t *>(its_segment);
        } else {
            // TODO: an error message
        }
    }
#endif
    return (the_configuration_data__ != nullptr);
}

void utility::auto_configuration_exit() {
#ifndef WIN32
    if (the_configuration_data__) {
        munmap(the_configuration_data__, sizeof(configuration_data_t));
        if (is_routing_manager_host__) {
            shm_unlink(VSOMEIP_SHM_NAME);
        }
    }
#endif
}

client_t utility::get_client_id() {
    if (the_configuration_data__ != nullptr) {
        std::lock_guard<std::mutex> its_lock(the_configuration_data__->mutex_);
        the_configuration_data__->next_client_id_++;
        return the_configuration_data__->next_client_id_;
    }
    return VSOMEIP_DIAGNOSIS_ADDRESS;
}

bool utility::is_routing_manager_host() {
    return is_routing_manager_host__;
}

} // namespace vsomeip
