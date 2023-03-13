//
// detail/impl/socket_ops_ext.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_boost or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_IPP
#define BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_IPP

#include <boost/asio/detail/impl/socket_ops.ipp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {
namespace socket_ops {

signed_size_type recvfrom(socket_type s, buf* bufs, size_t count,
    int flags, socket_addr_type* addr, std::size_t* addrlen,
    boost::system::error_code& ec, boost::asio::ip::address& da)
{
  clear_last_error();
#if defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
  LPFN_WSARECVMSG WSARecvMsg;
  DWORD NumberOfBytes;

  error_wrapper(WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
		 &WSARecvMsg_GUID, sizeof WSARecvMsg_GUID,
		 &WSARecvMsg, sizeof WSARecvMsg,
		 &NumberOfBytes, NULL, NULL), ec);
  if (ec.value() == SOCKET_ERROR) {
	WSARecvMsg = NULL;
	return 0;
  }
  
  WSABUF wsaBuf;
  WSAMSG msg;
  char controlBuffer[1024];
  msg.name = addr;
  msg.namelen = *addrlen;
  wsaBuf.buf = bufs->buf;
  wsaBuf.len = bufs->len;
  msg.lpBuffers = &wsaBuf;
  msg.dwBufferCount = count;
  msg.Control.len = sizeof controlBuffer;
  msg.Control.buf = controlBuffer;
  msg.dwFlags = flags;

  DWORD dwNumberOfBytesRecvd;
  signed_size_type result = error_wrapper(WSARecvMsg(s, &msg, &dwNumberOfBytesRecvd, NULL, NULL), ec);
  
  if (result >= 0) {
    ec = boost::system::error_code();

	// Find destination address
	for (LPWSACMSGHDR cmsg = WSA_CMSG_FIRSTHDR(&msg);
		 cmsg != NULL;
	  	 cmsg = WSA_CMSG_NXTHDR(&msg, cmsg))
	{
	  if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
	  	continue;

      struct in_pktinfo *pi = (struct in_pktinfo *) WSA_CMSG_DATA(cmsg);
	  if (pi)
	  {
	    da = boost::asio::ip::address_v4(ntohl(pi->ipi_addr.s_addr));
	  } 
	}      
  } else {
    dwNumberOfBytesRecvd = -1;
  }
  return dwNumberOfBytesRecvd;
#else // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
  char cmbuf[0x100];
  msghdr msg = msghdr();
  init_msghdr_msg_name(msg.msg_name, addr);
  msg.msg_namelen = static_cast<int>(*addrlen);
  msg.msg_iov = bufs;
  msg.msg_iovlen = static_cast<int>(count);
  msg.msg_control = cmbuf;
  msg.msg_controllen = sizeof(cmbuf);
  signed_size_type result = error_wrapper(::recvmsg(s, &msg, flags), ec);
  *addrlen = msg.msg_namelen;
  if (result >= 0) {
    ec = boost::system::error_code();

	// Find destination address
	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		 cmsg != NULL;
	  	 cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
	  if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO)
	  	continue;

      struct in_pktinfo *pi = (struct in_pktinfo *) CMSG_DATA(cmsg);
	  if (pi)
	  {
	    da = boost::asio::ip::address_v4(ntohl(pi->ipi_addr.s_addr));
	  } 
	}      
  }
  return result;
#endif // defined(BOOST_ASIO_WINDOWS) || defined(__CYGWIN__)
}

size_t sync_recvfrom(socket_type s, state_type state, buf* bufs,
    size_t count, int flags, socket_addr_type* addr,
    std::size_t* addrlen, boost::system::error_code& ec, boost::asio::ip::address& da)
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
        s, bufs, count, flags, addr, addrlen, ec, da);

    // Check if operation succeeded.
    if (bytes >= 0)
      return bytes;

    // Operation failed.
    if ((state & user_set_non_blocking)
        || (ec != boost::asio::error::would_block
          && ec != boost::asio::error::try_again))
      return 0;

    // Wait for socket to become ready.
    if (socket_ops::poll_read(s, 0, ec) < 0)
      return 0;
  }
}

#if defined(BOOST_ASIO_HAS_IOCP)

void complete_iocp_recvfrom(
    const weak_cancel_token_type& cancel_token,
    boost::system::error_code& ec, boost::asio::ip::address& da)
{
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

bool non_blocking_recvfrom(socket_type s,
    buf* bufs, size_t count, int flags,
    socket_addr_type* addr, std::size_t* addrlen,
    boost::system::error_code& ec, size_t& bytes_transferred, boost::asio::ip::address& da)
{
  for (;;)
  {
    // Read some data.
    signed_size_type bytes = socket_ops::recvfrom(
        s, bufs, count, flags, addr, addrlen, ec, da);

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

#endif // BOOST_ASIO_DETAIL_SOCKET_OPS_EXT_IPP
