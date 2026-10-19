/**
 * @file socket_helpers.h
 * @brief Helper functions for multi-platform sockets
 *
 * Copyright 2026 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef SOCKET_HELPERS_H
#define SOCKET_HELPERS_H

#include <sys/cdefs.h>
#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>

typedef SOCKET socket_handle_t; ///< Socket handle type
#define SOCKET_HANDLE_INVALID INVALID_SOCKET ///< Invalid socket handle

#else

typedef int socket_handle_t; ///< Socket handle type
#define SOCKET_HANDLE_INVALID (-1) ///< Invalid socket handle

#endif

__BEGIN_DECLS

/**
 * Initialise socket support.
 * @note This is only required for Windows but can safely be called for all
 *       platforms.
 *
 * @return Zero for success. Less than zero for error.
 */
int socket_init(void);

/**
 * Release socket support. See @ref socket_init().
 */
void socket_cleanup(void);

/**
 * Connect to provided address.
 *
 * An address containing a path separator ("/") is a Unix domain socket. A TCP
 * address is of the form "host:port", ":port" or "port", and an IPv6 address
 * is of the form "[address]:port". A TCP address without a host connects to
 * the loopback interface.
 *
 * @param addr Address string
 * @param s Connected socket output. @ref SOCKET_HANDLE_INVALID for error.
 * @return Zero for success. Less than zero for error. Greater than zero for
 *         failure to connect.
 */
int socket_connect(const char* addr, socket_handle_t* s);

/**
 * Retrieve numeric address of connected socket peer.
 *
 * For a TCP socket, this provides the numeric host address and port. For a
 * Unix domain socket, this provides the socket path and an empty port.
 *
 * @param s Connected socket
 * @param host Host output string
 * @param host_size Size of host output string in bytes
 * @param port Port output string
 * @param port_size Size of port output string in bytes
 * @return Zero for success. Less than zero for error.
 */
int socket_get_peer_name(
	socket_handle_t s,
	char* host,
	size_t host_size,
	char* port,
	size_t port_size
);

/**
 * Receive bytes from socket.
 * @note This may receive fewer bytes than requested.
 *
 * @param s Socket
 * @param buf Output buffer
 * @param len Length of output buffer in bytes
 * @return Number of bytes received. Zero for end of input. Less than zero for error.
 */
long socket_recv(socket_handle_t s, void* buf, size_t len);

/**
 * Send bytes to socket.
 * @note This suppresses the signal that some platforms raise when sending to
 *       a socket which the peer has already closed, such that the error is
 *       reported instead of terminating the process.
 *
 * @param s Socket
 * @param buf Buffer to send
 * @param len Length of buffer in bytes
 * @return Zero for success. Less than zero for error.
 */
int socket_send(socket_handle_t s, const void* buf, size_t len);

/**
 * Shut down write side of socket such that peer reads end of input while while
 * reading from this side remains possible.
 *
 * @param s Socket
 * @return Zero for success. Less than zero for error.
 */
int socket_shutdown_write(socket_handle_t s);

/**
 * Close socket.
 *
 * @param s Socket
 */
void socket_close(socket_handle_t s);

__END_DECLS

#endif
