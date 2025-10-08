// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_IO_CONTROL_OPERATION_HPP_
#define VSOMEIP_V3_IO_CONTROL_OPERATION_HPP_

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/socket_types.hpp>
#include <cstddef>

namespace vsomeip_v3 {

// I/O control operation
template<typename T>
class io_control_operation {
public:
    // Default constructor.
    io_control_operation(int command) : command_(command), value_(0) { }

    // Construct with a specific command value.
    io_control_operation(int command, T value) : command_(command), value_(static_cast<boost::asio::detail::ioctl_arg_type>(value)) { }

    // Get the name of the IO control command.
    int name() const { return command_; }

    // Set the value of the I/O control command.
    void set(T value) { value_ = static_cast<boost::asio::detail::ioctl_arg_type>(value); }

    // Get the current value of the I/O control command.
    T get() const { return static_cast<T>(value_); }

    // Get the address of the command data.
    boost::asio::detail::ioctl_arg_type* data() { return &value_; }

    // Get the address of the command data.
    const boost::asio::detail::ioctl_arg_type* data() const { return &value_; }

private:
    const int command_;
    boost::asio::detail::ioctl_arg_type value_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_IO_CONTROL_OPERATION_HPP_
