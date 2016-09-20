// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
    #include <iostream>
#else
    #include <dlfcn.h>
    #include <sys/mman.h>
#endif

#include <sys/stat.h>

#ifndef WIN32
    #include <fcntl.h>
#endif

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>

#include "../include/byteorder.hpp"
#include "../include/utility.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"

#ifdef WIN32
    #ifndef _WINSOCKAPI_
        #include <Windows.h>
    #endif
#endif

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
uint16_t utility::its_configuration_refs__(0);

#ifdef WIN32
HANDLE its_descriptor;
#endif

bool utility::auto_configuration_init(const std::string &_name) {
    std::shared_ptr<configuration> its_config(configuration::get());
#ifdef WIN32
    its_descriptor = CreateFileMapping(
        INVALID_HANDLE_VALUE,           // use paging file
        NULL,                           // default security
        PAGE_READWRITE,                 // read/write access
        0,                              // maximum object size (high-order DWORD)
        sizeof(configuration_data_t),   // maximum object size (low-order DWORD)
        VSOMEIP_SHM_NAME);              // name of mapping object

    if (its_descriptor != NULL) {
        void *its_segment = (LPTSTR)MapViewOfFile(its_descriptor,   // handle to map object
            FILE_MAP_ALL_ACCESS, // read/write permission
            0,
            0,
            sizeof(configuration_data_t));
        the_configuration_data__
            = reinterpret_cast<configuration_data_t *>(its_segment);
        if (the_configuration_data__ != nullptr && is_routing_manager_host__) {
            the_configuration_data__->mutex_ = CreateMutex(
                NULL,                           // default security attributes
                true,                           // initially owned
                "vSomeIP\\SharedMemoryLock");   // named mutex

            the_configuration_data__->client_base_
                = static_cast<unsigned short>(its_config->get_diagnosis_address() << 8);
            the_configuration_data__->used_client_ids_[0]
                = the_configuration_data__->client_base_;
            the_configuration_data__->client_base_++;
            the_configuration_data__->max_used_client_ids_index_ = 1;

            if (its_config->get_routing_host() == "" ||
                its_config->get_routing_host() == _name)
                is_routing_manager_host__ = true;

            its_configuration_refs__++;

            ReleaseMutex(the_configuration_data__->mutex_);
        }
    } else {
        // TODO: Error
    }
#else
    const mode_t previous_mask(::umask(static_cast<mode_t>(its_config->get_umask())));
    int its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
            static_cast<mode_t>(its_config->get_permissions_shm()));
    ::umask(previous_mask);
    if (its_descriptor > -1) {
        if (-1 == ftruncate(its_descriptor, sizeof(configuration_data_t))) {
            VSOMEIP_ERROR << "utility::auto_configuration_init: "
                    "ftruncate failed: " << std::strerror(errno);
        } else {
            void *its_segment = mmap(0, sizeof(configuration_data_t),
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     its_descriptor, 0);
            if(MAP_FAILED == its_segment) {
                VSOMEIP_ERROR << "utility::auto_configuration_init: "
                        "mmap failed: " << std::strerror(errno);
            } else {
                the_configuration_data__
                    = reinterpret_cast<configuration_data_t *>(its_segment);
                if(-1 == ::close(its_descriptor)) {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: "
                            "close failed: " << std::strerror(errno);
                }
                if (the_configuration_data__ != nullptr) {
                    int ret;
                    pthread_mutexattr_t attr;
                    ret = pthread_mutexattr_init(&attr);
                    if (0 == ret) {
                        ret = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
                        if (0 != ret) {
                            VSOMEIP_ERROR << "pthread_mutexattr_setpshared() failed " << ret;
                        }
                    } else {
                        VSOMEIP_ERROR << "pthread_mutexattr_init() failed " << ret;
                    }
                    ret = pthread_mutex_init(&the_configuration_data__->mutex_, (0==ret)?&attr:NULL);
                    if (0 != ret) {
                        VSOMEIP_ERROR << "pthread_mutex_init() failed " << ret;
                    }
                    ret = pthread_mutex_lock(&the_configuration_data__->mutex_);
                    if (0 != ret) {
                        VSOMEIP_ERROR << "pthread_mutex_lock() failed " << ret;
                    }

                    the_configuration_data__->client_base_
                        = static_cast<unsigned short>(its_config->get_diagnosis_address() << 8);
                    the_configuration_data__->used_client_ids_[0]
                        = the_configuration_data__->client_base_;
                    the_configuration_data__->client_base_++;
                    the_configuration_data__->max_used_client_ids_index_ = 1;

                    if (its_config->get_routing_host() == "" ||
                        its_config->get_routing_host() == _name)
                        is_routing_manager_host__ = true;

                    its_configuration_refs__++;

                    ret = pthread_mutex_unlock(&the_configuration_data__->mutex_);
                    if (0 != ret) {
                        VSOMEIP_ERROR << "pthread_mutex_unlock() failed " << ret;
                    }
                }
            }
        }
    } else {
        const mode_t previous_mask(::umask(static_cast<mode_t>(its_config->get_umask())));
        its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR,
                static_cast<mode_t>(its_config->get_permissions_shm()));
        ::umask(previous_mask);
        if (-1 == its_descriptor) {
            VSOMEIP_ERROR << "utility::auto_configuration_init: "
                    "shm_open failed: " << std::strerror(errno);
        } else {
            void *its_segment = mmap(0, sizeof(configuration_data_t),
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     its_descriptor, 0);
            if(MAP_FAILED == its_segment) {
                VSOMEIP_ERROR << "utility::auto_configuration_init: "
                        "mmap failed: " << std::strerror(errno);
            } else {
                the_configuration_data__
                    = reinterpret_cast<configuration_data_t *>(its_segment);
                if (-1 == ::close(its_descriptor)) {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: "
                            "close failed: " << std::strerror(errno);
                }
#ifdef WIN32
                WaitForSingleObject(the_configuration_data__->mutex_, INFINITE);
#else
                pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif
                its_configuration_refs__++;
#ifdef WIN32
                ReleaseMutex(the_configuration_data__->mutex_);
#else
                pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
            }
        }
    }
