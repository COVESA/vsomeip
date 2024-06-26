// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SOMEIPMESSAGE_HPP__
#define __SOMEIPMESSAGE_HPP__

#include <type_traits>

#include <boost/asio.hpp>

#include <serialization/fragmentpayload.hpp>
#include <someip/types/primitives.hpp>

namespace vsomeip_utilities {
namespace someip {

// Inverted field representation to align with Big-Endian byte order
struct SomeIpHeaderRaw_t {
    types::returnCode_e retCode { vsomeip_utilities::someip::types::returnCode_e::E_RC_UNKNOWN };
    types::messageType_e msgType { vsomeip_utilities::someip::types::messageType_e::E_MT_UNKNOWN };
    types::interfaceVersion_t interfaceVersion { 0 };
    types::protocolVersion_t protocolVersion { 0 };
    types::sessionId_t sessionId { 0 };
    types::clientId_t clientId { 0 };
    types::length_t length { 0 };
    types::methodId_t methodId { 0 };
    types::serviceId_t serviceId { 0 };
};

/**
 * @brief Returns the actual SomeIP length to be placed in the header
 * (disregard last 8 header bytes since they are retrieved together)
 *
 * @return constexpr std::size_t
 */
constexpr std::size_t calcInitialSomeIpMsgHeaderLengthAttr() {
    return sizeof(SomeIpHeaderRaw_t) - 8;
}

class SomeIpMessageBuilder;

/**
 * @brief Some/IP Message representation
 *
 */
class SomeIpMessage : public serialization::FragmentPayload<SomeIpHeaderRaw_t> {
    friend class SomeIpMessageBuilder;

public:
    // CTor
    SomeIpMessage();

    // Copy/Move CTors
    SomeIpMessage(const SomeIpMessage &);
    SomeIpMessage(const SomeIpMessage &&);

    // Copy/Move assignment operators
    const SomeIpMessage &operator=(const SomeIpMessage &);
    const SomeIpMessage &operator=(const SomeIpMessage &&);

    /**
     * @brief Returns the payload defined size (as referenced in the header)
     *
     * @return std::size_t
     */
    std::size_t payloadSize() const override;

    /**
     * @brief Create Message Builder
     *
     * @return SomeIpMessageBuilder
     */
    static SomeIpMessageBuilder create();

    // Message Attribute Getters
    types::length_t length() const;
    types::methodId_t methodId() const;
    types::clientId_t clientId() const;
    types::returnCode_e retCode() const;
    types::messageType_e msgType() const;
    types::serviceId_t serviceId() const;
    types::sessionId_t sessionId() const;
    types::protocolVersion_t protocolVersion() const;
    types::interfaceVersion_t interfaceVersion() const;

    // Message Attribute Setters
    void setLength(const types::length_t &_length);
    void setMethodId(const types::methodId_t &_methodId);
    void setClientId(const types::clientId_t &_clientId);
    void setRetCode(const types::returnCode_e &_retCode);
    void setMsgType(const types::messageType_e &_msgType);
    void setServiceId(const types::serviceId_t &_serviceId);
    void setSessionId(const types::sessionId_t &_sessionId);
    void setProtocolVersion(const types::protocolVersion_t &_protocolVersion);
    void setInterfaceVersion(const types::interfaceVersion_t &_interfaceVersion);

    /**
     * @brief Reads SOMEIP messages from a udp socket.
     *
     * @param _udp_socket udp socket over which to receive the messages
     * @param _expected_stream_size desired buffer size
     * @param _keep_receiving& boolean
     */
    static std::vector<std::shared_ptr<SomeIpMessage>> readSomeIpMessages(boost::asio::ip::udp::socket *_udp_socket, const int _expected_stream_size, bool& _keep_receiving);

    /**
     * @brief Sends SOMEIP message over udp socket.
     *
     * @param _udp_socket udp socket over which to send the message
     * @param _target udp endpoint
     * @param _message someip message to send
     */
    static void sendSomeIpMessage(boost::asio::ip::udp::socket *_udp_socket , boost::asio::ip::udp::socket::endpoint_type _target_sd, SomeIpMessage _message);

    /**
     * @brief Pushes the provided payload into the container
     *
     * @tparam T source payload type
     * @param _payload source payload
     * @return the size in bytes of the pushed data
     */
    template<typename T>
    std::size_t push(const T &_payload) {
        // Push payload and update POD length attribute
        std::size_t processedBytes = FragmentPayload<SomeIpHeaderRaw_t>::push(_payload);
        m_podPayload.length += static_cast<std::uint32_t>(processedBytes);

        return processedBytes;
    }

    /**
     * @brief Populates the provided entry from the payload's next memory location
     *
     * @tparam T target payload type
     * @param _payload target payload
     * @return the size in bytes of the pulled data
     */
    template<typename T>
    std::size_t pull(T &_outPayload) {
        // Pull payload and update POD length attribute
        std::size_t processedBytes = FragmentPayload<SomeIpHeaderRaw_t>::pull(_outPayload);
        m_podPayload.length -= static_cast<std::uint32_t>(processedBytes);

        return processedBytes;
    }

    /**
     * @brief Print to stdout utility method
     *
     */
    void print() const override;
};

/**
 * @brief Message Header builder
 *
 */
class SomeIpMessageBuilder {
public:
    // CTor/DTor
    SomeIpMessageBuilder() = default;
    virtual ~SomeIpMessageBuilder() = default;

    operator SomeIpMessage() const;

    // Setter Methods
    SomeIpMessageBuilder &serviceId(const types::serviceId_t _service);
    SomeIpMessageBuilder &methodId(const types::methodId_t _method);
    SomeIpMessageBuilder &clientId(const types::clientId_t _client);
    SomeIpMessageBuilder &sessionId(const types::sessionId_t _session);
    SomeIpMessageBuilder &protocolVersion(const types::protocolVersion_t _protoVer);
    SomeIpMessageBuilder &interfaceVersion(const types::interfaceVersion_t _ifaceVer);
    SomeIpMessageBuilder &messageType(const types::messageType_e _msgType);
    SomeIpMessageBuilder &returnCode(const types::returnCode_e _retCode);

    template<typename T>
    SomeIpMessageBuilder &push(T &_payload) {
        m_entry.push(_payload);
        return *this;
    }

    template<typename T>
    SomeIpMessageBuilder &push(T &&_payload) {
        auto p(std::move(_payload));
        m_entry.push(p);
        return *this;
    }

private:
    SomeIpMessage m_entry;
};

} // someip
} // vsomeip_utilities

#endif // __SOMEIPMESSAGE_HPP__
