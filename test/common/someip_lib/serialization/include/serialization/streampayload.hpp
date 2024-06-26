// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __STREAMPAYLOAD_HPP__
#define __STREAMPAYLOAD_HPP__

#include <iostream>

#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>

namespace vsomeip_utilities {
namespace serialization {

class StreamPayload {
public:
    StreamPayload() = default;

    StreamPayload(const StreamPayload &_other) {
        commit(boost::asio::buffer_copy(prepareBuffer(_other.m_payloadBuffer.size()),
                                        _other.m_payloadBuffer.data()));
    }

    const StreamPayload &operator=(const StreamPayload &_other) {
        commit(boost::asio::buffer_copy(prepareBuffer(_other.m_payloadBuffer.size()),
                                        _other.m_payloadBuffer.data()));
        return *this;
    }

    StreamPayload(const StreamPayload &&_other) {
        commit(boost::asio::buffer_copy(prepareBuffer(_other.m_payloadBuffer.size()),
                                        _other.m_payloadBuffer.data()));
    }

    const StreamPayload &operator=(const StreamPayload &&_other) {
        commit(boost::asio::buffer_copy(prepareBuffer(_other.m_payloadBuffer.size()),
                                        _other.m_payloadBuffer.data()));

        return *this;
    }

    /**
     * @brief Pushes the provided payload into the container
     *
     * @tparam T source payload type
     * @param _payload source payload
     * @return the size in bytes of the pushed data
     */
    template<typename T>
    std::size_t push(const T &_payload) {
        std::ostream os(&m_payloadBuffer);
        std::size_t processedBytes = _payload.serialize(os);

        if (processedBytes > 0) {
            commit(0);
            // TODO: This needs top be assessed, if we commit a number of processed bytes greater
            // than 0 the streambuf available bytes are set to 2x that number. Commiting 0 bytes
            // appears to workaround this behavior.
        }

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
    std::size_t pull(T &_payload) {
        std::istream is(&m_payloadBuffer);
        std::size_t processedBytes = _payload.deserialize(is);

        return processedBytes;
    }

protected:
    std::size_t streamSize() const { return m_payloadBuffer.size(); }

    /**
     * @brief Commits the next _size bytes into the input stream
     *
     * @param _size
     */
    void commit(const std::size_t _size) { m_payloadBuffer.commit(_size); }

    /**
     * @brief Consumes the nest _size bytes from the input stream
     *
     * @param _size
     */
    void consume(const std::size_t _size) { m_payloadBuffer.consume(_size); }

    /**
     * @brief Prepares the internal stream buffer with provided size
     *
     * @param _size
     * @return boost::asio::mutable_buffer Returns buffer handle
     */
    boost::asio::mutable_buffer prepareBuffer(std::size_t _size) {
        return m_payloadBuffer.prepare(_size);
    }

    /**
     * @brief Get a pointer to the first byte of the stream.
     *
     * @return unsigned char*
     */
    const unsigned char *begin() { return static_cast<const unsigned char*>(m_payloadBuffer.data().data()); };

    /**
     * @brief Get a pointer to the last byte of the stream.
     *
     * @return unsigned char*
     */
    const unsigned char *end() { return begin() + m_payloadBuffer.size(); }

    std::size_t readFromStream(std::istream &_stream, const std::size_t _size) {
        std::size_t processedBytes = 0;

        // Read into payload
        _stream.read(reinterpret_cast<char *>(prepareBuffer(_size).data()), _size);

        // if payload read successfully...
        if (!_stream.fail()) {
            // flag data as available
            commit(_size);
            processedBytes += _size;

        } else {
            std::cerr << __func__ << ": Failed to read " << _size << " bytes from stream!\n";
        }

        return processedBytes;
    }

    std::size_t writeToStream(std::ostream &_stream) const {
        std::size_t processedBytes = 0;

        // Serialize payload into stream
        _stream.write(static_cast<const char *>(m_payloadBuffer.data().data()), streamSize());

        // if payload serialization successfull, account payload bytes...
        if (!_stream.fail()) {
            processedBytes += streamSize();
        } else {
            std::cerr << __func__ << ": Failed to write " << streamSize() << " bytes from stream!\n";
        }

        return processedBytes;
    }

private:
    boost::asio::streambuf m_payloadBuffer;
};

} // someip
} // vsomeip_utilities

#endif // __STREAMPAYLOAD_HPP__
