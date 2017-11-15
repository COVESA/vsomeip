// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <memory>
#include <string>
#include <sstream>
#include <vector>

#include <ara/sec/secoc/counter_based_freshness.h>
#include <ara/sec/secoc/protocol.h>
#include <ara/sec/span.h>
#include <vsomeip/defines.hpp>

#include "../../interface/vsomeip/constants.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../logging/include/logger.hpp"
#include "../include/secoc_endpoint_base.hpp"

namespace {
    using CounterBasedFreshness = ara::sec::secoc::CounterBasedFreshness;
    using DataId = ara::sec::secoc::DataId;
    using Protocol = ara::sec::secoc::Protocol;
    using Runtime = ara::sec::secoc::Runtime;
}

namespace vsomeip {

secoc_endpoint_base::secoc_endpoint_base(uint16_t _port, std::shared_ptr<configuration> _configuration) :
                port_(_port), configuration_(_configuration), secoc_runtime_(Runtime::GetRuntime()) {
    auto events = configuration_->get_secured_multicast_events(_port);
    for (const auto& entry : events) {
        auto data_id = create_data_id(std::get<0>(entry), std::get<1>(entry), std::get<2>(entry));
        create_pdu_context(data_id);

        if (std::get<0>(entry) != VSOMEIP_SD_SERVICE) {
            auto context = get_pdu_context(data_id);
            data_id = create_data_id(std::get<0>(entry), ANY_INSTANCE, std::get<2>(entry));

            if (pdu_contexts_.find(data_id) == pdu_contexts_.end()) {
                pdu_contexts_[data_id] = context;
            }
        }
    }
}

secoc_endpoint_base::~secoc_endpoint_base() {}

bool secoc_endpoint_base::is_secured(service_t _service, instance_t _instance, method_t _method) {
    return pdu_contexts_.find(create_data_id(_service, _instance, _method)) != pdu_contexts_.end();
}

bool secoc_endpoint_base::authenticate(const uint8_t *_data, uint32_t _size, service_t _service, instance_t _instance,
                                       method_t _method, message_buffer_t& _buffer) {
    auto context = get_pdu_context(create_data_id(_service, _instance, _method));
    assert((_size + trailer_size) > _size);
    _buffer.resize(_size + trailer_size);
    Protocol::SecuredPdu spdu{_buffer};

    std::error_code ec{};
    {
        std::lock_guard<std::mutex> lock{context_mutex_[context]};
        ec = secoc_runtime_->GetProtocol()->Authenticate(
                        Protocol::AuthenticPdu(const_cast<byte_t *>(_data), _size),
                        *context,
                        spdu);
    }

    return !ec;
}

bool secoc_endpoint_base::verify(const uint8_t *_data, uint32_t _size, service_t _service, instance_t _instance,
                                 method_t _method, message_buffer_t& _buffer) {
    auto context = get_pdu_context(create_data_id(_service, _instance, _method));
    assert((_size - trailer_size) < _size);
    _buffer.resize(_size - trailer_size);
    Protocol::AuthenticPdu apdu{_buffer};

    std::error_code ec{};
    {
        std::lock_guard<std::mutex> lock{context_mutex_[context]};
        ec = secoc_runtime_->GetProtocol()->Verify(
                        Protocol::SecuredPdu(const_cast<byte_t *>(_data), _size),
                        *context,
                        apdu);
    }

    return !ec;
}

Runtime::PduContextPointer secoc_endpoint_base::get_pdu_context(const DataId& _data_id) {
    if (pdu_contexts_.find(_data_id) != pdu_contexts_.end()) {
        return pdu_contexts_[_data_id];
    } else {
        throw std::logic_error("Unexpected data id provided!");
    }
}

void secoc_endpoint_base::create_pdu_context(const DataId& _data_id) {
    if (pdu_contexts_.find(_data_id) == pdu_contexts_.end()) {
        mac_workspaces_[_data_id] = std::vector<uint8_t>(mac_size);
        pdu_contexts_[_data_id] = secoc_runtime_->CreatePduContext(
                        _data_id,
                        signer_,
                        verifier_,
                        std::unique_ptr<CounterBasedFreshness>(new CounterBasedFreshness(freshness_bit)),
                        ara::sec::Span<uint8_t>(mac_workspaces_[_data_id]),
                        mac_bit,
                        freshness_bit,
                        max_verify_attempts);
    }
}

DataId secoc_endpoint_base::create_data_id(service_t _service, instance_t _instance, method_t _method) {
    return DataId{((std::uint64_t(_service) << 32) | (_method << 16) | _instance), 48};
}

/**
 * secoc_endpoint_base::Signer
 */
secoc_endpoint_base::Signer::Signer() {
    mbedtls_md_init(&context_);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(mbedtls_md_type_t::MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&context_, info, 0) != 0) {
        throw std::runtime_error("Error trying to set up message digest context");
    }
}

