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

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct conn_priv
{
	int sock_fd;
	int conn_fd;
	int fd;
	struct sockaddr_storage remote_addr;
	socklen_t remote_addr_len;
	struct mutex_handle mutex;
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

		ret = mutex_init(&priv->mutex);
		if (ret < 0)
		{
			goto conn_init_exit;
		}

		priv->sock_fd = -1;
		priv->conn_fd = -1;
		priv->fd = -1;
	}

	return 0;

conn_init_exit:
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

	// Even though the EL proxy protocol only uses IPv4, there is no
	// reason a future client couldn't connect to the proxy over IPv6
	hints.ai_family = AF_UNSPEC;

	// TODO: Address targeting
	hints.ai_flags = AI_PASSIVE;

	ret = getaddrinfo(NULL, port_str, &hints, &res);
	if (ret != 0)
	{
		return -errno;
	}

	priv->sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (priv->sock_fd < 0)
	{
		ret = -errno;
		goto conn_listen_free;
	}

	ret = bind(priv->sock_fd, res->ai_addr, res->ai_addrlen);
	if (ret < 0)
	{
		ret = -errno;
		goto conn_listen_free;
	}

	if (pc->type == CONN_TYPE_TCP)
	{
		ret = listen(priv->sock_fd, 0);
		if (ret < 0)
		{
			ret = -errno;
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

	mutex_lock_shared(&priv->mutex);

	priv->conn_fd = accept(priv->sock_fd, (struct sockaddr *)&priv->remote_addr, &priv->remote_addr_len);

	mutex_unlock(&priv->mutex);

	if (priv->conn_fd < 0)
	{
		return -errno;
	}

	mutex_lock(&priv->mutex);

	priv->fd = priv->conn_fd;

	mutex_unlock(&priv->mutex);

	return 0;
}

int conn_connect(struct proxy_conn *pc, uint32_t addr, uint16_t port)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	char port_str[8];
	uint8_t *addr_sep = (uint8_t *)&addr;
	char addr_str[16];
	int ret;

	ret = snprintf(port_str, 8, "%hu", port);
	if (ret > 7 || ret < 1)
	{
		return -EINVAL;
	}

	ret = snprintf(addr_str, 16, "%hhu.%hhu.%hhu.%hhu", addr_sep[0], addr_sep[1], addr_sep[2], addr_sep[3]);
	if (ret > 15 || ret < 7)
	{
		return -EINVAL;
	}

	memset(&hints, 0x0, sizeof(struct addrinfo));

	if (pc->type != CONN_TYPE_TCP)
	{
		return -EPROTOTYPE;
	}
	hints.ai_socktype = SOCK_STREAM;

	// Even though the EL proxy protocol only uses IPv4, there is no
	// reason a future client couldn't connect to the proxy over IPv6
	hints.ai_family = AF_UNSPEC;

	ret = getaddrinfo(addr_str, port_str, &hints, &res);
	if (ret != 0)
	{
		return -errno;
	}

	priv->sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (priv->sock_fd < 0)
	{
		ret = -errno;
		goto conn_connect_free;
	}

	ret = connect(priv->sock_fd, res->ai_addr, res->ai_addrlen);
	if (ret < 0)
	{
		ret = -errno;
		goto conn_connect_free;
	}

	mutex_lock(&priv->mutex);

	priv->fd = priv->sock_fd;

	mutex_unlock(&priv->mutex);

conn_connect_free:
	freeaddrinfo(res);

	return ret;
}

int conn_recv(struct proxy_conn *pc, uint8_t *buff, size_t buff_len)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	int ret;

	mutex_lock_shared(&priv->mutex);

	if (pc->type == CONN_TYPE_TCP && priv->fd >= 0)
	{
		while (buff_len > 0)
		{
			ret = recvfrom(priv->fd, buff, buff_len, 0, NULL, NULL);

			if (ret < 1)
			{
				ret = (ret == 0) ? -EPIPE : -errno;

				goto conn_recv_exit;
			}

			buff_len -= ret;
			buff += ret;
		}
	}
	else
	{
		ret = -ENOTCONN;
	}

conn_recv_exit:
	mutex_unlock(&priv->mutex);

	return ret;
}

int conn_recv_any(struct proxy_conn *pc, uint8_t *buff, size_t buff_len, uint32_t *addr)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	uint8_t *addr_sep = (uint8_t *)addr;
	int ret;

	mutex_lock_shared(&priv->mutex);

	if (priv->fd >= 0)
	{
		ret = recvfrom(priv->fd, buff, buff_len, 0, (struct sockaddr *)&priv->remote_addr, &priv->remote_addr_len);

		if (ret < 1)
		{
			ret = (ret == 0) ? -EPIPE : -errno;

			goto conn_recv_exit;
		}
	}
	else
	{
		ret = -ENOTCONN;
	}