#endif
    return (the_configuration_data__ != nullptr);
}

void utility::auto_configuration_exit() {
    if (the_configuration_data__) {
#ifdef WIN32
        WaitForSingleObject(the_configuration_data__->mutex_, INFINITE);
        its_configuration_refs__--;
        ReleaseMutex(the_configuration_data__->mutex_);

        if (its_configuration_refs__ == 0) {
            UnmapViewOfFile(the_configuration_data__);
        }

        if (is_routing_manager_host__) {
            CloseHandle(its_descriptor);
        }
#else
        pthread_mutex_lock(&the_configuration_data__->mutex_);
        its_configuration_refs__--;
        pthread_mutex_unlock(&the_configuration_data__->mutex_);

        if (its_configuration_refs__ == 0) {
            if (-1 == ::munmap(the_configuration_data__, sizeof(configuration_data_t))) {
                VSOMEIP_ERROR << "utility::auto_configuration_exit: "
                        "munmap failed: " << std::strerror(errno);
            } else {
                VSOMEIP_DEBUG <<  "utility::auto_configuration_exit: "
                        "munmap succeeded.";
                the_configuration_data__ = nullptr;
            }
        }
        if (is_routing_manager_host__) {
            shm_unlink(VSOMEIP_SHM_NAME);
        }
#endif
    }
}

bool utility::is_used_client_id(client_t _client) {
    for (int i = 0;
         i < the_configuration_data__->max_used_client_ids_index_;
         i++) {
        if (the_configuration_data__->used_client_ids_[i] == _client)
            return true;
    }
    return false;
}

client_t utility::request_client_id(client_t _client) {
    if (the_configuration_data__ != nullptr) {
#ifdef WIN32
        WaitForSingleObject(the_configuration_data__->mutex_, INFINITE);
#else
        pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif
        if (the_configuration_data__->max_used_client_ids_index_
                == VSOMEIP_MAX_CLIENTS) {
            return ILLEGAL_CLIENT;
        }

        if (_client == ILLEGAL_CLIENT || is_used_client_id(_client)) {
            _client = the_configuration_data__->client_base_;
        }

        while (is_used_client_id(_client)) _client++;

        the_configuration_data__->used_client_ids_[
            the_configuration_data__->max_used_client_ids_index_] = _client;
        the_configuration_data__->max_used_client_ids_index_++;
#ifdef WIN32
        ReleaseMutex(the_configuration_data__->mutex_);
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
        return _client;
    }
    return VSOMEIP_DIAGNOSIS_ADDRESS;
}

void utility::release_client_id(client_t _client) {
    if (the_configuration_data__ != nullptr) {
#ifdef WIN32
        WaitForSingleObject(the_configuration_data__->mutex_, INFINITE);
#else
        pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif
        int i = 0;
        while (the_configuration_data__->used_client_ids_[i] != _client &&
               i < the_configuration_data__->max_used_client_ids_index_) {
            i++;
        }

        if (i < the_configuration_data__->max_used_client_ids_index_) {
            the_configuration_data__->max_used_client_ids_index_--;
            for (; i < the_configuration_data__->max_used_client_ids_index_; i++) {
                the_configuration_data__->used_client_ids_[i]
                    = the_configuration_data__->used_client_ids_[i+1];
            }
        }
#ifdef WIN32
        ReleaseMutex(the_configuration_data__->mutex_);
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
    }
}

bool utility::is_routing_manager_host() {
    return is_routing_manager_host__;
}

} // namespace vsomeip
