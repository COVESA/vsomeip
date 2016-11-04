// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef WIN32
    #include <iostream>
#else
    #include <dlfcn.h>
    #include <sys/mman.h>
    #include <thread>
    #include <sstream>
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

// pointer to shared memory
configuration_data_t *utility::the_configuration_data__(nullptr);
// critical section to protect shared memory pointers, handles and ref count in this process
CriticalSection utility::its_local_configuration_mutex__;
// number of times auto_configuration_init() has been called in this process
uint16_t utility::its_configuration_refs__(0);

#ifdef WIN32
// global (inter-process) mutex
static HANDLE configuration_data_mutex(INVALID_HANDLE_VALUE);
// memory mapping handle
static HANDLE its_descriptor(INVALID_HANDLE_VALUE);
#endif

bool utility::auto_configuration_init(const std::shared_ptr<configuration> &_config) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);

#ifdef WIN32
    if (its_configuration_refs__ > 0) {
        assert(configuration_data_mutex != INVALID_HANDLE_VALUE);
        assert(its_descriptor != INVALID_HANDLE_VALUE);
        assert(the_configuration_data__ != nullptr);

        ++its_configuration_refs__;
    } else {
        configuration_data_mutex = CreateMutex(
            NULL,                           // default security attributes
            true,                           // initially owned
            "vSomeIP_SharedMemoryLock");   // named mutex
        if (configuration_data_mutex) {

            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                VSOMEIP_DEBUG << "utility::auto_configuration_init: use existing shared memory";

                // mapping already exists, wait for mutex release and map in
                DWORD waitResult = WaitForSingleObject(configuration_data_mutex, INFINITE);
                if (waitResult == WAIT_OBJECT_0) {

                    its_descriptor = CreateFileMapping(
                        INVALID_HANDLE_VALUE,           // use paging file
                        NULL,                           // default security
                        PAGE_READWRITE,                 // read/write access
                        0,                              // maximum object size (high-order DWORD)
                        sizeof(configuration_data_t),   // maximum object size (low-order DWORD)
                        VSOMEIP_SHM_NAME);              // name of mapping object

                    if (its_descriptor && GetLastError() == ERROR_ALREADY_EXISTS) {

                        void *its_segment = (LPTSTR)MapViewOfFile(its_descriptor,   // handle to map object
                            FILE_MAP_ALL_ACCESS, // read/write permission
                            0,
                            0,
                            sizeof(configuration_data_t));

                        if (its_segment) {
                            the_configuration_data__
                                = reinterpret_cast<configuration_data_t *>(its_segment);

                            ++its_configuration_refs__;
                        } else {
                            VSOMEIP_ERROR << "utility::auto_configuration_init: MapViewOfFile failed (" << GetLastError() << ")";
                        }
                    } else {
                        if (its_descriptor) {
                            VSOMEIP_ERROR << "utility::auto_configuration_init: CreateFileMapping failed. expected existing mapping";
                        } else {
                            VSOMEIP_ERROR << "utility::auto_configuration_init: CreateFileMapping failed (" << GetLastError() << ")";
                        }
                    }
                } else {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: WaitForSingleObject(mutex) failed (" << GetLastError() << ")";
                }

            } else {
                VSOMEIP_DEBUG << "utility::auto_configuration_init: create new shared memory";

                // create and init new mapping
                its_descriptor = CreateFileMapping(
                    INVALID_HANDLE_VALUE,           // use paging file
                    NULL,                           // default security
                    PAGE_READWRITE,                 // read/write access
                    0,                              // maximum object size (high-order DWORD)
                    sizeof(configuration_data_t),   // maximum object size (low-order DWORD)
                    VSOMEIP_SHM_NAME);              // name of mapping object

                if (its_descriptor) {
                    void *its_segment = (LPTSTR)MapViewOfFile(its_descriptor,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        sizeof(configuration_data_t));
                    if (its_segment) {
                        the_configuration_data__
                            = reinterpret_cast<configuration_data_t *>(its_segment);

                        the_configuration_data__->client_base_
                            = static_cast<unsigned short>(_config->get_diagnosis_address() << 8);
                        the_configuration_data__->used_client_ids_[0]
                            = the_configuration_data__->client_base_;
                        the_configuration_data__->client_base_++;
                        the_configuration_data__->max_used_client_ids_index_ = 1;
                        the_configuration_data__->max_assigned_client_id_low_byte_ = 0x00;

                        the_configuration_data__->routing_manager_host_ = 0x0000;
                        std::string its_name = _config->get_routing_host();
                        if (its_name == "")
                            the_configuration_data__->routing_manager_host_ = the_configuration_data__->client_base_;

                        its_configuration_refs__++;
                    } else {
                        VSOMEIP_ERROR << "utility::auto_configuration_init: MapViewOfFile failed (" << GetLastError() << ")";
                    }
                } else {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: CreateFileMapping failed (" << GetLastError() << ")";
                }
            }

            // always release mutex
            ReleaseMutex(configuration_data_mutex);
        } else {
            VSOMEIP_ERROR << "utility::auto_configuration_init: CreateMutex failed (" << GetLastError() << ")";
        }

        // cleanup in case of error
        if (the_configuration_data__ == nullptr) {
            if (its_descriptor != INVALID_HANDLE_VALUE) {
                CloseHandle(its_descriptor);
                its_descriptor = INVALID_HANDLE_VALUE;
            }
            if (configuration_data_mutex != INVALID_HANDLE_VALUE) {
                CloseHandle(configuration_data_mutex);
                configuration_data_mutex = INVALID_HANDLE_VALUE;
            }
        }
    }
