/*!
 * @file conn.c
 *
 * @section LICENSE
 *
 * Copyright &copy; 2015, Scott K Logan
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of OpenELP nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * EchoLink&reg; is a registered trademark of Synergenics, LLC
 *
 * @author Scott K Logan <logans@cottsay.net>
 */

#include "conn.h"
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

#ifndef _WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#ifdef _WIN32
#  define MSG_NOSIGNAL 0
#  define SHUT_RDWR SD_BOTH
#  define SOCK_ERRNO -WSAGetLastError()
#else
#  define SOCKET_ERROR -1
#  define INVALID_SOCKET -1
#  define SOCK_ERRNO -errno
#  define closesocket(X) close(X)
#endif

struct conn_priv
{
	int sock_fd;
	int conn_fd;
	int fd;
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len;
	struct mutex_handle mutex;
#ifdef _WIN32
	WSADATA wsadat;
#endif
};

int conn_init(struct proxy_conn *pc)
{
	int ret;

	if (pc != NULL)
	{
		struct conn_priv *priv;

		if (pc->priv == NULL)
		{
			pc->priv = malloc(sizeof(struct conn_priv));
		}

		if (pc->priv == NULL)
		{
			return -ENOMEM;
		}

		memset(pc->priv, 0x0, sizeof(struct conn_priv));

		priv = (struct conn_priv *)pc->priv;

#ifdef _WIN32
		ret = WSAStartup(MAKEWORD(2,2),&priv->wsadat);
		if (ret != 0)
		{
			// TODO: Decode why
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
	}

	return 0;

conn_init_exit:
#ifdef _WIN32
	WSACleanup();
conn_init_exit_pre:
#endif
	free (pc->priv);
	pc->priv = NULL;

	return ret;
}

void conn_free(struct proxy_conn *pc)
{
	if (pc->priv != NULL)
	{
		struct conn_priv *priv = (struct conn_priv *)pc->priv;

		conn_close(pc);

#ifdef _WIN32
		WSACleanup();
#endif

		mutex_free(&priv->mutex);

		free(pc->priv);
	}
}

int conn_listen(struct proxy_conn *pc, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	char port_str[6];
	int ret;
	const char yes = 1;

	// TODO: Optimize
	ret = snprintf(port_str, 6, "%hu", port);
	if (ret > 5 || ret < 1)
	{
		return -EINVAL;
	}

	memset(&hints, 0x0, sizeof(struct addrinfo));

	switch(pc->type)
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

	// TODO: Address targeting
	hints.ai_flags = AI_PASSIVE;

	ret = getaddrinfo(NULL, port_str, &hints, &res);
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

	ret = setsockopt(priv->sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (ret == SOCKET_ERROR)
	{
		// TODO: Close priv->sock_fd
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}

	ret = bind(priv->sock_fd, res->ai_addr, res->ai_addrlen);
	if (ret == SOCKET_ERROR)
	{
		// TODO: Close priv->sock_fd
		ret = SOCK_ERRNO;
		goto conn_listen_free;
	}

	if (pc->type == CONN_TYPE_TCP)
	{
		ret = listen(priv->sock_fd, 1);
		if (ret == SOCKET_ERROR)
		{
			// TODO: Close priv->sock_fd
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

int conn_listen_wait(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

	priv->remote_addr_len = sizeof(struct sockaddr_storage);

	mutex_lock_shared(&priv->mutex);

	priv->conn_fd = accept(priv->sock_fd, (struct sockaddr *)&priv->remote_addr, &priv->remote_addr_len);

	mutex_unlock_shared(&priv->mutex);

	if (priv->conn_fd == INVALID_SOCKET)
	{
		return SOCK_ERRNO;
	}

	mutex_lock(&priv->mutex);

	priv->fd = priv->conn_fd;

	mutex_unlock(&priv->mutex);

	return 0;
}

int conn_connect(struct proxy_conn *pc, uint32_t addr, uint16_t port)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	struct sockaddr_in saddr;
	int ret;

	if (pc->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}

	memset(&saddr, 0x0, sizeof(struct sockaddr_in));

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = addr;

	priv->sock_fd = socket(saddr.sin_family, SOCK_STREAM, IPPROTO_TCP);
	if (priv->sock_fd == INVALID_SOCKET)
	{
		return SOCK_ERRNO;
	}

	ret = connect(priv->sock_fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;
		goto conn_connect_free;
	}

	mutex_lock(&priv->mutex);

	priv->fd = priv->sock_fd;

	mutex_unlock(&priv->mutex);

	return 0;

conn_connect_free:
	shutdown(priv->sock_fd, SHUT_RDWR);

	closesocket(priv->sock_fd);

	priv->sock_fd = INVALID_SOCKET;

	return ret;
}

int conn_recv(struct proxy_conn *pc, uint8_t *buff, size_t buff_len)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	int ret = 0;

	if (pc->type != CONN_TYPE_TCP)
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
		ret = recvfrom(priv->fd, (char *)buff, buff_len, 0, NULL, NULL);

		if (ret == 0)
		{
			ret = -EPIPE;

			goto conn_recv_exit;
		}
		else if (ret == SOCKET_ERROR)
		{
			ret = SOCK_ERRNO;

#ifdef _WIN32
			// TODO: WIN32 Hack
			if (ret == -WSAECONNRESET)
			{
				ret = -ECONNRESET;
			}
#endif

			goto conn_recv_exit;
		}

		buff_len -= ret;
		buff += ret;
	}

conn_recv_exit:
	mutex_unlock_shared(&priv->mutex);

	return ret;
}

int conn_recv_any(struct proxy_conn *pc, uint8_t *buff, size_t buff_len, uint32_t *addr)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	int ret;

	priv->remote_addr_len = sizeof(struct sockaddr_storage);

	mutex_lock_shared(&priv->mutex);

	if (priv->fd == INVALID_SOCKET)
	{
		ret = -ENOTCONN;

		goto conn_recv_any_exit;
	}

	ret = recvfrom(priv->fd, (char *)buff, buff_len, 0, (struct sockaddr *)&priv->remote_addr, &priv->remote_addr_len);

	if (ret == 0)
	{
		ret = -EPIPE;
	}
	else if (ret == SOCKET_ERROR)
	{
		ret = SOCK_ERRNO;

#ifdef _WIN32
		// TODO: WIN32 Hack
		if (ret == -WSAECONNRESET)
		{
			ret = -ECONNRESET;
		}
#endif
	}

conn_recv_any_exit:
	mutex_unlock_shared(&priv->mutex);

	if (addr != NULL && ret > 0)
	{
		*addr = ((struct sockaddr_in *)&priv->remote_addr)->sin_addr.s_addr;
	}

	return ret;
}

int conn_send(struct proxy_conn *pc, const uint8_t *buff, size_t buff_len)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	int ret;

	if (pc->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}

	mutex_lock_shared(&priv->mutex);

	if (priv->fd != INVALID_SOCKET)
	{
		while (buff_len > 0)
		{
			ret = send(priv->fd, (char *)buff, buff_len, MSG_NOSIGNAL);

			if (ret == 0)
			{
				ret = -EPIPE;

				goto conn_send_exit;
			}
			else if (ret == SOCKET_ERROR)
			{
				ret = SOCK_ERRNO;

#ifdef _WIN32
				// TODO: WIN32 Hack
				if (ret == -WSAECONNRESET)
				{
					ret = -ECONNRESET;
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

int conn_send_to(struct proxy_conn *pc, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	struct sockaddr_in saddr;
	int ret;

	if (pc->type != CONN_TYPE_UDP)
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
			ret = sendto(priv->fd, (char *)buff, buff_len, MSG_NOSIGNAL, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

			if (ret == 0)
			{
				ret = -EPIPE;

				goto conn_send_to_exit;
			}
			else if (ret == SOCKET_ERROR)
			{
				ret = SOCK_ERRNO;

#ifdef _WIN32
				// TODO: WIN32 Hack
				if (ret == -WSAECONNRESET)
				{
					ret = -ECONNRESET;
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

void conn_drop(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

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

void conn_close(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

	// First, shutdown any active connections
	conn_shutdown(pc);

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

void conn_shutdown(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

	mutex_lock_shared(&priv->mutex);

	if (priv->conn_fd != INVALID_SOCKET)
	{
		shutdown(priv->conn_fd, SHUT_RDWR);
	}

	if (priv->sock_fd != INVALID_SOCKET)
	{
		shutdown(priv->sock_fd, SHUT_RDWR);
	}

	mutex_unlock_shared(&priv->mutex);
}
