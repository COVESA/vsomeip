//
// detail/socket_ops_ext_local.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (C) 2016-2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_boost or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_LOCAL_HPP
#define BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_LOCAL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/socket_ops.hpp>

namespace boost {
namespace asio {
namespace detail {
namespace socket_ops {

BOOST_ASIO_DECL signed_size_type recv(socket_type s, buf* bufs,
    size_t count, int flags, boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid);

BOOST_ASIO_DECL signed_size_type recvfrom(socket_type s, buf* bufs,
    size_t count, int flags, socket_addr_type* addr,
    std::size_t* addrlen, boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid);

BOOST_ASIO_DECL size_t sync_recvfrom(socket_type s, state_type state,
    buf* bufs, size_t count, int flags, socket_addr_type* addr,
    std::size_t* addrlen, boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid);

#if defined(BOOST_ASIO_HAS_IOCP)

BOOST_ASIO_DECL void complete_iocp_recvfrom(
    const weak_cancel_token_type& cancel_token,
    boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid);

#else // defined(BOOST_ASIO_HAS_IOCP)

BOOST_ASIO_DECL bool non_blocking_recv(socket_type s,
    buf* bufs, size_t count, int flags, bool is_stream,
    boost::system::error_code& ec, size_t& bytes_transferred,
    std::uint32_t& uid, std::uint32_t& gid);

BOOST_ASIO_DECL bool non_blocking_recvfrom(socket_type s,
    buf* bufs, size_t count, int flags,
    socket_addr_type* addr, std::size_t* addrlen,
    boost::system::error_code& ec, size_t& bytes_transferred,
    std::uint32_t& uid, std::uint32_t& gid);

#endif // defined(BOOST_ASIO_HAS_IOCP)

} // namespace socket_ops
} // namespace detail
} // namespace asio
} // namespace boost


#if defined(BOOST_ASIO_HEADER_ONLY)
# include <boost/asio/detail/impl/socket_ops_ext_local.ipp>
#endif // defined(BOOST_ASIO_HEADER_ONLY)

#endif // BOOST_EXT_ASIO_DETAIL_SOCKET_OPS_HPP
