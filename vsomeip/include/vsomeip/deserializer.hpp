//
// deserializer.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DESERIALIZER_HPP
#define VSOMEIP_DESERIALIZER_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/message_base.hpp>
#include <vector>

namespace vsomeip {
/// \interface deserializer
/// Interface to deserialize a Some/IP message from a given stream.
class deserializer {
public:
	virtual ~deserializer() {};

	/// Creates a message from the current data stream. The maximum number of
	/// bytes that is used can be limited by #set_remaining.
	/// \returns Pointer to the deserialized message or 0 if deserialization
	/// failed.
	virtual message_base * deserialize_message() = 0;

	/// Deserializes the next byte from the current data stream to value.
	/// On a successful return the current read position is increased by 1.
	/// \param _value reference to the variable that is used to store the result
	/// \returns true, if the derserialization succeeded, false if it failed.
	virtual bool deserialize(uint8_t& _value) = 0;
	/// Deserializes the next two bytes from the current data stream to value.
	/// On a successful return the current read position is increased by 2.
	/// \param _value reference to the variable that is used to store the result
	/// \returns true, if the derserialization succeeded, false if it failed.
	virtual bool deserialize(uint16_t& _value) = 0;
	/// Deserializes the next three or four bytes from the current data stream
	/// and copies it to value. On a successful return the current read position
	/// is increased by 3 or 4, depending on #_omitLastByte.
	/// \param _value reference to the variable that is used to store the result
	/// \param _omit_last_byte set to true in order to serialize three bytes only
	/// \returns true, if the derserialization succeeded, false if it failed.
	virtual bool deserialize(uint32_t& _value, bool _omit_last_byte = false) = 0;
	/// Deserializes the next two bytes from the current data stream to value.
	/// The current read position is shifted by #_length elements.
	/// \param _data pointer to the buffer that is used to store the result
	/// \param _length the number of bytes to deserialize
	/// \returns true, if the derserialization succeeded, false if it failed.
	virtual bool deserialize(uint8_t *_data, std::size_t _length) = 0;
	/// Deserializes the next two bytes from the current data stream to value.
	/// \param _value reference to the variable that is used to store the
	/// result. The number of deserialized bytes is determined by the capacity
	/// of _value.
	/// \returns true, if the derserialization succeeded, false if it failed.
	virtual bool deserialize(std::vector<uint8_t>& _value) = 0;

	/// Reads the byte at #_index bytes ahead of the current position and stores it
	/// in #_value.
	/// \param _index Offset to the current position
	/// \value Reference to variable that is used to store the result
	/// \returns True if the byte could be read, false otherwise
	virtual bool look_ahead(std::size_t _index, uint8_t &_value) const = 0;
	/// Reads the word at #_index bytes ahead of the current position and stores it
	/// in #_value.
	/// \param _index Offset to the current position
	/// \value Reference to variable that is used to store the result
	/// \returns True if the byte could be read, false otherwise
	virtual bool look_ahead(std::size_t _index, uint16_t &_value) const = 0;
	/// Reads the long at #_index bytes ahead of the current position and stores it
	/// in #_value.
	/// \param _index offset to the current position
	/// \value reference to variable that is used to store the result
	/// \returns true if the byte could be read, false otherwise
	virtual bool look_ahead(std::size_t _index, uint32_t &_value) const = 0;

	/// Returns the number of available bytes in the current data stream.
	/// \returns Number of available bytes
	virtual std::size_t get_available() const = 0;
	/// Returns the number of remaining bytes in the current data stream. This
	/// number decreases with each call to #deserialize but is unchanged after
	/// call to #look_ahead.
	/// \returns Number of remaining bytes
	virtual std::size_t get_remaining() const = 0;
	/// Sets the number of remaining bytes. This allows to partially evaluate
	/// a data input stream, which particulary needed whenever data packet
	/// contain more than one Some/IP message.
	/// \param Number of remaining bytes.
	virtual void set_remaining(std::size_t _remaining) = 0;

	/// Set the deserializers data input stream.
	/// \param _data Pointer to the input stream.
	/// \param _length Number of bytes in the input stream.
	virtual void set_data(uint8_t *_data, std::size_t _length) = 0;
	/// Append data to the current input stream.
	/// \param _data Pointer to the input stream.
	/// \param _length Number of bytes in the input stream.
	virtual void append_data(const uint8_t *_data, std::size_t _length) = 0;
	/// Drop a number of bytes from the current input stream.
	/// \param _length number of bytes to drop
	virtual void drop_data(std::size_t _length) = 0;

	/// Clear internal settings. After a call, the deserializer is unassigned to
	/// any data input stream.
	virtual void reset() = 0;

	/// Debugging only.
	/// Intentionally left undocumented.
	virtual void show_data() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_DESERIALIZER_HPP
