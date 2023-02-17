//
// detail/impl/socket_ops_ext_local.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (C) 2016-2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_boost or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_LOCAL_IPP
#define BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_LOCAL_IPP

#include <boost/asio/detail/impl/socket_ops.ipp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {
namespace socket_ops {

signed_size_type recv(socket_type s, buf* bufs, size_t count,
    int flags, boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid)
{
  uid = 0xFFFFFFFF;
  gid = 0xFFFFFFFF;
  struct ucred *ucredp;

#if defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  // Receive some data.
  DWORD recv_buf_count = static_cast<DWORD>(count);
  DWORD bytes_transferred = 0;
  DWORD recv_flags = flags;
  int result = ::WSARecv(s, bufs,
        recv_buf_count, &bytes_transferred, &recv_flags, 0, 0);
  get_last_error(ec, true);        
  if (ec.value() == ERROR_NETNAME_DELETED)
    ec = boost::asio::error::connection_reset;
  else if (ec.value() == ERROR_PORT_UNREACHABLE)
    ec = boost::asio::error::connection_refused;
  if (result != 0)
    return socket_error_retval;
  ec.assign(0, ec.category());
  return bytes_transferred;
#else // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  msghdr msg = msghdr();
  msg.msg_iov = bufs;
  msg.msg_iovlen = static_cast<int>(count);

  union {
    struct cmsghdr cmh;
    char   control[CMSG_SPACE(sizeof(struct ucred))];
  } control_un;

  // Set 'control_un' to describe ancillary data that we want to receive
  control_un.cmh.cmsg_len = CMSG_LEN(sizeof(struct ucred));
  control_un.cmh.cmsg_level = SOL_SOCKET;
  control_un.cmh.cmsg_type = SCM_CREDENTIALS;

  // Set 'msg' fields to describe 'control_un'
  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);

  signed_size_type result = ::recvmsg(s, &msg, flags);
  get_last_error(ec, true);
  if (result >= 0) {
    ec.assign(0, ec.category());

    // Find UID / GID
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_CREDENTIALS
          || cmsg->cmsg_len != CMSG_LEN(sizeof(struct ucred)))
        continue;

      ucredp = (struct ucred *) CMSG_DATA(cmsg);
      if (ucredp) {
        uid = ucredp->uid;
        gid = ucredp->gid;
      }
	}
  }
  return result;
#endif // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
}

signed_size_type recvfrom(socket_type s, buf* bufs, size_t count,
    int flags, socket_addr_type* addr, std::size_t* addrlen,
    boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid)
{
  uid = 0xFFFFFFFF;
  gid = 0xFFFFFFFF;
  struct ucred *ucredp;
  clear_last_error();
#if defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  // Receive some data.
  DWORD recv_buf_count = static_cast<DWORD>(count);
  DWORD bytes_transferred = 0;
  DWORD recv_flags = flags;
  int tmp_addrlen = (int)*addrlen;
  int result = ::WSARecvFrom(s, bufs, recv_buf_count,
        &bytes_transferred, &recv_flags, addr, &tmp_addrlen, 0, 0);
  get_last_error(ec, true);
  *addrlen = (std::size_t)tmp_addrlen;
  if (ec.value() == ERROR_NETNAME_DELETED)
    ec = boost::asio::error::connection_reset;
  else if (ec.value() == ERROR_PORT_UNREACHABLE)
    ec = boost::asio::error::connection_refused;
  if (result != 0)
    return socket_error_retval;
  ec.assign(0, ec.category());
  return bytes_transferred;
#else // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  msghdr msg = msghdr();
  init_msghdr_msg_name(msg.msg_name, addr);
  msg.msg_namelen = static_cast<int>(*addrlen);
  msg.msg_iov = bufs;
  msg.msg_iovlen = static_cast<int>(count);

  union {
    struct cmsghdr cmh;
    char   control[CMSG_SPACE(sizeof(struct ucred))];
  } control_un;

  // Set 'control_un' to describe ancillary data that we want to receive
  control_un.cmh.cmsg_len = CMSG_LEN(sizeof(struct ucred));
  control_un.cmh.cmsg_level = SOL_SOCKET;
  control_un.cmh.cmsg_type = SCM_CREDENTIALS;

  // Set 'msg' fields to describe 'control_un'
  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);

  signed_size_type result = ::recvmsg(s, &msg, flags);
  get_last_error(ec, true);
  *addrlen = msg.msg_namelen;
  if (result >= 0) {
    ec.assign(0, ec.category());

    // Find UID / GID
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
         cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_CREDENTIALS
          || cmsg->cmsg_len != CMSG_LEN(sizeof(struct ucred)))
        continue;

      ucredp = (struct ucred *) CMSG_DATA(cmsg);
      if (ucredp) {
        uid = ucredp->uid;
        gid = ucredp->gid;
      }
    }
  }
  return result;
