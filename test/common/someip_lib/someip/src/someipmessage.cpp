// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <someip/someipmessage.hpp>

namespace vsomeip_utilities {
namespace someip {

// CTor
SomeIpMessage::SomeIpMessage() {
    m_podPayload.length =
            calcInitialSomeIpMsgHeaderLengthAttr(); // Initialize POD's length attribute
}

SomeIpMessage::SomeIpMessage(const SomeIpMessage &&_other)
    : serialization::FragmentPayload<SomeIpHeaderRaw_t> { _other } {
}

SomeIpMessage::SomeIpMessage(const SomeIpMessage &_other)
    : serialization::FragmentPayload<SomeIpHeaderRaw_t> { _other } {
}

const SomeIpMessage &SomeIpMessage::operator=(const SomeIpMessage &_other) {
    serialization::FragmentPayload<SomeIpHeaderRaw_t>::operator=(_other);
    return *this;
}

const SomeIpMessage &SomeIpMessage::operator=(const SomeIpMessage &&_other) {
    serialization::FragmentPayload<SomeIpHeaderRaw_t>::operator=(_other);
    return *this;
}

std::size_t SomeIpMessage::payloadSize() const {
    // Calc real payload length
    // (disregard last 8 header bytes since they were already retrieved)
    return length() - 8;
}

types::serviceId_t SomeIpMessage::serviceId() const {
    return m_podPayload.serviceId;
}

void SomeIpMessage::setServiceId(const types::serviceId_t &_serviceId) {
    m_podPayload.serviceId = _serviceId;
}

void SomeIpMessage::setMethodId(const types::methodId_t &_methodId) {
    m_podPayload.methodId = _methodId;
}

types::methodId_t SomeIpMessage::methodId() const {
    return m_podPayload.methodId;
}

// This method should not be used except to force wrong values
void SomeIpMessage::setLength(const types::length_t &_length) {
    m_podPayload.length = _length;
}

types::length_t SomeIpMessage::length() const {
    return m_podPayload.length;
}

void SomeIpMessage::setClientId(const types::clientId_t &_clientId) {
    m_podPayload.clientId = _clientId;
}

types::clientId_t SomeIpMessage::clientId() const {
    return m_podPayload.clientId;
}

void SomeIpMessage::setSessionId(const types::sessionId_t &_sessionId) {
    m_podPayload.sessionId = _sessionId;
}

types::sessionId_t SomeIpMessage::sessionId() const {
    return m_podPayload.sessionId;
}

void SomeIpMessage::setProtocolVersion(const types::protocolVersion_t &_protocolVersion) {
    m_podPayload.protocolVersion = _protocolVersion;
}

types::protocolVersion_t SomeIpMessage::protocolVersion() const {
    return m_podPayload.protocolVersion;
}

void SomeIpMessage::setInterfaceVersion(const types::interfaceVersion_t &_interfaceVersion) {
    m_podPayload.interfaceVersion = _interfaceVersion;
}

types::interfaceVersion_t SomeIpMessage::interfaceVersion() const {
    return m_podPayload.interfaceVersion;
}

void SomeIpMessage::setMsgType(const types::messageType_e &_msgType) {
    m_podPayload.msgType = _msgType;
}

types::messageType_e SomeIpMessage::msgType() const {
    return m_podPayload.msgType;
}

void SomeIpMessage::setRetCode(const types::returnCode_e &_retCode) {
    m_podPayload.retCode = _retCode;
}

types::returnCode_e SomeIpMessage::retCode() const {
    return m_podPayload.retCode;
}

void SomeIpMessage::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
              << "\n\tPOD size: " << serialization::PodPayload<SomeIpHeaderRaw_t>::size()
              << "\n\t--\n\tlength: " << length() << '\n'
              << std::hex << std::showbase << "\tserviceId: " << unsigned(serviceId())
              << "\n\tmethodId: " << unsigned(methodId())
              << "\n\tclientId: " << unsigned(clientId())
              << "\n\tretCode: " << unsigned(retCode())
              << "\n\tmsgType: " << unsigned(msgType())
              << "\n\tsessionId: " << unsigned(sessionId())
              << "\n\tprotocolVersion: " << unsigned(protocolVersion())
              << "\n\tinterfaceVersion: " << unsigned(interfaceVersion())
              << "\n\t--" << std::dec
              << "\n\tPayload Size: " << unsigned(payloadSize()) << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

std::vector<std::shared_ptr<SomeIpMessage>> SomeIpMessage::readSomeIpMessages(boost::asio::ip::udp::socket *_udp_socket,
        const int _expected_stream_size, bool& _keep_receiving) {
    // Create a buffer to receive incoming messages from the socket.
    std::vector<std::uint8_t> receive_buffer(_expected_stream_size);

    boost::system::error_code error;

    // Read messages from the udp socket and store them in the uint8_t vector.
    std::size_t bytes_transferred = _udp_socket->receive(
            boost::asio::buffer(receive_buffer, receive_buffer.capacity()), 0, error);

    if (error) {
        _keep_receiving = false;
        std::cout << __func__ << " error: " << error.message();
    }

    // Vector of SOMEIP messages.
    std::vector<std::shared_ptr<SomeIpMessage>> messages;

    // Create a stream buffer.
    std::shared_ptr<boost::asio::streambuf> pStreambuf = std::make_shared<boost::asio::streambuf>();
    pStreambuf->prepare(_expected_stream_size);

    // Create an io stream to deserialize incoming messages with someip_lib.
    std::iostream ios(pStreambuf.get());

    // Feed _bytes to the io stream.
    for (int i = 0; i < int(bytes_transferred); i++) {
        ios << receive_buffer[i];
    }

    // Extract all the messages in the stream.
    std::size_t messageLengthSum = 0;
    while (messageLengthSum < bytes_transferred) {
        // Extract a Some-IP message from the stream.
        auto message = std::make_shared<vsomeip_utilities::someip::SomeIpMessage>();
        const std::size_t msgLength = message->deserialize(ios);
        messages.push_back(message);
        messageLengthSum += msgLength;
    }

    return messages;
}

void SomeIpMessage::sendSomeIpMessage(boost::asio::ip::udp::socket *_udp_socket , boost::asio::ip::udp::socket::endpoint_type _target, SomeIpMessage _message) {
    std::size_t size_sub_msg = 0;
    size_sub_msg += _message.actualSize();
    auto stream_buf_sub = std::make_shared<boost::asio::streambuf>();
    stream_buf_sub->prepare(size_sub_msg);
    std::ostream os_sub(stream_buf_sub.get());
    _message.serialize(os_sub);

    // Send a subscription message.
    _udp_socket->send_to(boost::asio::buffer(stream_buf_sub->data()), _target);
}

SomeIpMessageBuilder SomeIpMessage::create() {
    return SomeIpMessageBuilder();
}

SomeIpMessageBuilder::operator SomeIpMessage() const {
    return std::move(m_entry);
}

SomeIpMessageBuilder &SomeIpMessageBuilder::serviceId(const types::serviceId_t _service) {
    m_entry.setServiceId(_service);
    return *this;
}

SomeIpMessageBuilder &SomeIpMessageBuilder::methodId(const types::methodId_t _method) {
    m_entry.setMethodId(_method);
    return *this;
}

SomeIpMessageBuilder &SomeIpMessageBuilder::clientId(const types::clientId_t _client) {
    m_entry.setClientId(_client);
    return *this;
}

SomeIpMessageBuilder &SomeIpMessageBuilder::sessionId(const types::sessionId_t _session) {
    m_entry.setSessionId(_session);
    return *this;
}

SomeIpMessageBuilder &
SomeIpMessageBuilder::protocolVersion(const types::protocolVersion_t _protoVer) {
    m_entry.setProtocolVersion(_protoVer);
    return *this;
}

SomeIpMessageBuilder &
SomeIpMessageBuilder::interfaceVersion(const types::interfaceVersion_t _ifaceVer) {
    m_entry.setInterfaceVersion(_ifaceVer);
    return *this;
}

SomeIpMessageBuilder &SomeIpMessageBuilder::messageType(const types::messageType_e _msgType) {
    m_entry.setMsgType(_msgType);
    return *this;
}

SomeIpMessageBuilder &SomeIpMessageBuilder::returnCode(const types::returnCode_e _retCode) {
    m_entry.setRetCode(_retCode);
    return *this;
}

} // namespace someip
} // namespace vsomeip_utilities
