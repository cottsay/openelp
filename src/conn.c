/*!
 * @file conn.c
 *
 * @copyright
 * Copyright &copy; 2016, Scott K Logan
 *
 * @copyright
 * All rights reserved.
 *
 * @copyright
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * @copyright
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * @copyright
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @copyright
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan &lt;logans@cottsay.net&gt;
 *
 * @brief Network connection implementation
 */

#include "conn.h"
#ifdef _WIN32
#  include "conn_wsa_errno.h"
#endif
#include "mutex.h"

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

#ifndef MSG_NOSIGNAL
/// Requests not to send SIGPIPE on errors
#  define MSG_NOSIGNAL 0
#endif

#ifdef _WIN32
/// Disallow further receptions and transmissions
#  define SHUT_RDWR SD_BOTH

/// Last socket function error value
#  define SOCK_ERRNO -conn_wsa_errno()
#else
/// Close the given socket handle
#  define closesocket(X) close(X)

/// Invalid socket value
#  define INVALID_SOCKET -1

/// Last socket function error value
#  define SOCK_ERRNO -errno

/// Socket function return value indicating an error
#  define SOCKET_ERROR -1

/// Socket handle type
typedef int SOCKET;
#endif

/*!
 * @brief Private data for an instance of a network connection
 */
struct conn_priv
{
	/// Actual socket file descriptor
	SOCKET sock_fd;

	/// Socket connection file descriptor
	SOCKET conn_fd;

	/// One of conn_priv::sock_fd or conn_priv::conn_fd, used for TX/RX
	SOCKET fd;

	/// Storage for the remote address of the connection
	struct sockaddr_storage remote_addr;

	/// Length of conn_priv::remote_addr_len
	socklen_t remote_addr_len;

	/// Mutex for protecting the socket file descriptors
	struct mutex_handle mutex;

#ifdef _WIN32
	/// Information about the Windows Sockets implementation
	WSADATA wsadat;
#endif
};

int conn_init(struct conn_handle *conn)
{
	struct conn_priv *priv;
	int ret;

	if (conn->priv == NULL)
	{
		conn->priv = malloc(sizeof(struct conn_priv));
	}

	if (conn->priv == NULL)
	{
		return -ENOMEM;
	}

	memset(conn->priv, 0x0, sizeof(struct conn_priv));

	priv = (struct conn_priv *)conn->priv;

#ifdef _WIN32
	ret = WSAStartup(MAKEWORD(2, 2), &priv->wsadat);
	if (ret != 0)
	{
		ret = -ret;
		goto conn_init_exit_pre;
	}
#endif

	ret = mutex_init(&priv->mutex);
	if (ret < 0)
	{
		goto conn_init_exit;
	}

	priv->sock_fd = INVALID_SOCKET;
	priv->conn_fd = INVALID_SOCKET;
	priv->fd = INVALID_SOCKET;

	return 0;

conn_init_exit:
#ifdef _WIN32
	WSACleanup();
conn_init_exit_pre:
#endif
	free (conn->priv);
	conn->priv = NULL;

	return ret;
}

void conn_free(struct conn_handle *conn)
{
	if (conn->priv != NULL)
	{
		struct conn_priv *priv = (struct conn_priv *)conn->priv;

		conn_close(conn);

#ifdef _WIN32
		WSACleanup();
#endif

		mutex_free(&priv->mutex);

		free(conn->priv);

		conn->priv = NULL;
	}
}

int conn_listen(struct conn_handle *conn)
{
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	int ret;
	const int yes = 1;

	memset(&hints, 0x0, sizeof(struct addrinfo));

	switch(conn->type)
	{
	case CONN_TYPE_TCP:
		hints.ai_socktype = SOCK_STREAM;
		break;
	case  CONN_TYPE_UDP:
		hints.ai_socktype = SOCK_DGRAM;
		break;
	default:
		return -1;
	}

	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;

	ret = getaddrinfo(conn->source_addr, conn->source_port == NULL ? "0" : conn->source_port, &hints, &res);
	if (ret != 0)
	{
		return -EADDRNOTAVAIL;
	}

	priv->sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (priv->sock_fd == INVALID_SOCKET)
	{
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}

	ret = setsockopt(priv->sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(int));
	if (ret == SOCKET_ERROR)
	{
		/// @TODO Close priv->sock_fd
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}

#ifdef __APPLE__
	ret = setsockopt(priv->sock_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&yes, sizeof(int));
	if (ret == SOCKET_ERROR)
	{
		/// @TODO Close priv->sock_fd
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}
#endif

	ret = bind(priv->sock_fd, res->ai_addr, (socklen_t)res->ai_addrlen);
	if (ret == SOCKET_ERROR)
	{
		/// @TODO Close priv->sock_fd
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}

	if (conn->type == CONN_TYPE_TCP)
	{
		ret = listen(priv->sock_fd, 0);
		if (ret == SOCKET_ERROR)
		{
			/// @TODO Close priv->sock_fd
			ret = SOCK_ERRNO;
			goto conn_listen_free;
		}
	}

	mutex_lock(&priv->mutex);

	priv->fd = priv->sock_fd;

	mutex_unlock(&priv->mutex);

conn_listen_free:
	freeaddrinfo(res);

	return ret;
}

