//
// payload.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PAYLOAD_HPP
#define VSOMEIP_PAYLOAD_HPP

#include <vector>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
/// Interface for the payload of a Some/IP message. Contains method
/// to set the data within the payload. 
class payload {
public:
    /// Get the internal data buffer of the payload object.
    /// \returns Pointer to the internal data buffer
	virtual uint8_t * get_data() = 0;

	/// Get a read-only pointer to the internal data
	virtual const uint8_t * get_data() const = 0;
    
    /// Get the length of the internal data buffer of the payload object. The
    /// length is the valid part of the internal buffer and is not necessarily
    /// the same as the buffers capacity.
    /// \returns Length of the internal data buffer
	virtual uint32_t get_length() const = 0;
	
    /// Set the maximum number of bytes the internal buffer can contain. 
    /// \param _length New capacity of the internal buffer.
    /// \warning If the new length is smaller than the current length, data will be lost.
    virtual void set_capacity(uint32_t _length) = 0;
    
    /// Set the values of the internal data buffer of a payload object. Copies
    /// #_length bytes from #_data to the internal buffer.
    /// \param _data Pointer to the source data buffer
    /// \param _length Number of bytes that will be copied from #_data
	virtual void set_data(const uint8_t *_data, uint32_t _length) = 0;
    
	/// Set the values of the internal data buffer of a payload object. Copies
    /// #_data.size() bytes from #_data to the internal buffer.
    /// \param _data Reference to the source data buffer.
	virtual void set_data(const std::vector<uint8_t>& _data) = 0;

protected:
    /// Intentionally undocumented.
	virtual ~payload() {};
};

} // namespace vsomeip

#endif // VSOMEIP_PAYLOAD_HPP
