//
// serializer.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERIALIZER_HPP
#define VSOMEIP_SERIALIZER_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

/// \interface serializer
/// Offers methods to serialize Some/IP messages to a continous buffer. The 
/// buffer can be created within the serializer or be created externally and
/// be provided to the serializer. 
class serializer {
public:
	virtual ~serializer() {};

    /// Serialize an object which implements the serializable interface.  
    /// \param _from the object that shall be serialized
	virtual bool serialize(const serializable *_from) = 0;

    /// Serialize a byte.
    /// \param _value value to serialize
	virtual bool serialize(const uint8_t _value) = 0;

    /// Serialize a word (2-byte).
    /// \param _value value to serialize
	virtual bool serialize(const uint16_t _value) = 0;

    /// (Partly) serialize a long (4-byte).
    /// \param _value value to serialize
    /// \param _omit_last_byte if set, the last byte will not be serialized
	virtual bool serialize(const uint32_t _value, bool _omit_last_byte = false) = 0;

    /// Serialize a byte.
    /// \param _value value of the byte to serialize
	virtual bool serialize(const uint8_t *_data, uint32_t _length) = 0;

    /// Access the internal buffer of the serializer.
    /// \returns Pointer to the internal buffer of the serializer
	virtual uint8_t * get_data() const = 0;
    
    /// Get the capacity (maximum length of the serialized data) of the serializer
    /// \returns Capacity of the serializer
	virtual uint32_t get_capacity() const = 0;

    /// Get the size (current amount of serialized bytes) of the serializer
    /// \returns Number of serialized data bytes.	
    virtual uint32_t get_size() const = 0;

    /// Create a buffer of the given length. An existing internal buffer will be dropped.
    /// \param _capacity Maximum size of the internal buffer
	virtual void create_data(uint32_t _capacity) = 0;
    
    /// Assign an existing data buffer to the serialize. 
    /// \param _data Pointer to the beginning of the buffer
    /// \param _capacity Maximum available number of bytes within the buffer
    /// \warning The assigned buffer will be overwritten but it won´t be deleted by the
    /// serializer.
 	virtual void set_data(uint8_t *_data, uint32_t _capacity) = 0;

    /// Reset the serializer. All serialized data will be dropped.
    ///
	virtual void reset() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZER_HPP