int conn_accept(struct conn_handle *conn, struct conn_handle *accepted)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	struct conn_priv *apriv = (struct conn_priv *)accepted->priv;

	apriv->remote_addr_len = sizeof(struct sockaddr_storage);

	mutex_lock_shared(&priv->mutex);

	apriv->conn_fd = accept(priv->sock_fd, (struct sockaddr *)&apriv->remote_addr, &apriv->remote_addr_len);

	mutex_unlock_shared(&priv->mutex);

	if (apriv->conn_fd == INVALID_SOCKET)
	{
		return SOCK_ERRNO;
	}

	mutex_lock(&apriv->mutex);

	apriv->fd = apriv->conn_fd;

	mutex_unlock(&apriv->mutex);

	return 0;
}

int conn_connect(struct conn_handle *conn, const char *addr, const char *port)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *res_remote = NULL;
	const int yes = 1;
	int ret;

	if (conn->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}

	memset(&hints, 0x0, sizeof(struct addrinfo));

	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(conn->source_addr, conn->source_port == NULL ? "0" : conn->source_port, &hints, &res);
	if (ret != 0)
	{
		ret = -EADDRNOTAVAIL;
		goto conn_connect_free_early;
	}

	memset(&hints, 0x0, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(addr, port, &hints, &res_remote);
	if (ret != 0)
	{
		ret = -EADDRNOTAVAIL;
		goto conn_connect_free_early;
	}

	priv->sock_fd = socket(AF_INET, res->ai_socktype, res->ai_protocol);
	if (priv->sock_fd == INVALID_SOCKET)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free_early;
	}

	ret = setsockopt(priv->sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(int));
	if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free_early;
	}

#ifdef __APPLE__
	ret = setsockopt(priv->sock_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&yes, sizeof(int));
	if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free_early;
	}
#endif

	ret = bind(priv->sock_fd, res->ai_addr, (socklen_t)res->ai_addrlen);
	if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free;
	}

	ret = connect(priv->sock_fd, res_remote->ai_addr, (socklen_t)res_remote->ai_addrlen);
	if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free;
	}

	freeaddrinfo(res_remote);
	freeaddrinfo(res);

	mutex_lock(&priv->mutex);

	priv->fd = priv->sock_fd;

	mutex_unlock(&priv->mutex);

	return 0;

conn_connect_free:
	shutdown(priv->sock_fd, SHUT_RDWR);

	closesocket(priv->sock_fd);

	priv->sock_fd = INVALID_SOCKET;

conn_connect_free_early:
	if (res_remote != NULL)
	{
		freeaddrinfo(res_remote);
	}

	if (res != NULL)
	{
		freeaddrinfo(res);
	}

	return ret;
}

int conn_recv(struct conn_handle *conn, uint8_t *buff, size_t buff_len)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	int ret = 0;
	int bytes_read = 0;

	if (conn->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}

	mutex_lock_shared(&priv->mutex);

	if (priv->fd == INVALID_SOCKET)
	{
		ret = -ENOTCONN;

		goto conn_recv_exit;
	}

	while (buff_len > 0)
	{
		ret = recvfrom(priv->fd, (char *)buff, (socklen_t)buff_len, 0, NULL, NULL);

		if (ret == 0)
		{
			ret = -EPIPE;

			goto conn_recv_exit;
		}
		else if (ret == SOCKET_ERROR)
		{
			ret = SOCK_ERRNO;

#ifdef _WIN32
			if (ret == -WSAESHUTDOWN)
			{
				ret = -EPIPE;
			}
#endif

			goto conn_recv_exit;
		}

		buff_len -= ret;
		buff += ret;
		ret = bytes_read += ret;
	}

conn_recv_exit:
	mutex_unlock_shared(&priv->mutex);

	return ret;
}

int conn_recv_any(struct conn_handle *conn, uint8_t *buff, size_t buff_len, uint32_t *addr, uint16_t *port)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	int ret;

	priv->remote_addr_len = sizeof(struct sockaddr_storage);

	mutex_lock_shared(&priv->mutex);

	if (priv->fd == INVALID_SOCKET)
	{
		ret = -ENOTCONN;

		goto conn_recv_any_exit;
	}

	ret = recvfrom(priv->fd, (char *)buff, (socklen_t)buff_len, 0, (struct sockaddr *)&priv->remote_addr, &priv->remote_addr_len);

	if (ret == 0)
	{
		ret = -EPIPE;
	}
	else if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;

#ifdef _WIN32
		if (ret == -WSAESHUTDOWN)
		{
			ret = -EPIPE;
		}
#endif
	}

conn_recv_any_exit:
	mutex_unlock_shared(&priv->mutex);

	if (addr != NULL && ret > 0)
	{
		*addr = ((struct sockaddr_in *)&priv->remote_addr)->sin_addr.s_addr;
	}

	if (port != NULL && ret > 0)
	{
		*port = htons(((struct sockaddr_in *)&priv->remote_addr)->sin_port);
	}

	return ret;
}

