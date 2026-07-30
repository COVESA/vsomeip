// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <memory>
#include <mutex>
#include <vsomeip/defines.hpp>

#include "../include/application_impl.hpp"
#include "../include/runtime_impl.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/payload_impl.hpp"
#include "../../plugin/include/plugin_manager_impl.hpp"
#include "vsomeip/internal/logger.hpp"

namespace vsomeip_v3 {

std::string runtime_impl::get_property(const std::string& _name) {
    auto its_runtime = std::static_pointer_cast<runtime_impl>(get());
    std::scoped_lock its_lock{its_runtime->properties_mutex_};
    auto found_property = its_runtime->properties_.find(_name);
    if (found_property != its_runtime->properties_.end()) {
        return found_property->second;
    }
    return "";
}

void runtime_impl::set_property(const std::string& _name, const std::string& _value) {
    auto its_runtime = std::static_pointer_cast<runtime_impl>(get());
    std::scoped_lock its_lock{its_runtime->properties_mutex_};
    its_runtime->properties_[_name] = _value;
}

std::shared_ptr<runtime> runtime_impl::get() {
    static std::shared_ptr<runtime> the_runtime_ = std::make_shared<runtime_impl>();
    return the_runtime_;
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
    message_header_impl its_header;
    its_header.protocol_version_ = VSOMEIP_PROTOCOL_VERSION;
    its_header.interface_version_ = DEFAULT_MAJOR;
    its_header.code_ = return_code_e::E_OK;
    return std::make_shared<message_impl>(its_header, _reliable, 0x0 /* check_result */);
}

std::shared_ptr<message> runtime_impl::create_request(bool _reliable) const {
    message_header_impl its_header;
    its_header.protocol_version_ = VSOMEIP_PROTOCOL_VERSION;
    its_header.interface_version_ = DEFAULT_MAJOR;
    its_header.type_ = message_type_e::MT_REQUEST;
    its_header.code_ = return_code_e::E_OK;
    return std::make_shared<message_impl>(its_header, _reliable, 0x0 /* check_result */);
}

std::shared_ptr<message> runtime_impl::create_response(const std::shared_ptr<message>& _request) const {
    // protocol_version is intentionally left at the header default, mirroring
    // the previous behavior which never set it on responses.
    message_header_impl its_header;
    its_header.service_ = _request->get_service();
    its_header.instance_ = _request->get_instance();
    its_header.method_ = _request->get_method();
    its_header.client_ = _request->get_client();
    its_header.session_ = _request->get_session();
    its_header.interface_version_ = _request->get_interface_version();
    its_header.type_ = message_type_e::MT_RESPONSE;
    its_header.code_ = return_code_e::E_OK;
    return std::make_shared<message_impl>(its_header, _request->is_reliable(), 0x0 /* check_result */);
}

std::shared_ptr<message> runtime_impl::create_notification(bool _reliable) const {
    message_header_impl its_header;
    its_header.protocol_version_ = VSOMEIP_PROTOCOL_VERSION;
    its_header.interface_version_ = DEFAULT_MAJOR;
    its_header.type_ = message_type_e::MT_NOTIFICATION;
    its_header.code_ = return_code_e::E_OK;
    return std::make_shared<message_impl>(its_header, _reliable, 0x0 /* check_result */);
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
    if (found_application != applications_.end()) {
        return found_application->second.lock();
    }
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