secoc_endpoint_base::Signer::~Signer() {
    mbedtls_md_free(&context_);
}

ara::sec::crypto::Key const& secoc_endpoint_base::Signer::GetKey() const {
    throw std::logic_error("Not supported");
}

std::size_t secoc_endpoint_base::Signer::GetTagSize() const {
    return static_cast<std::size_t>(mbedtls_md_get_size(context_.md_info));
}

void secoc_endpoint_base::Signer::Start(ara::sec::crypto::CipherParameters*) {
    if (mbedtls_md_starts(&context_) != 0) {
        throw std::runtime_error("Error trying to start message authentication");
    }
}

void secoc_endpoint_base::Signer::Update(const ara::sec::Span<std::uint8_t>& _data_in) {
    if (mbedtls_md_update(&context_, _data_in.data(), _data_in.size()) != 0) {
        throw std::runtime_error("Error trying to update message authentication code");
    }
}

void secoc_endpoint_base::Signer::Finish(ara::sec::Span<std::uint8_t>& _mac_out) {
    assert(_mac_out.size() >= mbedtls_md_get_size(context_.md_info));
    if (mbedtls_md_finish(&context_, _mac_out.data()) != 0) {
        throw std::runtime_error("Error trying to finish message authentication");
    }
}

void secoc_endpoint_base::Signer::Process(const ara::sec::Span<std::uint8_t>& _data_in, ara::sec::Span<std::uint8_t>& _mac_out) {
    Start(nullptr);
    Update(_data_in);
    Finish(_mac_out);
}

/**
 * secoc_endpoint_base::Verifier
 */
secoc_endpoint_base::Verifier::Verifier() {
    mbedtls_md_init(&context_);
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(mbedtls_md_type_t::MBEDTLS_MD_SHA256);
    if (mbedtls_md_setup(&context_, info, 0) != 0) {
        throw std::runtime_error("Error trying to set up message digest context");
    }

    buffer_.resize(static_cast<std::size_t>(mbedtls_md_get_size(info)));
}

secoc_endpoint_base::Verifier::~Verifier() {
    mbedtls_md_free(&context_);
}

ara::sec::crypto::Key const& secoc_endpoint_base::Verifier::GetKey() const {
    throw std::logic_error("Not supported");
}

void secoc_endpoint_base::Verifier::Start(ara::sec::crypto::CipherParameters*) {
    if (mbedtls_md_starts(&context_) != 0) {
        throw std::runtime_error("Error trying to start message authentication");
    }
}

void secoc_endpoint_base::Verifier::Update(const ara::sec::Span<std::uint8_t>& _data_in) {
    if (mbedtls_md_update(&context_, _data_in.data(), _data_in.size()) != 0) {
        throw std::runtime_error("Error trying to update message authentication code");
    }
}

bool secoc_endpoint_base::Verifier::Finish(const ara::sec::Span<std::uint8_t>& authenticator, std::size_t length) {
    // non byte aligned lengths not supported
    assert(!(length % 8));

    std::size_t byte_length = length / 8;
    assert(byte_length <= static_cast<std::size_t>(authenticator.size()));
    assert(byte_length <= buffer_.size());

    if (mbedtls_md_finish(&context_, buffer_.data()) != 0) {
        throw std::runtime_error("Error trying to finish message authentication");
    }
    return std::equal(authenticator.begin(), authenticator.begin() + byte_length, buffer_.begin());
}

bool secoc_endpoint_base::Verifier::Process(const ara::sec::Span<std::uint8_t>& _data_in,
                                            const ara::sec::Span<std::uint8_t>& _auth_in, std::size_t length) {
    Start(nullptr);
    Update(_data_in);
    return Finish(_auth_in, length);
}

}  // namespace vsomeip
