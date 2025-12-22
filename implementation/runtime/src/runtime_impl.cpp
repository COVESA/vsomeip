// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// In libvsomeip, e.g., in runtime_impl constructor or a static constructor
#ifdef __linux__
#include <dlfcn.h>
#endif
#include <memory>
#include <mutex>
#include <vsomeip/defines.hpp>

#include "../include/application_impl.hpp"
#include "../include/runtime_impl.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/payload_impl.hpp"
#include "../../utility/include/global_state.hpp"

namespace vsomeip_v3 {
#ifdef __linux
// From https://gcc.gnu.org/onlinedocs/gcc-4.4.7/gcc/Function-Attributes.html
// The constructor attribute causes the function to be called automatically before execution enters main (). Similarly, the destructor
// attribute causes the function to be called automatically after main () has completed or exit () has been called. Functions with these
// attributes are useful for initializing data that will be used implicitly during the execution of the program.
__attribute__((constructor)) void pin_library() {
    Dl_info info;
    // Get path to self using any symbol in this library
    if (dladdr(reinterpret_cast<void*>(pin_library), &info)) {

        dlopen(info.dli_fname, RTLD_NOW | RTLD_NODELETE | RTLD_NOLOAD);
    }
    // Force global_state initialization at library load time (before main()).
    // This ensures global_state is destroyed AFTER any static application objects,
    // since static destruction follows reverse initialization order.
    (void)global_state::get();
}
#endif

std::string runtime_impl::get_property(const std::string& _name) {
    std::scoped_lock its_lock{global_state::get().properties_mutex_};
    auto& properties = global_state::get().properties_;
    auto found_property = properties.find(_name);
    if (found_property != properties.end())
        return found_property->second;
    return "";
}

void runtime_impl::set_property(const std::string& _name, const std::string& _value) {
    std::scoped_lock its_lock{global_state::get().properties_mutex_};
    global_state::get().properties_[_name] = _value;
}

std::shared_ptr<runtime> runtime_impl::get() {
    return global_state::get().runtime_;
}

std::shared_ptr<application> runtime_impl::create_application(const std::string& _name) {

    return create_application(_name, "");
}

std::shared_ptr<application> runtime_impl::create_application(const std::string& _name, const std::string& _path) {
    std::scoped_lock its_lock{applications_mutex_};
    static std::uint32_t postfix_id = 0;
    std::string its_name = _name;
    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        its_name += "_" + std::to_string(postfix_id++);
    }
    std::shared_ptr<application> application = std::make_shared<application_impl>(its_name, _path);
    applications_[its_name] = application;
    return application;
}

std::shared_ptr<message> runtime_impl::create_message(bool _reliable) const {
    auto its_message = std::make_shared<message_impl>();
    its_message->set_protocol_version(VSOMEIP_PROTOCOL_VERSION);
    its_message->set_return_code(return_code_e::E_OK);
    its_message->set_reliable(_reliable);
    its_message->set_interface_version(DEFAULT_MAJOR);
    return its_message;
}

std::shared_ptr<message> runtime_impl::create_request(bool _reliable) const {
    auto its_request = std::make_shared<message_impl>();
    its_request->set_protocol_version(VSOMEIP_PROTOCOL_VERSION);
    its_request->set_message_type(message_type_e::MT_REQUEST);
    its_request->set_return_code(return_code_e::E_OK);
    its_request->set_reliable(_reliable);
    its_request->set_interface_version(DEFAULT_MAJOR);
    return its_request;
}

std::shared_ptr<message> runtime_impl::create_response(const std::shared_ptr<message>& _request) const {
    auto its_response = std::make_shared<message_impl>();
    its_response->set_service(_request->get_service());
    its_response->set_instance(_request->get_instance());
    its_response->set_method(_request->get_method());
    its_response->set_client(_request->get_client());
    its_response->set_session(_request->get_session());
    its_response->set_interface_version(_request->get_interface_version());
    its_response->set_message_type(message_type_e::MT_RESPONSE);
    its_response->set_return_code(return_code_e::E_OK);
    its_response->set_reliable(_request->is_reliable());
    return its_response;
}

std::shared_ptr<message> runtime_impl::create_notification(bool _reliable) const {
    auto its_notification = std::make_shared<message_impl>();
    its_notification->set_protocol_version(VSOMEIP_PROTOCOL_VERSION);
    its_notification->set_message_type(message_type_e::MT_NOTIFICATION);
    its_notification->set_return_code(return_code_e::E_OK);
    its_notification->set_reliable(_reliable);
    its_notification->set_interface_version(DEFAULT_MAJOR);
    return its_notification;
}

std::shared_ptr<payload> runtime_impl::create_payload() const {
    return std::make_shared<payload_impl>();
}

std::shared_ptr<payload> runtime_impl::create_payload(const byte_t* _data, uint32_t _size) const {
    return std::make_shared<payload_impl>(_data, _size);
}

std::shared_ptr<payload> runtime_impl::create_payload(const std::vector<byte_t>& _data) const {
    return std::make_shared<payload_impl>(_data);
}

std::shared_ptr<application> runtime_impl::get_application(const std::string& _name) const {
    std::scoped_lock its_lock{applications_mutex_};
    auto found_application = applications_.find(_name);
    if (found_application != applications_.end())
        return found_application->second.lock();
    return nullptr;
}

void runtime_impl::remove_application(const std::string& _name) {
    std::scoped_lock its_lock{applications_mutex_};
    auto found_application = applications_.find(_name);
    if (found_application != applications_.end()) {
        applications_.erase(_name);
    }
}

} // namespace vsomeip_v3
