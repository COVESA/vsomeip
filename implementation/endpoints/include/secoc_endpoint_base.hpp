// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef IMPLEMENTATION_ENDPOINTS_INCLUDE_SECOC_ENDPOINT_BASE_HPP_
#define IMPLEMENTATION_ENDPOINTS_INCLUDE_SECOC_ENDPOINT_BASE_HPP_

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/
#include <ara/sec/crypto/signer.h>
#include <ara/sec/crypto/verifier.h>
#include <ara/sec/secoc/data_id.h>
#include <ara/sec/secoc/runtime.h>
#include <ara/sec/secoc/pdu_context.h>
#include <mbedtls/md.h>
#include <mutex>
#include <memory>
#include <vsomeip/defines.hpp>

#include "buffer.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip {

class secoc_endpoint_base {
public:
    secoc_endpoint_base(uint16_t _port, std::shared_ptr<configuration> _configuration);
    virtual ~secoc_endpoint_base();

protected:
    constexpr static uint8_t mac_size = 32;
    constexpr static uint16_t mac_bit = mac_size * 8;
    constexpr static uint8_t freshness_size = 8;
    constexpr static uint8_t freshness_bit = freshness_size * 8;
    constexpr static uint16_t trailer_size = mac_size + freshness_size;
    constexpr static uint8_t max_verify_attempts = 16;

    bool is_secured(service_t _service, instance_t _instance, method_t _method);
    bool authenticate(const uint8_t *_data, uint32_t _size, service_t _service, instance_t _instance, method_t _method, message_buffer_t& _buffer);
    bool verify(const uint8_t *_data, uint32_t _size, service_t _service, instance_t _instance, method_t _method,
                message_buffer_t& _buffer);

    class Signer : public ara::sec::crypto::Signer {
     public:
        Signer();
        virtual ~Signer();
        ara::sec::crypto::Key const& GetKey() const override;

        void Start(ara::sec::crypto::CipherParameters*) override;
        void Update(const ara::sec::Span<std::uint8_t>&) override;
        void Finish(ara::sec::Span<std::uint8_t>&) override;
        void Process(const ara::sec::Span<std::uint8_t>&, ara::sec::Span<std::uint8_t>&) override;
        std::size_t GetTagSize() const override;
     private:
        mbedtls_md_context_t context_;
    };

    class Verifier : public ara::sec::crypto::Verifier {
     public:
        Verifier();
        virtual ~Verifier();
        virtual ara::sec::crypto::Key const& GetKey() const override;

        void Start(ara::sec::crypto::CipherParameters*) override;
        void Update(const ara::sec::Span<std::uint8_t>&) override;
        bool Finish(const ara::sec::Span<std::uint8_t>&, std::size_t length) override;
        bool Process(const ara::sec::Span<std::uint8_t>&, const ara::sec::Span<std::uint8_t>&, std::size_t length) override;
     private:
        mbedtls_md_context_t context_;
        std::vector<std::uint8_t> buffer_;
    };
private:
    ara::sec::secoc::DataId create_data_id(service_t _service, instance_t _instance, method_t _method);
    ara::sec::secoc::DataId create_data_id(const uint8_t *_data, uint32_t _size, instance_t _instance);
    ara::sec::secoc::Runtime::PduContextPointer get_pdu_context(const ara::sec::secoc::DataId& _data_id);
    void create_pdu_context(const ara::sec::secoc::DataId& _data_id);

    struct DataIdComparator {
        bool operator()(const ara::sec::secoc::DataId& lhs, const ara::sec::secoc::DataId& rhs) const {
            return lhs.length != rhs.length || lhs.value < rhs.value;
        }
    };

    uint16_t port_;
    std::shared_ptr<configuration> configuration_;
    Verifier verifier_;
    Signer signer_;
    std::map<ara::sec::secoc::Runtime::PduContextPointer, std::mutex> context_mutex_;
    ara::sec::secoc::Runtime::RuntimePointer secoc_runtime_;
    std::map<ara::sec::secoc::DataId, ara::sec::secoc::Runtime::PduContextPointer, DataIdComparator> pdu_contexts_;
    std::map<ara::sec::secoc::DataId, message_buffer_t, DataIdComparator> mac_workspaces_;
};

}  // namespace vsomeip

#endif  // IMPLEMENTATION_ENDPOINTS_INCLUDE_SECOC_ENDPOINT_BASE_HPP_
