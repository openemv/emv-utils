/**
 * @file socket_helpers_test.c
 * @brief Unit tests for socket helper functions
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "socket_helpers.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
typedef int socklen_compat_t;
#else
typedef socklen_t socklen_compat_t;
#endif

static const char unix_path[] = "./socket_helpers_test.sock";
static const char unix_path_missing[] = "./socket_helpers_test_missing.sock";
static const uint8_t response[] = { 0x8A, 0x02, 0x30, 0x30 };
static const char* const invalid_addr[] = {
	"[::1",
	"127.0.0.1:",
	"127.0.0.1:5000x",
	"localhost",
	"0",
	"65536",
	"70000",
	"99999999999",
};

// Helper functions
static socket_handle_t listen_tcp(char* addr, size_t addr_size);
static socket_handle_t listen_unix(const char* path);
static int exchange(socket_handle_t listener, const char* addr, const char* peer_host, const char* peer_port);

static socket_handle_t listen_tcp(char* addr, size_t addr_size)
{
	int r;
	socket_handle_t listener;
	struct sockaddr_in sa;
	socklen_compat_t sa_len = sizeof(sa);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == SOCKET_HANDLE_INVALID) {
		fprintf(stderr, "socket() failed\n");
		return SOCKET_HANDLE_INVALID;
	}

	// Listen on the loopback interface using a port chosen by the system
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa.sin_port = 0;
	r = bind(listener, (const struct sockaddr*)&sa, sizeof(sa));
	if (r) {
		fprintf(stderr, "bind() failed; r=%d\n", r);
		goto error;
	}
	r = listen(listener, 2);
	if (r) {
		fprintf(stderr, "listen() failed; r=%d\n", r);
		goto error;
	}

	// Provide the address in the ":port" form
	r = getsockname(listener, (struct sockaddr*)&sa, &sa_len);
	if (r) {
		fprintf(stderr, "getsockname() failed; r=%d\n", r);
		goto error;
	}
	snprintf(addr, addr_size, ":%u", (unsigned int)ntohs(sa.sin_port));

	return listener;

error:
	socket_close(listener);
	return SOCKET_HANDLE_INVALID;
}

static socket_handle_t listen_unix(const char* path)
{
	int r;
	socket_handle_t listener;
	struct sockaddr_un sa;

	listener = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listener == SOCKET_HANDLE_INVALID) {
		fprintf(stderr, "socket() failed\n");
		return SOCKET_HANDLE_INVALID;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	memcpy(sa.sun_path, path, strlen(path));
	r = bind(listener, (const struct sockaddr*)&sa, sizeof(sa));
	if (r) {
		fprintf(stderr, "bind() failed; r=%d\n", r);
		goto error;
	}
	r = listen(listener, 1);
	if (r) {
		fprintf(stderr, "listen() failed; r=%d\n", r);
		goto error;
	}

	return listener;

error:
	socket_close(listener);
	return SOCKET_HANDLE_INVALID;
}

static int exchange(socket_handle_t listener, const char* addr, const char* peer_host, const char* peer_port)
{
	int r;
	socket_handle_t s = SOCKET_HANDLE_INVALID;
	socket_handle_t peer = SOCKET_HANDLE_INVALID;
	char host[256];
	char port[32];
	uint8_t request[300];
	uint8_t buf[1024];
	size_t buf_len;
	long bytes;

	for (size_t i = 0; i < sizeof(request); ++i) {
		request[i] = i;
	}

	// A blocking connect() completes once the connection is queued by the
	// listener, which allows a single thread to connect before it accepts
	r = socket_connect(addr, &s);
	if (r) {
		fprintf(stderr, "socket_connect(\"%s\") failed; r=%d\n", addr, r);
		r = 1;
		goto exit;
	}

	r = socket_get_peer_name(s, host, sizeof(host), port, sizeof(port));
	if (r) {
		fprintf(stderr, "socket_get_peer_name() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	if (strcmp(host, peer_host) != 0 || strcmp(port, peer_port) != 0) {
		fprintf(stderr, "socket_get_peer_name() failed; host=\"%s\"; port=\"%s\"\n", host, port);
		r = 1;
		goto exit;
	}

	peer = accept(listener, NULL, NULL);
	if (peer == SOCKET_HANDLE_INVALID) {
		fprintf(stderr, "accept() failed\n");
		r = 1;
		goto exit;
	}

	// Send the request and half-close, after which the peer must read end of
	// input after the request
	r = socket_send(s, request, sizeof(request));
	if (r) {
		fprintf(stderr, "socket_send() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = socket_shutdown_write(s);
	if (r) {
		fprintf(stderr, "socket_shutdown_write() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	buf_len = 0;
	while ((bytes = recv(peer, (void*)(buf + buf_len), sizeof(buf) - buf_len, 0)) > 0) {
		buf_len += bytes;
	}
	if (bytes < 0) {
		fprintf(stderr, "recv() failed\n");
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(request) || memcmp(buf, request, sizeof(request)) != 0) {
		fprintf(stderr, "Peer received incorrect request; buf_len=%zu\n", buf_len);
		r = 1;
		goto exit;
	}

	bytes = send(peer, (const void*)response, sizeof(response), 0);
	if (bytes != (long)sizeof(response)) {
		fprintf(stderr, "send() failed; bytes=%ld\n", bytes);
		r = 1;
		goto exit;
	}
	socket_close(peer);
	peer = SOCKET_HANDLE_INVALID;

	// Read the response until the peer closes the connection
	buf_len = 0;
	while ((bytes = socket_recv(s, buf + buf_len, sizeof(buf) - buf_len)) > 0) {
		buf_len += bytes;
	}
	if (bytes < 0) {
		fprintf(stderr, "socket_recv() failed; bytes=%ld\n", bytes);
		r = 1;
		goto exit;
	}
	if (buf_len != sizeof(response) || memcmp(buf, response, sizeof(response)) != 0) {
		fprintf(stderr, "Incorrect response; buf_len=%zu\n", buf_len);
		r = 1;
		goto exit;
	}

	r = 0;
	goto exit;

exit:
	if (peer != SOCKET_HANDLE_INVALID) {
		socket_close(peer);
	}
	if (s != SOCKET_HANDLE_INVALID) {
		socket_close(s);
	}
	return r;
}

int main(void)
{
	int r;
	socket_handle_t listener = SOCKET_HANDLE_INVALID;
	socket_handle_t s;
	char addr[32];

	r = socket_init();
	if (r) {
		fprintf(stderr, "socket_init() failed; r=%d\n", r);
		return 1;
	}

	printf("\nTest 1: TCP connection without host\n");
	listener = listen_tcp(addr, sizeof(addr));
	if (listener == SOCKET_HANDLE_INVALID) {
		r = 1;
		goto exit;
	}
	r = exchange(listener, addr, "127.0.0.1", addr + 1);
	if (r) {
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 2: TCP connection with host\n");
	{
		char host_addr[64];

		snprintf(host_addr, sizeof(host_addr), "127.0.0.1%s", addr);
		r = exchange(listener, host_addr, "127.0.0.1", addr + 1);
		if (r) {
			r = 1;
			goto exit;
		}
	}
	socket_close(listener);
	listener = SOCKET_HANDLE_INVALID;
	printf("Success\n");

	printf("\nTest 3: Unix domain socket connection\n");
	remove(unix_path);
	listener = listen_unix(unix_path);
	if (listener == SOCKET_HANDLE_INVALID) {
		r = 1;
		goto exit;
	}
	r = exchange(listener, unix_path, unix_path, "");
	if (r) {
		r = 1;
		goto exit;
	}
	socket_close(listener);
	listener = SOCKET_HANDLE_INVALID;
	printf("Success\n");

	printf("\nTest 4: Unix domain socket without listener\n");
	remove(unix_path_missing);
	r = socket_connect(unix_path_missing, &s);
	if (r <= 0 || s != SOCKET_HANDLE_INVALID) {
		fprintf(stderr, "socket_connect() unexpectedly succeeded; r=%d\n", r);
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTest 5: Invalid addresses\n");
	for (size_t i = 0; i < sizeof(invalid_addr) / sizeof(invalid_addr[0]); ++i) {
		r = socket_connect(invalid_addr[i], &s);
		if (r >= 0 || s != SOCKET_HANDLE_INVALID) {
			fprintf(stderr, "socket_connect() unexpectedly accepted \"%s\"; r=%d\n", invalid_addr[i], r);
			r = 1;
			goto exit;
		}
	}
	printf("Success\n");

	r = 0;
	goto exit;

exit:
	if (listener != SOCKET_HANDLE_INVALID) {
		socket_close(listener);
	}
	remove(unix_path);
	socket_cleanup();
	return r;
}
