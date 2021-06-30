/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   miniserver.c.                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mskinner <v.golskiy@ya.ru>                 +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2021/06/18 17:08:48 by mskinner          #+#    #+#             */
/*   Updated: 2021/06/18 18:40:01 by mskinner         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>

typedef struct	s_cli {
	int id;
	int fd;
	struct s_cli* next;
}		t_cli;

t_cli*	g_cli = NULL;
fd_set	write_fds, read_fds, curr_fds;
int	g_id = 0, sock_fd;
char	msg[42];
char	str[42 * 4096], tmp[42 * 4096], buf[42 * 4096 + 42];

int	get_id(int fd) {
	t_cli*	c = g_cli;

	while (c) {
		if (c->fd == fd)
			return (c->id);
		c = c->next;
	}
	return (-1);
}

int	get_max_fd(void) {
	int	max = sock_fd;
	t_cli*	c = g_cli;

	while (c) {
		if (c->fd > max)
			max = c->fd;
		c = c->next;
	}
	return (max);	
}

void	exit_error(void) {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

void	send_to_all(int fd, char *s) {
	t_cli*	c = g_cli;

	while (c) {
		if (c->fd != fd && FD_ISSET(c->fd, &curr_fds)) {
			if (send(c->fd, s, strlen(s), 0) < 0)
				exit_error();
		}
		c = c->next;
	}
}

int	add_cli_to_s(int fd) {
	t_cli*	c = g_cli;
	t_cli*	new;

	if (!(new = (t_cli*)calloc(1, sizeof(t_cli))))
		exit_error();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;
	if (!c)
		g_cli = new;
	else {
		while (c->next)
			c = c->next;
		c->next = new;
	}
	return (new->id);
}

void	add_client(void) {
	struct sockaddr_in	cli_addr;
	socklen_t		len = sizeof(cli_addr);
	int			cli_fd;

	if ((cli_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &len)) < 0)
		exit_error();
	sprintf(msg, "server: client %d just arrived\n", add_cli_to_s(cli_fd));
	send_to_all(cli_fd, msg);
	FD_SET(cli_fd, &curr_fds);
}

int	del_client(int fd) {
	t_cli*	c = g_cli;
	t_cli*	old;
	int	id = get_id(fd);

	if (c) {
		if (c->fd == fd) {
			g_cli = c->next;
			free(c);
		}
		else {
			while (c->next && c->next->fd != fd)
				c = c->next;
			old = c->next;
			c->next = c->next->next;
			free(old);
		}
	}
	return (id);
}

void	exchange_msg(int fd) {
	int i = -1, j = 0;

	while (str[++i]) {
		tmp[j++] = str[i];
		if (str[i] == '\n') {
			sprintf(buf, "client %d: %s", get_id(fd), tmp);
			send_to_all(fd, buf);
			j = 0;
			bzero(&buf, strlen(buf));
			bzero(&tmp, strlen(tmp));
		}
	}
	bzero(&str, strlen(str));
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}
	struct sockaddr_in	serv_addr;
	uint16_t		port = atoi(argv[1]);

	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = 127 | (1 << 24);
	serv_addr.sin_port = htons(port);

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		exit_error();
	if (bind(sock_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		exit_error();
	if (listen(sock_fd, 0) < 0)
		exit_error();
	FD_ZERO(&curr_fds);
	FD_SET(sock_fd, &curr_fds);
	bzero(&str, sizeof(str));
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	while (1) {
		write_fds = read_fds = curr_fds;
		if (select(get_max_fd() + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= get_max_fd(); ++fd) {
			if (FD_ISSET(fd, &read_fds)) {
				if (fd == sock_fd) {
					bzero(&msg, sizeof(msg));
					add_client();
					break;
				}
				else {
					if (recv(fd, str, sizeof(str), 0) <= 0) {
						bzero(&msg, sizeof(msg));
						sprintf(msg, "server: client %d just left\n", del_client(fd));
						send_to_all(fd, msg);
						FD_CLR(fd, &curr_fds);
						close(fd);
						break;
					}
					else
						exchange_msg(fd);
				}
			}
		}
	}
	return (0);
}

