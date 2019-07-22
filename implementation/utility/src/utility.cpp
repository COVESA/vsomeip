// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #include <iostream>
    #include <intrin.h>
#else
    #include <dlfcn.h>
    #include <signal.h>
    #include <sys/mman.h>
    #include <thread>
    #include <sstream>
#endif

#include <sys/stat.h>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>

#include "../include/byteorder.hpp"
#include "../include/utility.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"

#ifdef _WIN32
    #ifndef _WINSOCKAPI_
        #include <Windows.h>
    #endif
#endif

namespace vsomeip {

uint64_t utility::get_message_size(const byte_t *_data, size_t _size) {
    uint64_t its_size(0);
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

const std::string utility::get_base_path(
        const std::shared_ptr<configuration> &_config) {
    return std::string(VSOMEIP_BASE_PATH + _config->get_network() + "-");
}

const std::string utility::get_shm_name(
        const std::shared_ptr<configuration> &_config) {
    return std::string("/" + _config->get_network());
}

// pointer to shared memory
configuration_data_t *utility::the_configuration_data__(nullptr);
// critical section to protect shared memory pointers, handles and ref count in this process
CriticalSection utility::its_local_configuration_mutex__;
// number of times auto_configuration_init() has been called in this process
std::atomic<std::uint16_t> utility::its_configuration_refs__(0);
// pointer to used client IDs array in shared memory
std::uint16_t* utility::used_client_ids__(0);

#ifdef _WIN32
// global (inter-process) mutex
static HANDLE configuration_data_mutex(INVALID_HANDLE_VALUE);
// memory mapping handle
static HANDLE its_descriptor(INVALID_HANDLE_VALUE);
#endif

bool utility::auto_configuration_init(const std::shared_ptr<configuration> &_config) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);
    const std::uint16_t its_max_clients =
            get_max_number_of_clients(_config->get_diagnosis_mask());

    const size_t its_shm_size = sizeof(configuration_data_t) +
            (its_max_clients + 1) * sizeof(client_t);
#ifdef _WIN32
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
                VSOMEIP_INFO << "utility::auto_configuration_init: use existing shared memory";