conn_recv_exit:
	mutex_unlock(&priv->mutex);

	if (addr != NULL && ret >= 0)
	{
		if (sscanf(inet_ntoa(((struct sockaddr_in *)&priv->remote_addr)->sin_addr), "%hhu.%hhu.%hhu.%hhu", &addr_sep[0], &addr_sep[1], &addr_sep[2], &addr_sep[3]) != 4)
		{
			fprintf(stderr, "bad sccanf\n");
			ret = -EINVAL;
		}
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

	if (priv->fd >= 0)
	{
		while (buff_len > 0)
		{
			ret = send(priv->fd, buff, buff_len, MSG_NOSIGNAL);

			if (ret < 1)
			{
				ret = (ret == 0) ? -EPIPE : -errno;

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
	mutex_unlock(&priv->mutex);

	return ret;
}

int conn_send_to(struct proxy_conn *pc, const uint8_t *buff, size_t buff_len, uint32_t addr, uint16_t port)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	char port_str[8];
	uint8_t *addr_sep = (uint8_t *)&addr;
	char addr_str[16];
	int ret;

	ret = snprintf(port_str, 8, "%hu", port);
	if (ret > 7 || ret < 1)
	{
		return -EINVAL;
	}

	ret = snprintf(addr_str, 16, "%hhu.%hhu.%hhu.%hhu", addr_sep[0], addr_sep[1], addr_sep[2], addr_sep[3]);
	if (ret > 15 || ret < 7)
	{
		return -EINVAL;
	}

	memset(&hints, 0x0, sizeof(struct addrinfo));

	if (pc->type != CONN_TYPE_UDP)
	{
		return -EPROTOTYPE;
	}
	hints.ai_socktype = SOCK_DGRAM;

	// Even though the EL proxy protocol only uses IPv4, there is no
	// reason a future client couldn't connect to the proxy over IPv6
	hints.ai_family = AF_UNSPEC;

	ret = getaddrinfo(addr_str, port_str, &hints, &res);
	if (ret != 0)
	{
		return -errno;
	}

	mutex_lock_shared(&priv->mutex);

	if (priv->fd >= 0)
	{
		while (buff_len > 0)
		{
			ret = sendto(priv->fd, buff, buff_len, MSG_NOSIGNAL, res->ai_addr, res->ai_addrlen);

			if (ret < 1)
			{
				ret = (ret == 0) ? -EPIPE : -errno;

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
	mutex_unlock(&priv->mutex);

	freeaddrinfo(res);

	return ret;
}

void conn_drop(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

	// First, shutdown any active connections
	mutex_lock_shared(&priv->mutex);

	if (priv->conn_fd < 0 && priv->fd < 0)
	{
		// Nothing to do here
		mutex_unlock(&priv->mutex);

		return;
	}

	if (priv->conn_fd >= 0)
	{
		shutdown(priv->conn_fd, SHUT_RDWR);
	}

	mutex_unlock(&priv->mutex);

	// Now that we know there will be no one blocking while
	// holding the shared lock, we can get the exclusive lock
	// and close the descriptors
	mutex_lock(&priv->mutex);

	priv->fd = -1;

	if (priv->conn_fd >= 0)
	{
		close(priv->conn_fd);
		priv->conn_fd = -1;
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

	priv->fd = -1;

	if (priv->conn_fd >= 0)
	{
		close(priv->conn_fd);
		priv->conn_fd = -1;
	}

	if (priv->sock_fd >= 0)
	{
		close(priv->sock_fd);
		priv->sock_fd = -1;
	}

	mutex_unlock(&priv->mutex);
}

void conn_shutdown(struct proxy_conn *pc)
{
	struct conn_priv *priv = (struct conn_priv *)pc->priv;

	mutex_lock_shared(&priv->mutex);

	if (priv->conn_fd < 0 && priv->sock_fd < 0 && priv->fd < 0)
	{
		// Nothing to do here
		mutex_unlock(&priv->mutex);

		return;
	}

	if (priv->conn_fd >= 0)
	{
		shutdown(priv->conn_fd, SHUT_RDWR);
	}

	if (priv->sock_fd >= 0)
	{
		shutdown(priv->sock_fd, SHUT_RDWR);
	}

	mutex_unlock(&priv->mutex);
}
