/**
 * @file socket_helpers.c
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

#include "socket_helpers.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
typedef int socklen_compat_t;
#else
typedef socklen_t socklen_compat_t;
#endif

// Helper functions
static int parse_addr(const char* addr, char* host, size_t host_size, char* port, size_t port_size);
static void suppress_sigpipe(socket_handle_t s);
static int connect_unix(const char* path, socket_handle_t* s);
static int connect_tcp(const char* host, const char* port, socket_handle_t* s);

int socket_init(void)
{
#ifdef _WIN32
	int r;
	WSADATA wsa_data;

	r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (r) {
		return -1;
	}
#endif

	return 0;
}

void socket_cleanup(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

static int parse_addr(const char* addr, char* host, size_t host_size, char* port, size_t port_size)
{
	const char* colon;
	const char* port_str;
	size_t host_len;
	char* endptr;
	unsigned long port_num;

	if (strchr(addr, '/')) {
		// Return zero for a Unix domain socket
		return 0;
	}

	// TCP address is of the form "host:port", ":port" or "port"
	// IPv6 address is of the form "[address]:port"
	colon = strrchr(addr, ':');
	if (!colon) {
		// "port"
		host_len = 0;
		port_str = addr;
	} else if (addr[0] == '[') {
		// "[address]:port" for an IPv6 address
		host_len = colon - addr;
		if (host_len < 3 || addr[host_len - 1] != ']') {
			return -1;
		}
		++addr;
		host_len -= 2;
		port_str = colon + 1;
	} else {
		// "host:port" or ":port"
		host_len = colon - addr;
		port_str = colon + 1;
	}

	// Port must be a decimal number from 1 to 65535 because getaddrinfo()
	// does not reject an out of range port on all platforms
	if (*port_str < '0' || *port_str > '9') {
		// Return less than zero for invalid address
		return -2;
	}
	port_num = strtoul(port_str, &endptr, 10);
	if (*endptr) {
		// Return less than zero for invalid address
		return -3;
	}
	if (port_num < 1 || port_num > 65535) {
		// Return less than zero for invalid address
		return -4;
	}

	if (host_len >= host_size) {
		// Return less than zero for invalid address
		return -5;
	}

	if (host_len) {
		memcpy(host, addr, host_len);
		host[host_len] = 0;
	} else {
		// A TCP address without a host uses the loopback interface
		snprintf(host, host_size, "%s", "127.0.0.1");
	}
	snprintf(port, port_size, "%lu", port_num);

	// Return greater than zero for TCP address
	return 1;
}

static void suppress_sigpipe(socket_handle_t s)
{
#ifdef SO_NOSIGPIPE
	// Platforms without MSG_NOSIGNAL, like MacOS, provide a socket option
	// instead. Windows does not raise a signal at all.
	int opt = 1;
	setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif
}

static int connect_unix(const char* path, socket_handle_t* s)
{
	int r;
	struct sockaddr_un sa;
	size_t path_len = strlen(path);

	memset(&sa, 0, sizeof(sa));
	if (path_len >= sizeof(sa.sun_path)) {
		// Unix domain socket path too long
		return -1;
	}
	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, path_len);

	*s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (*s == SOCKET_HANDLE_INVALID) {
		return 1;
	}
	suppress_sigpipe(*s);

	r = connect(*s, (const struct sockaddr*)&sa, sizeof(sa));
	if (r) {
		socket_close(*s);
		*s = SOCKET_HANDLE_INVALID;
		return 2;
	}

	return 0;
}

static int connect_tcp(const char* host, const char* port, socket_handle_t* s)
{
	int r;
	struct addrinfo hints;
	struct addrinfo* result = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;

	r = getaddrinfo(host, port, &hints, &result);
	if (r || !result) {
		return 1;
	}

	// Try each resolved address once
	for (struct addrinfo* ai = result; ai; ai = ai->ai_next) {
		*s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (*s == SOCKET_HANDLE_INVALID) {
			continue;
		}
		suppress_sigpipe(*s);

		r = connect(*s, ai->ai_addr, (socklen_compat_t)ai->ai_addrlen);
		if (r == 0) {
			freeaddrinfo(result);
			return 0;
		}

		socket_close(*s);
		*s = SOCKET_HANDLE_INVALID;
	}

	freeaddrinfo(result);
	return 2;
}

int socket_connect(const char* addr, socket_handle_t* s)
{
	int r;
	char host[256];
	char port[32];

	if (!addr || !s) {
		return -1;
	}
	*s = SOCKET_HANDLE_INVALID;

	r = parse_addr(addr, host, sizeof(host), port, sizeof(port));
	if (r < 0) {
		return -2;
	}

	if (r == 0) {
		r = connect_unix(addr, s);
	} else {
		r = connect_tcp(host, port, s);
	}
	if (r < 0) {
		return -3;
	}
	if (r > 0) {
		return 1;
	}

	return 0;
}

int socket_get_peer_name(
	socket_handle_t s,
	char* host,
	size_t host_size,
	char* port,
	size_t port_size
)
{
	int r;
	struct sockaddr_storage ss;
	socklen_compat_t ss_len = sizeof(ss);

	if (!host || !host_size || !port || !port_size) {
		return -1;
	}

	memset(&ss, 0, sizeof(ss));
	r = getpeername(s, (struct sockaddr*)&ss, &ss_len);
	if (r) {
		return -2;
	}

	if (ss.ss_family == AF_UNIX) {
		const struct sockaddr_un* sa = (const struct sockaddr_un*)&ss;
		size_t path_len;

		// The path may or may not be NUL terminated within the length
		// provided by getpeername()
		if (ss_len <= offsetof(struct sockaddr_un, sun_path)) {
			// Unnamed socket
			return -3;
		}
		path_len = strnlen(sa->sun_path, ss_len - offsetof(struct sockaddr_un, sun_path));
		if (!path_len) {
			// Unnamed socket
			return -4;
		}
		if (path_len >= host_size) {
			return -5;
		}

		memcpy(host, sa->sun_path, path_len);
		host[path_len] = 0;
		port[0] = 0;
		return 0;
	}

	r = getnameinfo(
		(const struct sockaddr*)&ss,
		ss_len,
		host,
		host_size,
		port,
		port_size,
		NI_NUMERICHOST | NI_NUMERICSERV
	);
	if (r) {
		return -6;
	}

	return 0;
}

long socket_recv(socket_handle_t s, void* buf, size_t len)
{
#ifdef _WIN32
	return recv(s, buf, (int)len, 0);
#else
	return recv(s, buf, len, 0);
#endif
}

int socket_send(socket_handle_t s, const void* buf, size_t len)
{
	const uint8_t* ptr = buf;

	while (len) {
		long bytes;

#ifdef _WIN32
		bytes = send(s, (const char*)ptr, (int)len, 0);
#elif defined(MSG_NOSIGNAL)
		bytes = send(s, ptr, len, MSG_NOSIGNAL);
#else
		bytes = send(s, ptr, len, 0);
#endif
		if (bytes <= 0) {
			return -1;
		}

		ptr += bytes;
		len -= bytes;
	}

	return 0;
}

int socket_shutdown_write(socket_handle_t s)
{
	int r;

#ifdef _WIN32
	r = shutdown(s, SD_SEND);
#else
	r = shutdown(s, SHUT_WR);
#endif
	if (r) {
		return -1;
	}

	return 0;
}

void socket_close(socket_handle_t s)
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}