int conn_send(struct conn_handle *conn, const uint8_t *buff, size_t buff_len)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	int ret;

	if (conn->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}

	mutex_lock_shared(&priv->mutex);

	if (priv->fd != INVALID_SOCKET)
	{
		while (buff_len > 0)
		{
			ret = send(priv->fd, (char *)buff, (socklen_t)buff_len, MSG_NOSIGNAL);

			if (ret == 0)
			{
				ret = -EPIPE;

				goto conn_send_exit;
			}
			else if (ret == SOCKET_ERROR)
			{
				ret = SOCK_ERRNO;

#ifdef _WIN32
				if (ret == -WSAESHUTDOWN)
				{
					ret = -EPIPE;
				}
#endif

				goto conn_send_exit;
			}

			buff_len -= ret;
		}

		ret = 0;
	}
	else
	{
		ret = -ENOTCONN;
	}

conn_send_exit:
	mutex_unlock_shared(&priv->mutex);

	return ret;
}

int conn_send_to(struct conn_handle *conn, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	struct sockaddr_in saddr;
	int ret;

	if (conn->type != CONN_TYPE_UDP)
	{
		return -EPROTOTYPE;
	}

	memset(&saddr, 0x0, sizeof(struct sockaddr_in));

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = addr;

	mutex_lock_shared(&priv->mutex);

	if (priv->fd != INVALID_SOCKET)
	{
		while (buff_len > 0)
		{
			ret = sendto(priv->fd, (char *)buff, (socklen_t)buff_len, MSG_NOSIGNAL, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

			if (ret == 0)
			{
				ret = -EPIPE;

				goto conn_send_to_exit;
			}
			else if (ret == SOCKET_ERROR)
			{
				ret = SOCK_ERRNO;

#ifdef _WIN32
				if (ret == -WSAESHUTDOWN)
				{
					ret = -EPIPE;
				}
#endif
				goto conn_send_to_exit;
			}

			buff_len -= ret;
		}

		ret = 0;
	}
	else
	{
		ret = -ENOTCONN;
	}

conn_send_to_exit:
	mutex_unlock_shared(&priv->mutex);

	return ret;
}

void conn_drop(struct conn_handle *conn)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;

	// First, shutdown any active connections
	mutex_lock_shared(&priv->mutex);

	if (priv->conn_fd == INVALID_SOCKET && priv->fd == INVALID_SOCKET)
	{
		// Nothing to do here
		mutex_unlock_shared(&priv->mutex);

		return;
	}

	if (priv->conn_fd != INVALID_SOCKET)
	{
		shutdown(priv->conn_fd, SHUT_RDWR);
	}

	mutex_unlock_shared(&priv->mutex);

	// Now that we know there will be no one blocking while
	// holding the shared lock, we can get the exclusive lock
	// and close the descriptors
	mutex_lock(&priv->mutex);

	priv->fd = INVALID_SOCKET;

	if (priv->conn_fd != INVALID_SOCKET)
	{
		closesocket(priv->conn_fd);
		priv->conn_fd = INVALID_SOCKET;
	}

	mutex_unlock(&priv->mutex);
}

void conn_close(struct conn_handle *conn)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;

	// First, shutdown any active connections
	conn_shutdown(conn);

	// Now that we know there will be no one blocking while
	// holding the shared lock, we can get the exclusive lock
	// and close the descriptors
	mutex_lock(&priv->mutex);

	priv->fd = INVALID_SOCKET;

	if (priv->conn_fd != INVALID_SOCKET)
	{
		closesocket(priv->conn_fd);
		priv->conn_fd = INVALID_SOCKET;
	}

	if (priv->sock_fd != INVALID_SOCKET)
	{
		closesocket(priv->sock_fd);
		priv->sock_fd = INVALID_SOCKET;
	}

	mutex_unlock(&priv->mutex);
}

void conn_shutdown(struct conn_handle *conn)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;

	mutex_lock_shared(&priv->mutex);

	if (priv->conn_fd != INVALID_SOCKET)
	{
		shutdown(priv->conn_fd, SHUT_RDWR);
	}

	if (priv->sock_fd != INVALID_SOCKET)
	{
		shutdown(priv->sock_fd, SHUT_RDWR);
#ifdef _WIN32
		/// @TODO WIN32 Hack to cancel an in-progress accept
		closesocket(priv->sock_fd);
		priv->sock_fd = INVALID_SOCKET;
#endif
	}

	mutex_unlock_shared(&priv->mutex);
}

void conn_get_remote_addr(const struct conn_handle *conn, char dest[40])
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	struct sockaddr_in *addr = (struct sockaddr_in *)&priv->remote_addr;
	inet_ntop(addr->sin_family, &addr->sin_addr.s_addr, dest, 40);
}

int conn_in_use(struct conn_handle *conn)
{
	struct conn_priv *priv = (struct conn_priv *)conn->priv;
	int ret = 0;

	mutex_lock_shared(&priv->mutex);

	if (priv->fd != INVALID_SOCKET)
	{
		ret = 1;
	}

	mutex_unlock_shared(&priv->mutex);

	return ret;
}