                // mapping already exists, wait for mutex release and map in
                DWORD waitResult = WaitForSingleObject(configuration_data_mutex, INFINITE);
                if (waitResult == WAIT_OBJECT_0) {

                    its_descriptor = CreateFileMapping(
                        INVALID_HANDLE_VALUE,           // use paging file
                        NULL,                           // default security
                        PAGE_READWRITE,                 // read/write access
                        0,                              // maximum object size (high-order DWORD)
                        its_shm_size,                   // maximum object size (low-order DWORD)
                        utility::get_shm_name(_config).c_str());// name of mapping object

                    if (its_descriptor && GetLastError() == ERROR_ALREADY_EXISTS) {

                        void *its_segment = (LPTSTR)MapViewOfFile(its_descriptor,   // handle to map object
                            FILE_MAP_ALL_ACCESS, // read/write permission
                            0,
                            0,
                            its_shm_size);

                        if (its_segment) {
                            the_configuration_data__
                                = reinterpret_cast<configuration_data_t *>(its_segment);

                            ++its_configuration_refs__;
                            used_client_ids__ = reinterpret_cast<unsigned short*>(
                                    reinterpret_cast<size_t>(&the_configuration_data__->routing_manager_host_) + sizeof(unsigned short));

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
                VSOMEIP_INFO << "utility::auto_configuration_init: create new shared memory";

                // create and init new mapping
                its_descriptor = CreateFileMapping(
                    INVALID_HANDLE_VALUE,           // use paging file
                    NULL,                           // default security
                    PAGE_READWRITE,                 // read/write access
                    0,                              // maximum object size (high-order DWORD)
                    its_shm_size,                   // maximum object size (low-order DWORD)
                    utility::get_shm_name(_config).c_str());// name of mapping object

                if (its_descriptor) {
                    void *its_segment = (LPTSTR)MapViewOfFile(its_descriptor,   // handle to map object
                        FILE_MAP_ALL_ACCESS, // read/write permission
                        0,
                        0,
                        its_shm_size);
                    if (its_segment) {
                        the_configuration_data__
                            = reinterpret_cast<configuration_data_t *>(its_segment);

                        the_configuration_data__->client_base_
                            = static_cast<unsigned short>((_config->get_diagnosis_address() << 8) & _config->get_diagnosis_mask());
                        the_configuration_data__->max_clients_ = its_max_clients;
                        the_configuration_data__->max_used_client_ids_index_ = 1;
                        the_configuration_data__->max_assigned_client_id_ = 0x00;
                        the_configuration_data__->routing_manager_host_ = 0x0000;
                        // the clientid array starts right after the routing_manager_host_ struct member
                        used_client_ids__ = reinterpret_cast<unsigned short*>(
                                reinterpret_cast<size_t>(&the_configuration_data__->routing_manager_host_) + sizeof(unsigned short));
                        used_client_ids__[0] = the_configuration_data__->client_base_;
                        the_configuration_data__->client_base_++;
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
    if (its_configuration_refs__ > 0) {
        // shm is already mapped into the process
        its_configuration_refs__++;
    } else {
         int its_descriptor = shm_open(utility::get_shm_name(_config).c_str(), O_RDWR | O_CREAT | O_EXCL,
                static_cast<mode_t>(_config->get_permissions_shm()));
         if (its_descriptor > -1) {
            if (-1 == chmod(std::string("/dev/shm").append(utility::get_shm_name(_config)).c_str(),
                    static_cast<mode_t>(_config->get_permissions_uds()))) {
                VSOMEIP_ERROR << __func__ << ": chmod: " << strerror(errno);
            }
            if (-1 == ftruncate(its_descriptor, its_shm_size)) {
                VSOMEIP_ERROR << "utility::auto_configuration_init: "
                        "ftruncate failed: " << std::strerror(errno);
            } else {
                void *its_segment = mmap(0, its_shm_size,
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
                            ret = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
                            if (0 != ret) {
                                VSOMEIP_ERROR << "pthread_mutexattr_setrobust() failed " << ret;
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
                            = static_cast<unsigned short>((_config->get_diagnosis_address() << 8) & _config->get_diagnosis_mask());
                        the_configuration_data__->max_clients_ = its_max_clients;
                        the_configuration_data__->max_used_client_ids_index_ = 1;
                        the_configuration_data__->max_assigned_client_id_ = 0x00;
                        the_configuration_data__->routing_manager_host_ = 0x0000;
                        // the clientid array starts right after the routing_manager_host_ struct member
                        used_client_ids__ = reinterpret_cast<unsigned short*>(
                                reinterpret_cast<size_t>(&the_configuration_data__->routing_manager_host_) + sizeof(unsigned short));
                        used_client_ids__[0] = the_configuration_data__->client_base_;
                        the_configuration_data__->client_base_++;

                        std::string its_name = _config->get_routing_host();

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
            its_descriptor = shm_open(utility::get_shm_name(_config).c_str(), O_RDWR,
                    static_cast<mode_t>(_config->get_permissions_shm()));

            int retry_count = 8;
            std::chrono::milliseconds retry_delay = std::chrono::milliseconds(10);
            while (its_descriptor == -1 && retry_count-- > 0) {
                std::this_thread::sleep_for(retry_delay);
                its_descriptor = shm_open(utility::get_shm_name(_config).c_str(), O_RDWR,
                        static_cast<mode_t>(_config->get_permissions_shm()));
                retry_delay *= 2;
            }

            if (-1 == its_descriptor) {
                VSOMEIP_ERROR << "utility::auto_configuration_init: "
                        "shm_open failed: " << std::strerror(errno);
            } else {
                // truncate to make sure we work on valid shm;
                // in case creator already called truncate, this effectively becomes a noop
                if (-1 == ftruncate(its_descriptor, its_shm_size)) {
                    VSOMEIP_ERROR << "utility::auto_configuration_init: "
                        "ftruncate failed: " << std::strerror(errno);
                } else {
                    void *its_segment = mmap(0, its_shm_size,
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

                            int its_result = pthread_mutex_lock(&the_configuration_data__->mutex_);

                            used_client_ids__ = reinterpret_cast<unsigned short*>(
                                    reinterpret_cast<size_t>(&the_configuration_data__->routing_manager_host_)
                                            + sizeof(unsigned short));

                            if (EOWNERDEAD == its_result) {
                                VSOMEIP_WARNING << "utility::auto_configuration_init EOWNERDEAD";
                                check_client_id_consistency();
                                if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
                                    VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
                                }
                            }

                            its_configuration_refs__++;
                            pthread_mutex_unlock(&the_configuration_data__->mutex_);
                        }

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

void utility::auto_configuration_exit(client_t _client,
        const std::shared_ptr<configuration> &_config) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);
    if (the_configuration_data__) {
#ifdef _WIN32
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
            used_client_ids__ = nullptr;

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
            const std::uint16_t its_max_clients =
                    get_max_number_of_clients(_config->get_diagnosis_mask());
            const size_t its_shm_size = sizeof(configuration_data_t) +
                    (its_max_clients + 1) * sizeof(client_t);
            if (-1 == ::munmap(the_configuration_data__, its_shm_size)) {
                VSOMEIP_ERROR << "utility::auto_configuration_exit: "
                        "munmap failed: " << std::strerror(errno);
            } else {
                VSOMEIP_INFO <<  "utility::auto_configuration_exit: "
                        "munmap succeeded.";
                the_configuration_data__ = nullptr;
                used_client_ids__ = nullptr;
                if (unlink_shm) {
                    shm_unlink(utility::get_shm_name(_config).c_str());
                }
            }

        }
#endif
    }
}

bool utility::is_used_client_id(client_t _client,
        const std::shared_ptr<configuration> &_config) {
    for (int i = 0;
         i < the_configuration_data__->max_used_client_ids_index_;
         i++) {
        if (used_client_ids__[i] == _client) {
            return true;
        }
    }
#ifndef _WIN32
    std::stringstream its_client;
    its_client << utility::get_base_path(_config) << std::hex << _client;
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

std::set<client_t> utility::get_used_client_ids() {
    std::set<client_t> clients;

    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);

    if (the_configuration_data__ != nullptr) {
#ifdef _WIN32
        DWORD waitResult = WaitForSingleObject(configuration_data_mutex, INFINITE);
        assert(waitResult == WAIT_OBJECT_0);
        (void)waitResult;
#else
        if (EOWNERDEAD == pthread_mutex_lock(&the_configuration_data__->mutex_)) {
            VSOMEIP_WARNING << "utility::request_client_id EOWNERDEAD";
            check_client_id_consistency();
            if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
                VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
            }
        }
#endif
        for (int i = 1;
             i < the_configuration_data__->max_used_client_ids_index_;
             i++) {
            clients.insert(used_client_ids__[i]);
        }

#ifdef _WIN32
        BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
        assert(releaseResult);
        (void)releaseResult;
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
    }

    return clients;
}

client_t utility::request_client_id(const std::shared_ptr<configuration> &_config, const std::string &_name, client_t _client) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);

    if (the_configuration_data__ != nullptr) {
        const std::string its_name = _config->get_routing_host();
#ifdef _WIN32
        DWORD waitResult = WaitForSingleObject(configuration_data_mutex, INFINITE);
        assert(waitResult == WAIT_OBJECT_0);
        (void)waitResult;
#else
        if (EOWNERDEAD == pthread_mutex_lock(&the_configuration_data__->mutex_)) {
            VSOMEIP_WARNING << "utility::request_client_id EOWNERDEAD";
            check_client_id_consistency();
            if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
                VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
            }
        }

        pid_t pid = getpid();
        if (its_name == "" || _name == its_name) {
            if (the_configuration_data__->pid_ != 0) {
                if (pid != the_configuration_data__->pid_) {
                    if (kill(the_configuration_data__->pid_, 0) == -1) {
                        VSOMEIP_WARNING << "Routing Manager seems to be inactive. Taking over...";
                        the_configuration_data__->routing_manager_host_ = 0x0000;
                    }
                }
            }
        }
#endif

        bool set_client_as_manager_host(false);
        if (its_name != "" && its_name == _name) {
            if (the_configuration_data__->routing_manager_host_ == 0x0000) {
                set_client_as_manager_host = true;
            } else {
                VSOMEIP_ERROR << "Routing manager with id " << the_configuration_data__->routing_manager_host_ << " already exists.";
#ifdef _WIN32
                BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
                assert(releaseResult);
                (void)releaseResult;
#else
                pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
                return ILLEGAL_CLIENT;
            }
        } else if (its_name == "" && the_configuration_data__->routing_manager_host_ == 0x0000) {
            set_client_as_manager_host = true;
        }

        if (the_configuration_data__->max_used_client_ids_index_
                == the_configuration_data__->max_clients_) {
            VSOMEIP_ERROR << "Max amount of possible concurrent active"
                    << " vsomeip applications reached (" << std::dec <<
                    the_configuration_data__->max_clients_ << ").";
#ifdef _WIN32
            BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
            assert(releaseResult);
            (void)releaseResult;
#else
            pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
            return ILLEGAL_CLIENT;
        }

        bool use_autoconfig = true;
        if (_name != "" && _client != ILLEGAL_CLIENT) { // preconfigured client id
            // check if application name has preconfigured client id in json
            const client_t its_preconfigured_client_id = _config->get_id(_name);
            if (its_preconfigured_client_id) {
                // preconfigured client id for application name present in json
                if (its_preconfigured_client_id == _client) {
                    // preconfigured client id for application name present in json
                    // and requested
                    if (!is_used_client_id(_client, _config)) {
                        use_autoconfig = false;
                    }
                } else {
                    // preconfigured client id for application name present in
                    // json, but different client id requested
                    if (!is_used_client_id(its_preconfigured_client_id, _config)) {
                        // assign preconfigured client id if not already used
                        _client = its_preconfigured_client_id;
                        use_autoconfig = false;
                    } else if (!is_used_client_id(_client, _config)) {
                        // if preconfigured client id is already used and
                        // requested is unused assign requested
                        use_autoconfig = false;
                    }
                }
            }
        }

        if (use_autoconfig) {
            if (_client == ILLEGAL_CLIENT || is_used_client_id(_client, _config)) {
                if (the_configuration_data__->max_assigned_client_id_ != 0x00) {
                    _client = the_configuration_data__->max_assigned_client_id_;
                } else {
                    _client = the_configuration_data__->client_base_;
                }
            }
            int increase_count = 0;

            while (_client <= the_configuration_data__->max_assigned_client_id_
                    || _config->is_configured_client_id(_client)
                    || is_used_client_id(_client, _config)) {
                if (_client + 1 > used_client_ids__[0] + the_configuration_data__->max_clients_) {
                    _client = the_configuration_data__->client_base_;
                    the_configuration_data__->max_assigned_client_id_ = the_configuration_data__->client_base_;
                } else {
                    _client++;
                    increase_count++;
                    if (increase_count > the_configuration_data__->max_clients_) {
                        VSOMEIP_ERROR << "Max amount of possible concurrent active"
                                << " vsomeip applications reached (" << std::dec <<
                                the_configuration_data__->max_clients_ << ").";
#ifdef _WIN32
                        BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
                        assert(releaseResult);
                        (void)releaseResult;
#else
                        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
                        return ILLEGAL_CLIENT;
                    }
                }
            }
            the_configuration_data__->max_assigned_client_id_ = _client;
        }

        if (set_client_as_manager_host) {
            the_configuration_data__->routing_manager_host_ = _client;
#ifndef _WIN32
            the_configuration_data__->pid_ = pid;
#endif
        }

        used_client_ids__[the_configuration_data__->max_used_client_ids_index_] = _client;
        the_configuration_data__->max_used_client_ids_index_++;


#ifdef _WIN32
        BOOL releaseResult = ReleaseMutex(configuration_data_mutex);
        assert(releaseResult);
        (void)releaseResult;
#else
        pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
        return _client;
    }
    return ILLEGAL_CLIENT;
}

void utility::release_client_id(client_t _client) {
    std::unique_lock<CriticalSection> its_lock(its_local_configuration_mutex__);
    if (the_configuration_data__ != nullptr) {
#ifdef _WIN32
        WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
        if (EOWNERDEAD == pthread_mutex_lock(&the_configuration_data__->mutex_)) {
            VSOMEIP_WARNING << "utility::release_client_id EOWNERDEAD";
            check_client_id_consistency();
            if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
                VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
            }
        }
#endif
        int i = 0;
        for (; i < the_configuration_data__->max_used_client_ids_index_; i++) {
            if (used_client_ids__[i] == _client) {
                break;
            }
        }

        if (i < the_configuration_data__->max_used_client_ids_index_) {
            for (; i < (the_configuration_data__->max_used_client_ids_index_ - 1); i++) {
                used_client_ids__[i] = used_client_ids__[i+1];
            }
            the_configuration_data__->max_used_client_ids_index_--;
        }
#ifdef _WIN32
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

#ifdef _WIN32
    WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
    if (EOWNERDEAD == pthread_mutex_lock(&the_configuration_data__->mutex_)) {
        VSOMEIP_WARNING << "utility::is_routing_manager_host EOWNERDEAD";
        check_client_id_consistency();
        if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
            VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
        }
    }
#endif

    bool is_routing_manager = (the_configuration_data__->routing_manager_host_ == _client);

#ifdef _WIN32
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

#ifdef _WIN32
    WaitForSingleObject(configuration_data_mutex, INFINITE);
#else
    if (EOWNERDEAD == pthread_mutex_lock(&the_configuration_data__->mutex_)) {
        VSOMEIP_WARNING << "utility::set_routing_manager_host EOWNERDEAD";
        check_client_id_consistency();
        if (0 != pthread_mutex_consistent(&the_configuration_data__->mutex_)) {
            VSOMEIP_ERROR << "pthread_mutex_consistent() failed ";
        }
    }
#endif

    the_configuration_data__->routing_manager_host_ = _client;

#ifdef _WIN32
    ReleaseMutex(configuration_data_mutex);
#else
    pthread_mutex_unlock(&the_configuration_data__->mutex_);
#endif
}


inline bool utility::get_struct_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) {
    uint32_t its_length = 0;
    bool length_field_deployed(false);
    // [TR_SOMEIP_00080] d If the length of the length field is not specified, a length of 0
    // has to be assumed and no length field is in the message.
    if (length_field_deployed) {
        if (_buffer_size >= sizeof(uint32_t)) {
            std::memcpy(&its_length, _buffer, sizeof(uint32_t));
            _length = bswap_32(its_length);
            _buffer_size -= skip_struct_length_;
            _buffer += skip_struct_length_;
            return true;
        }
    } else {
        _length = 0;
        return true;
    }

    return false;
}

inline bool utility::get_union_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) {
    uint32_t its_length = 0;

    // [TR_SOMEIP_00125] d If the Interface Specification does not specify the length of the
    // length field for a union, 32 bit length of the length field shall be used.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_length, _buffer, sizeof(uint32_t));
        _length = bswap_32(its_length);
        _buffer_size -= skip_union_length_;
        _buffer += skip_union_length_;
        return true;
    }
    return false;
}

inline bool utility::get_array_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) {
    uint32_t its_length = 0;

    // [TR_SOMEIP_00106] d The layout of arrays with dynamic length basically is based on
    // the layout of fixed length arrays. To determine the size of the array the serialization
    // adds a length field (default length 32 bit) in front of the data, which counts the bytes
    // of the array. The length does not include the size of the length field. Thus, when
    // transporting an array with zero elements the length is set to zero.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_length, _buffer, sizeof(uint32_t));
        _length = bswap_32(its_length);
        _buffer_size -= skip_array_length_;
        _buffer += skip_array_length_;
        return true;
    }
    return false;
}