#else
    const mode_t previous_mask(::umask(static_cast<mode_t>(_config->get_umask())));
    int its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
            static_cast<mode_t>(_config->get_permissions_shm()));
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
                        = static_cast<unsigned short>(_config->get_diagnosis_address() << 8);
                    the_configuration_data__->used_client_ids_[0]
                        = the_configuration_data__->client_base_;
                    the_configuration_data__->client_base_++;
                    the_configuration_data__->max_used_client_ids_index_ = 1;
                    the_configuration_data__->max_assigned_client_id_low_byte_ = 0x00;

                    the_configuration_data__->routing_manager_host_ = 0x0000;
                    std::string its_name = _config->get_routing_host();
                    if (its_name == "")
                        the_configuration_data__->routing_manager_host_ = the_configuration_data__->client_base_;

                    its_configuration_refs__++;

                    the_configuration_data__->initialized_ = 1;

                    ret = pthread_mutex_unlock(&the_configuration_data__->mutex_);
                    if (0 != ret) {
                        VSOMEIP_ERROR << "pthread_mutex_unlock() failed " << ret;
                    }
                }

                if(-1 == ::close(its_descriptor)) {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: "
                            "close failed: " << std::strerror(errno);
                }
            }
        }
    } else if (errno == EEXIST) {
        const mode_t previous_mask(::umask(static_cast<mode_t>(_config->get_umask())));
        its_descriptor = shm_open(VSOMEIP_SHM_NAME, O_RDWR,
                static_cast<mode_t>(_config->get_permissions_shm()));
        ::umask(previous_mask);
        if (-1 == its_descriptor) {
            VSOMEIP_ERROR << "utility::auto_configuration_init: "
                    "shm_open failed: " << std::strerror(errno);
        } else {
            // truncate to make sure we work on valid shm;
            // in case creator already called truncate, this effectively becomes a noop
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
                    configuration_data_t *configuration_data
                        = reinterpret_cast<configuration_data_t *>(its_segment);

                    // check if it is ready for use (for 3 seconds)
                    int retry_count = 300;
                    while (configuration_data->initialized_ == 0 && --retry_count > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    if (configuration_data->initialized_ == 0) {
                        VSOMEIP_ERROR << "utility::auto_configuration_init: data in shm not initialized";
                    } else {
                        the_configuration_data__ = configuration_data;

                        pthread_mutex_lock(&the_configuration_data__->mutex_);
                        its_configuration_refs__++;
                        pthread_mutex_unlock(&the_configuration_data__->mutex_);

                        if (-1 == ::close(its_descriptor)) {
                            VSOMEIP_ERROR << "utility::auto_configuration_init: "
                                    "close failed: " << std::strerror(errno);
                        }
                    }
                }
            }
        }
    }
#endif
    return (the_configuration_data__ != nullptr);
}

void utility::auto_configuration_exit(client_t _client) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);
    if (the_configuration_data__) {
#ifdef WIN32
        // not manipulating data in shared memory, no need to take global mutex

        assert(configuration_data_mutex != INVALID_HANDLE_VALUE);
        assert(its_descriptor != INVALID_HANDLE_VALUE);

        its_configuration_refs__--;

        if (is_routing_manager_host(_client)) {
            set_routing_manager_host(0x0000);
        }

        if (its_configuration_refs__ == 0) {
            UnmapViewOfFile(the_configuration_data__);
            the_configuration_data__ = nullptr;

            CloseHandle(its_descriptor);
            its_descriptor = NULL;

            CloseHandle(configuration_data_mutex);
            configuration_data_mutex = NULL;
        }
#else
        its_configuration_refs__--;

        bool unlink_shm = false;
        if (is_routing_manager_host(_client)) {
            set_routing_manager_host(0x0000);
            unlink_shm = true;
        }

        if (its_configuration_refs__ == 0) {
            if (-1 == ::munmap(the_configuration_data__, sizeof(configuration_data_t))) {
                VSOMEIP_ERROR << "utility::auto_configuration_exit: "
                        "munmap failed: " << std::strerror(errno);
            } else {
                VSOMEIP_DEBUG <<  "utility::auto_configuration_exit: "
                        "munmap succeeded.";
                the_configuration_data__ = nullptr;
                if (unlink_shm) {
                    shm_unlink(VSOMEIP_SHM_NAME);
                }
            }

        }
#endif
    }
}

