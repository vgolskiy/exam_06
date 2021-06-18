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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/select.h>

typedef struct		s_client {
	int		fd;
	int             id;
	struct s_client	*next;
}	t_client;

t_client*	g_clients;

int	sock_fd, g_id = 0;
fd_set	curr_sock, read_fds, write_fds;
char	msg[42];
char	str[42*2300], tmp[42*2300], buf[42*2300 + 42];

void	exit_error(void) {
	write(STDERR_FILENO, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(EXIT_FAILURE);
}

int		get_id(int fd) {
	t_client*	it = g_clients;

	while (it) {
		if (it->fd == fd)
			return (it->id);
		it = it->next;
	}
	return (-1);
}

int		get_max_fd(void) {
	int		max = sock_fd;
	t_client*	it = g_clients;

	while (it) {
		if (it->fd > max)
			max = it->fd;
		it = it->next;
	}
	return (max);
}

void	send_to_all(int fd, char* s) {
	t_client* it = g_clients;

	while (it) {
		if (it->fd != fd && FD_ISSET(it->fd, &write_fds)) {
			if (send(it->fd, s, strlen(s), 0) < 0)
				exit_error();
        }
		it = it->next;
    }
}

int		add_client_to_list(int fd) {
	t_client*	it = g_clients;
	t_client*	new;

	if (!(new = (t_client *)calloc(1, sizeof(t_client))))
		exit_error();
	new->id = g_id++;
	new->fd = fd;
	new->next = NULL;
	if (!g_clients)
		g_clients = new;
	else {
		while (it->next)
			it = it->next;
		it->next = new;
	}
	return (new->id);
}

void	add_client(void) {
    struct sockaddr_in	clientaddr;
    socklen_t		len = sizeof(clientaddr);
    int			client_fd;

    if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0)
        exit_error();
    sprintf(msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
    send_to_all(client_fd, msg);
    FD_SET(client_fd, &curr_sock);
}

int		delete_client(int fd) {
	t_client*	it = g_clients;
	t_client*	old;
	int		id;

	if (it && it->fd == fd) {
        g_clients = it->next;
		id = it->id;
        free(it);
    }
    else {
        while(it && it->next && it->next->fd != fd)
            it = it->next;
        old = it->next;
        it->next = it->next->next;
		id = old->id;
        free(old);
    }
    return (id);
}

void	exit_msg(int fd) {
    int i = -1;
    int j = 0;

    while (str[++i]) {
        tmp[j++] = str[i];
        if (str[i] == '\n') {
            sprintf(buf, "client %d: %s", get_id(fd), tmp);
            send_to_all(fd, buf);
            j = 0;
            bzero(&tmp, strlen(tmp));
            bzero(&buf, strlen(buf));
        }
    }
    bzero(&str, strlen(str));
}

int		main(int argc, char **argv) {
	if (argc != 2) {
		write(STDERR_FILENO, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in	servaddr;
	uint16_t			port = atoi(argv[1]);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = 127 | (1 << 24);
	servaddr.sin_port = port >> 8 | port << 8;

	if (((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		|| (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		|| (listen(sock_fd, 0) < 0))
		exit_error();

	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&buf, sizeof(buf));
	bzero(&str, sizeof(str));
	while(1) {
        write_fds = read_fds = curr_sock;
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
                        sprintf(msg, "server: client %d just left\n", delete_client(fd));
                        send_to_all(fd, msg);
                        FD_CLR(fd, &curr_sock);
                        close(fd);
                        break;
                    }
                    else
                        exit_msg(fd);
                }
            }   
        }
    }
    return (EXIT_SUCCESS);
}