inline bool utility::is_range(const byte_t* &_buffer, uint32_t &_buffer_size) {
    uint32_t its_type = 0;

    // [TR_SOMEIP_00128] If the Interface Specification does not specify the length of the
    // type field of a union, 32 bit length of the type field shall be used.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_type, _buffer, sizeof(uint32_t));
        its_type = bswap_32(its_type);
        _buffer_size -= skip_union_type_;
        _buffer += skip_union_type_;
        if (its_type == 0x02) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

inline bool utility::parse_id_item(const byte_t* &_buffer, uint32_t& parsed_ids_bytes, ranges_t& its_ranges, uint32_t &_buffer_size) {
    // get "union IdItem" length
    uint32_t iditem_length = 0;
    if (get_union_length(_buffer, _buffer_size, iditem_length)) {
        // determine type of union
        uint16_t its_first = 0;
        uint16_t its_last = 0;
        if (is_range(_buffer, _buffer_size)) {
            // get range of instance IDs "struct IdRange" length
            uint32_t range_length = 0;
            if (get_struct_length(_buffer, _buffer_size, range_length)) {
                // read first and last instance range
                if (parse_range(_buffer, _buffer_size, its_first, its_last)) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                } else {
                    return false;
                }
            }
        } else {
            // a single instance ID
            if (parse_id(_buffer, _buffer_size, its_first)) {
                if (its_first != ANY_METHOD) {
                    if (its_first != 0x00) {
                        its_last = its_first;
                        its_ranges.insert(std::make_pair(its_first, its_last));
                    }
                } else {
                    its_first = 0x01;
                    its_last = 0xFFFE;
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
        parsed_ids_bytes += (skip_union_length_type_ + iditem_length);
    }
    return true;
}

inline bool utility::parse_range(const byte_t* &_buffer, uint32_t &_buffer_size, uint16_t &_first, uint16_t &_last){
    uint16_t its_first = 0;
    uint16_t its_last = 0;

    if (_buffer_size >= sizeof(uint16_t) * 2) {
        if (parse_id(_buffer, _buffer_size, its_first)) {
            _first = its_first;
        }
        if (parse_id(_buffer, _buffer_size, its_last)) {
            _last = its_last;
        }
        if (_first != _last
                && (_first == ANY_METHOD || _last == ANY_METHOD)) {
            return false;
        }
        if (_first != 0x0 && _last != 0x00
                && _first <= _last) {
            if (_first == ANY_METHOD &&
                    _last == ANY_METHOD) {
                _first = 0x01;
                _last = 0xFFFE;
            }
            return true;
        } else {
            if (_first == 0x00 && _last > _first
                    && _last != ANY_METHOD) {
                _first = 0x01;
                return true;
            }
            return false;
        }
    }
    return false;
}

inline bool utility::parse_id(const byte_t* &_buffer, uint32_t &_buffer_size, uint16_t &_id) {
    uint16_t its_id = 0;
    if (_buffer_size >= sizeof(uint16_t)) {
        std::memcpy(&its_id, _buffer, sizeof(uint16_t));
        _id = bswap_16(its_id);
        _buffer_size -= id_width_;
        _buffer += id_width_;
        return true;
    }
    return false;
}

bool utility::parse_uid_gid(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_uid, uint32_t &_gid) {
    uint32_t its_uid = 0xffffffff;
    uint32_t its_gid = 0xffffffff;

    if (_buffer_size >= sizeof(uint32_t) * 2) {
        std::memcpy(&its_uid, _buffer, sizeof(uint32_t));
        _uid = bswap_32(its_uid);

        std::memcpy(&its_gid, _buffer + sizeof(uint32_t), sizeof(uint32_t));
        _gid = bswap_32(its_gid);

        _buffer_size -= (uid_width_ + gid_width_);
        _buffer += (uid_width_ + gid_width_);
        return true;
    }
    return false;
}

bool utility::parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_uid, uint32_t &_gid, ::std::shared_ptr<policy> &_policy) {
    uint32_t its_uid = 0xffffffff;
    uint32_t its_gid = 0xffffffff;
    bool has_error(false);

    // get user ID String
    if (parse_uid_gid(_buffer, _buffer_size, its_uid, its_gid)) {
        _uid = its_uid;
        _gid = its_gid;

        // policy elements
        std::pair<uint32_t, uint32_t> its_uid_range, its_gid_range;
        std::set<std::pair<uint32_t, uint32_t>> its_uids, its_gids;

        // fill uid and gid range
        std::get<0>(its_uid_range) = its_uid;
        std::get<1>(its_uid_range) = its_uid;
        std::get<0>(its_gid_range) = its_gid;
        std::get<1>(its_gid_range) = its_gid;
        its_uids.insert(its_uid_range);
        its_gids.insert(its_gid_range);

        _policy->ids_.insert(std::make_pair(its_uids, its_gids));
        _policy->allow_who_ = true;
        _policy->allow_what_ = true;

        // get struct AclUpdate
        uint32_t acl_length = 0;
        if (get_struct_length(_buffer, _buffer_size, acl_length)) {
            // get requests array length
            uint32_t requests_array_length = 0;
            if (get_array_length(_buffer, _buffer_size, requests_array_length)) {
                // loop through requests array consisting of n x "struct Request"
                uint32_t parsed_req_bytes = 0;
                while (parsed_req_bytes + skip_struct_length_ <= requests_array_length) {
                    // get request struct length
                    uint32_t req_length = 0;
                    if (get_struct_length(_buffer, _buffer_size, req_length)) {
                        if (req_length != 0)
                            parsed_req_bytes += skip_struct_length_;

                        uint16_t its_service_id = 0;
                        ids_t its_instance_method_ranges;
                        // get serviceID
                        if (!parse_id(_buffer, _buffer_size, its_service_id)) {
                            has_error = true;
                        } else {
                            if (its_service_id == 0x00
                                    || its_service_id == 0xFFFF) {
                                VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with service ID: 0x"
                                        << its_service_id << " is not allowed!";
                                return false;
                            }
                            // add length of serviceID
                            parsed_req_bytes += id_width_;
                        }

                        // get instances array length
                        uint32_t instances_array_length = 0;
                        if (get_array_length(_buffer, _buffer_size, instances_array_length)) {
                            // loop trough instances array consisting of n x "struct Instance"
                            uint32_t parsed_inst_bytes = 0;
                            while (parsed_inst_bytes + skip_struct_length_ <= instances_array_length) {
                                // get instance struct length
                                uint32_t inst_length = 0;
                                if (get_struct_length(_buffer, _buffer_size, inst_length)) {
                                    if (inst_length != 0)
                                        parsed_inst_bytes += skip_struct_length_;

                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    // get "IdItem[] ids" array length
                                    uint32_t ids_array_length = 0;
                                    if (get_array_length(_buffer, _buffer_size, ids_array_length)) {
                                        uint32_t parsed_ids_bytes = 0;
                                        while (parsed_ids_bytes + skip_struct_length_ <= ids_array_length) {
                                            if (!parse_id_item(_buffer, parsed_ids_bytes, its_instance_ranges, _buffer_size)) {
                                                return false;
                                            }
                                        }
                                        parsed_inst_bytes += (skip_array_length_ + ids_array_length);
                                    }
                                    // get "IdItem[] methods" array length
                                    uint32_t methods_array_length = 0;
                                    if (get_array_length(_buffer, _buffer_size, methods_array_length)) {
                                        uint32_t parsed_method_bytes = 0;
                                        while (parsed_method_bytes + skip_struct_length_ <= methods_array_length) {
                                            if (!parse_id_item(_buffer, parsed_method_bytes, its_method_ranges, _buffer_size)) {
                                                return false;
                                            }
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                        parsed_inst_bytes += (skip_array_length_ + methods_array_length);
                                    }
                                }
                            }
                            parsed_req_bytes += (skip_array_length_ + instances_array_length);
                        }
                        if (!its_instance_method_ranges.empty()) {
                            _policy->services_.insert(
                                    std::make_pair(its_service_id, its_instance_method_ranges));
                        }
                    }
                }
            }
            // get offers array length
            uint32_t offers_array_length = 0;
            if (get_array_length(_buffer, _buffer_size, offers_array_length)){
                // loop through offers array
                uint32_t parsed_offers_bytes = 0;
                while (parsed_offers_bytes + skip_struct_length_ <= offers_array_length) {
                    // get service ID
                    uint16_t its_service_id = 0;
                    ranges_t its_instance_ranges;
                    // get serviceID
                    if (!parse_id(_buffer, _buffer_size, its_service_id)) {
                        has_error = true;
                    } else {
                        if (its_service_id == 0x00
                                || its_service_id == 0xFFFF) {
                            VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with service ID: 0x"
                                    << its_service_id << " is not allowed!";
                            return false;
                        }
                        // add length of serviceID
                        parsed_offers_bytes += id_width_;
                    }

                    // get "IdItem[] ids" array length
                    uint32_t ids_array_length = 0;
                    if (get_array_length(_buffer, _buffer_size, ids_array_length)) {
                        uint32_t parsed_ids_bytes = 0;
                        while (parsed_ids_bytes + skip_struct_length_ <= ids_array_length) {
                            if (!parse_id_item(_buffer, parsed_ids_bytes, its_instance_ranges, _buffer_size)) {
                                return false;
                            }
                        }
                        parsed_offers_bytes += (skip_array_length_ + ids_array_length);
                    }
                    if (!its_instance_ranges.empty()) {
                       _policy->offers_.insert(
                               std::make_pair(its_service_id, its_instance_ranges));
                   }
                }
            }
        } else {
            VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with empty request / offer section is not allowed!";
            has_error = true;
        }
    } else {
        VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy without UID / GID is not allowed!";
        has_error = true;
    }

    if (!has_error)
        return true;
    else
        return false;
}

void utility::check_client_id_consistency() {
    if (1 < the_configuration_data__->max_used_client_ids_index_) {
        client_t prevID = used_client_ids__[0];
        int i = 1;
        for (; i < the_configuration_data__->max_used_client_ids_index_; i++) {
            const client_t currID = used_client_ids__[i];
            if (prevID == currID) {
                break;
            }
            prevID = currID;
        }

        if (i < the_configuration_data__->max_used_client_ids_index_) {
            for (; i < (the_configuration_data__->max_used_client_ids_index_ - 1); i++) {
                used_client_ids__[i] = used_client_ids__[i+1];
            }
            the_configuration_data__->max_used_client_ids_index_--;
        }
    }
}

std::uint16_t utility::get_max_number_of_clients(std::uint16_t _diagnosis_max) {
    std::uint16_t its_max_clients(0);
    const int bits_for_clients =
#ifdef _WIN32
            __popcnt(
#else
            __builtin_popcount(
#endif
                    static_cast<std::uint16_t>(~_diagnosis_max));
    for (int var = 0; var < bits_for_clients; ++var) {
        its_max_clients = static_cast<std::uint16_t>(its_max_clients | (1 << var));
    }
    return its_max_clients;
}

const uint8_t utility::uid_width_ = sizeof(uint32_t);
const uint8_t utility::gid_width_ = sizeof(uint32_t);
const uint8_t utility::id_width_ = sizeof(uint16_t);
const uint8_t utility::range_width_ = sizeof(uint32_t);

const uint8_t utility::skip_union_length_ = sizeof(uint32_t);
const uint8_t utility::skip_union_type_ = sizeof(uint32_t);
const uint8_t utility::skip_union_length_type_ = sizeof(uint32_t) + sizeof(uint32_t);
const uint8_t utility::skip_struct_length_ = sizeof(uint32_t);
const uint8_t utility::skip_array_length_ = sizeof(uint32_t);

} // namespace vsomeip