bool utility::is_used_client_id(client_t _client) {
    for (int i = 0;
         i < the_configuration_data__->max_used_client_ids_index_;
         i++) {
        if (the_configuration_data__->used_client_ids_[i] == _client
                || _client
                        < (the_configuration_data__->client_base_
                           + the_configuration_data__->max_assigned_client_id_low_byte_)) {
            return true;
        }
    }
#ifndef WIN32
    std::stringstream its_client;
    its_client << VSOMEIP_BASE_PATH << std::hex << _client;
    if (exists(its_client.str())) {
        if (-1 == ::unlink(its_client.str().c_str())) {
            VSOMEIP_WARNING << "unlink failed for " << its_client.str() << ". Client identifier 0x"
                    << std::hex << _client << " can't be reused!";
            return true;
        }

    }
#endif
    return false;
}

client_t utility::request_client_id(const std::shared_ptr<configuration> &_config, const std::string &_name, client_t _client) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);

    if (the_configuration_data__ != nullptr) {
#ifdef WIN32
        DWORD waitResult = WaitForSingleObject(configuration_data_mutex, INFINITE);
        assert(waitResult == WAIT_OBJECT_0);
        (void)waitResult;
#else
        pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif
        const std::string its_name = _config->get_routing_host();
        bool set_client_as_manager_host(false);
        if (its_name != "" && its_name == _name) {
            if (the_configuration_data__->routing_manager_host_ == 0x0000) {
                set_client_as_manager_host = true;
            } else {
                VSOMEIP_ERROR << "Routing manager with id " << the_configuration_data__->routing_manager_host_ << " already exists.";
#ifdef WIN32
                BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
                assert(releaseResult);
                (void)releaseResult;
#else
                pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
                return VSOMEIP_DIAGNOSIS_ADDRESS;
            }
        } else if (its_name == "" && the_configuration_data__->routing_manager_host_ == 0x0000) {
            set_client_as_manager_host = true;
        }

        if (the_configuration_data__->max_used_client_ids_index_
                == VSOMEIP_MAX_CLIENTS) {
            VSOMEIP_ERROR << "Max amount of possible concurrent active"
                    << " vsomeip applications reached.";
#ifdef WIN32
            BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
            assert(releaseResult);
            (void)releaseResult;
#else
            pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
            return ILLEGAL_CLIENT;
        }

        bool configured_and_not_used(false);
        if (_client != ILLEGAL_CLIENT && !is_used_client_id(_client)) {
            configured_and_not_used = true;
        }

        if (_client == ILLEGAL_CLIENT || is_used_client_id(_client)) {
            _client = the_configuration_data__->client_base_;
        }

        while (is_used_client_id(_client)) {
            if ((_client & 0x00FF) + 1 > VSOMEIP_MAX_CLIENTS) {
                _client = the_configuration_data__->client_base_;
                the_configuration_data__->max_assigned_client_id_low_byte_ = 0;
            } else {
                _client++;
            }
        }
        if (!configured_and_not_used) {
            the_configuration_data__->max_assigned_client_id_low_byte_ =
                    static_cast<unsigned char>((_client & 0x00FF)
                            % VSOMEIP_MAX_CLIENTS);
        }

        if (set_client_as_manager_host) {
            the_configuration_data__->routing_manager_host_ = _client;
        }

        the_configuration_data__->used_client_ids_[
            the_configuration_data__->max_used_client_ids_index_] = _client;
        the_configuration_data__->max_used_client_ids_index_++;


#ifdef WIN32
        BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
        assert(releaseResult);
        (void)releaseResult;
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
        return _client;
    }
    return VSOMEIP_DIAGNOSIS_ADDRESS;
}

void utility::release_client_id(client_t _client) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);
    if (the_configuration_data__ != nullptr) {
#ifdef WIN32
        WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
        pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif
        int i = 0;
        while (the_configuration_data__->used_client_ids_[i] != _client &&
               i < the_configuration_data__->max_used_client_ids_index_) {
            i++;
        }

        if (i <= the_configuration_data__->max_used_client_ids_index_) {
            the_configuration_data__->max_used_client_ids_index_--;
            for (; i < the_configuration_data__->max_used_client_ids_index_; i++) {
                the_configuration_data__->used_client_ids_[i]
                    = the_configuration_data__->used_client_ids_[i+1];
            }
        }
#ifdef WIN32
        ReleaseMutex(configuration_data_mutex);
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
    }
}

bool utility::is_routing_manager_host(client_t _client) {
    if (the_configuration_data__ == nullptr) {
        VSOMEIP_ERROR << "utility::is_routing_manager_host: configuration data not available";
        return false;
    }

#ifdef WIN32
    WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
    pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif

    bool is_routing_manager = (the_configuration_data__->routing_manager_host_ == _client);

#ifdef WIN32
    ReleaseMutex(configuration_data_mutex);
#else
    pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif

    return is_routing_manager;
}

void utility::set_routing_manager_host(client_t _client) {
    if (the_configuration_data__ == nullptr) {
        VSOMEIP_ERROR << "utility::set_routing_manager_host: configuration data not available";
        return;
    }

#ifdef WIN32
    WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
    pthread_mutex_lock(&the_configuration_data__->mutex_);
#endif

    the_configuration_data__->routing_manager_host_ = _client;

#ifdef WIN32
    ReleaseMutex(configuration_data_mutex);
#else
    pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
}

} // namespace vsomeip