#endif // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
}

size_t sync_recvfrom(socket_type s, state_type state, buf* bufs,
    size_t count, int flags, socket_addr_type* addr,
    std::size_t* addrlen, boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid)
{
  if (s == invalid_socket)
  {
    ec = boost::asio::error::bad_descriptor;
    return 0;
  }

  // Read some data.
  for (;;)
  {
    // Try to complete the operation without blocking.
    signed_size_type bytes = socket_ops::recvfrom(
        s, bufs, count, flags, addr, addrlen, ec, uid, gid);

    // Check if operation succeeded.
    if (bytes >= 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != boost::asio::error::would_block
          && ec != boost::asio::error::try_again))
      return 0;

    // Wait for socket to become ready.
    if (socket_ops::poll_read(s, 0, -1, ec) < 0)
      return 0;
  }
}

#if defined(BOOST_ASIO_HAS_IOCP)

void complete_iocp_recvfrom(
    const weak_cancel_token_type& cancel_token,
    boost::system::error_code& ec,
    std::uint32_t& uid, std::uint32_t& gid)
{
  uid = 0xFFFFFFFF;
  gid = 0xFFFFFFFF;
  // Map non-portable errors to their portable counterparts.
  if (ec.value() == ERROR_NETNAME_DELETED)
  {
    if (cancel_token.expired())
      ec = boost::asio::error::operation_aborted;
    else
      ec = boost::asio::error::connection_reset;
  }
  else if (ec.value() == ERROR_PORT_UNREACHABLE)
  {
    ec = boost::asio::error::connection_refused;
  }
}

#else // defined(BOOST_ASIO_HAS_IOCP)

bool non_blocking_recv(socket_type s,
    buf* bufs, size_t count, int flags, bool is_stream,
    boost::system::error_code& ec, size_t& bytes_transferred,
    std::uint32_t& uid, std::uint32_t& gid)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = socket_ops::recv(s, bufs, count, flags, ec, uid, gid);

    // Check for end of stream.
    if (is_stream && bytes == 0)
    {
      ec = boost::asio::error::eof;
      return true;
    }

    // Retry operation if interrupted by signal.
    if (ec == boost::asio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == boost::asio::error::would_block
        || ec == boost::asio::error::try_again)
      return false;

    // Operation is complete.
    if (bytes >= 0)
    {
      ec = boost::system::error_code();
      bytes_transferred = bytes;
    }
    else
      bytes_transferred = 0;

    return true;
  }
}

bool non_blocking_recvfrom(socket_type s,
    buf* bufs, size_t count, int flags,
    socket_addr_type* addr, std::size_t* addrlen,
    boost::system::error_code& ec, size_t& bytes_transferred,
    std::uint32_t& uid, std::uint32_t& gid)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = socket_ops::recvfrom(
        s, bufs, count, flags, addr, addrlen, ec, uid, gid);

    // Retry operation if interrupted by signal.
    if (ec == boost::asio::error::interrupted)
      continue;

    // Check if we need to run the operation again.
    if (ec == boost::asio::error::would_block
        || ec == boost::asio::error::try_again)
      return false;

    // Operation is complete.
    if (bytes >= 0)
    {
      ec = boost::system::error_code();
      bytes_transferred = bytes;
    }
    else
      bytes_transferred = 0;

    return true;
  }
}

#endif // defined(BOOST_ASIO_HAS_IOCP)

} // namespace socket_ops
} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_LOCAL_IPP
