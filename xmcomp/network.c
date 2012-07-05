#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "sha1/sha1.h"

#include "network.h"

#define STANDARD_BUFFER_SIZE 1024

int net_connect(char *host, int port) {
	struct sockaddr_in server_addr;
	struct hostent *s_ent;
	int sock, error;

	LDEBUG("connecting to '%s:%d'", host, port);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	server_addr.sin_addr.s_addr = inet_addr(host);
	if (server_addr.sin_addr.s_addr == (u_int)-1) {
		s_ent = gethostbyname(host);
		if (!s_ent) {
			LERROR("cannot resolve host '%s'", host);
			return -1;
		}
		server_addr.sin_family = s_ent->h_addrtype;
		memcpy(&server_addr.sin_addr, s_ent->h_addr, s_ent->h_length);
	}
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == -1) {
		error = errno;
		LERROR("cannot create socket: %d: %s", error, strerror(error));
		return -1;
	}

	if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
		error = errno;
		LERROR("cannot connect: %d: %s", error, strerror(error));
		close(sock);
		return -1;
	}

	LDEBUG("connected to '%s:%d', socket %d", host, port, sock);

	return sock;
}

int net_send(int sock, char *data, int size) {
	int bytes_sent, error;
	
	LDEBUG("sending %d bytes via %d: '%.*s'", size, sock, size, data);
	bytes_sent = send(sock, data, size, 0);
	LDEBUG("%d bytes sent", bytes_sent);

	if (bytes_sent < 0) {
		error = errno;
		LERROR("failed to send buffer of size %d via %d: %d: %s",
				size, sock, error, strerror(error));
		return -1;
	}

	return bytes_sent;
}

int net_send_all(int sock, char *data, int size) {
	int bytes_sent, error;

	LDEBUG("sending %d bytes via %d: '%.*s'", size, sock, size, data);

	while (size > 0) {
		bytes_sent = send(sock, data, size, 0);
		if (bytes_sent < 0) {
			error = errno;
			LERROR("failed to send %d bytes via %d: %d: %s",
					size, sock, error, strerror(error));
			return -1;
		}

		size -= bytes_sent;
		data += bytes_sent;
	}

	return 0;
}

#define HANDSHAKE_SUCCESS "<handshake/>"
int authorize(int sock, char *id, char *pass) {
	char buffer[STANDARD_BUFFER_SIZE];
	int length;
	SHA1Context sha;

	LDEBUG("authorizing for id '%s' on %d", id, sock);

	strncpy(buffer, id, STANDARD_BUFFER_SIZE);
	strncat(buffer, pass, STANDARD_BUFFER_SIZE - strlen(buffer) - 1);

	SHA1Reset(&sha);
	SHA1Input(&sha, (unsigned char *)buffer, strlen(buffer));
	if (!SHA1Result(&sha)) {
		LERROR("failed to compute SHA1 digest for authentication");
		return 1;
	}

	strcpy(buffer, "<handshake>");
	sprintf(buffer + strlen(buffer), "%08x%08x%08x%08x%08x",
			sha.Message_Digest[0],
			sha.Message_Digest[1],
			sha.Message_Digest[2],
			sha.Message_Digest[3],
			sha.Message_Digest[4]);
	strcat(buffer, "</handshake>");

	if (net_send_all(sock, buffer, strlen(buffer))) {
		LERROR("failed to send SHA-1 handshake request");
		return 1;
	}

	length = net_recv(sock, buffer, STANDARD_BUFFER_SIZE-1);
	if (length < 0) {
		LERROR("failed to receive handshake response");
		return 1;
	}

	buffer[length] = 0;
	if (strcmp(buffer, HANDSHAKE_SUCCESS)) {
		LERROR("cannot authorize: server acknowledgement is '%s', expected '%s'",
				buffer, HANDSHAKE_SUCCESS);
		return 1;
	}

	return 0;
}

int net_stream(int sock, char *from, char *to, char *pass) {
	char buffer[STANDARD_BUFFER_SIZE], quote[2] = { 0, 0 }, *id_start, *id_end;
	int length;

	LDEBUG("opening stream from '%s' to '%s' on %d", from, to, sock);

	length = sprintf(buffer,
		"<?xml version='1.0'?> "
		"<stream:stream "
			"xmlns:stream='http://etherx.jabber.org/streams' "
			"xmlns='jabber:component:accept' "
			"to='%s' "
			"from='%s'>", to, from);
	if (net_send_all(sock, buffer, length)) {
		LERROR("failed to send stream opening stanza from '%s' to '%s' via %d", from, to, sock);
		return 1;
	}

	length = net_recv(sock, buffer, STANDARD_BUFFER_SIZE-1);
	if (length < 0) {
		LERROR("failed to receive stream opening stanza from '%s' via %d", to, sock);
		return 1;
	}

	buffer[length] = 0;
	LDEBUG("received stanza '%s' from '%s' via %d", buffer, to, sock);

	id_start = strstr(buffer, "id=");
	if (!id_start) {
		LERROR("failed to find 'id' attribute in the server response");
		return 1;
	}

	id_start += 3;
	if (!*id_start) {
		LERROR("malformed stanza received");
		return 1;
	}
	quote[0] = *id_start;

	++id_start;
	id_end = strstr(id_start, quote);
	if (!id_end) {
		LERROR("malformed stanza received");
		return 1;
	}

	*id_end = 0;
	LDEBUG("received stream id = '%s'", id_start);

	return authorize(sock, id_start, pass);
}

int net_unstream(int sock) {
	static char *end_stream = "</stream:stream>";

	LDEBUG("closing stream on %d", sock);

	if (net_send_all(sock, end_stream, strlen(end_stream))) {
		LERROR("failed to close stream on %d", sock);
		return 1;
	}

	return 0;
}

int net_recv(int sock, char *buffer, int size) {
/* Wait for "timeout" seconds for the socket to become readable
readable_timeout(int sock, int timeout)
{
    struct timeval tv;
    fd_set         rset;
    int            isready;

    FD_ZERO(&rset);
    FD_SET(sock, &rset);

    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

 again:
    isready = select(sock+1, &rset, NULL, NULL, &tv);
    if (isready < 0) {
        if (errno == EINTR) goto again;
        perror("select"); _exit(1);
    }

    return isready;
}*/

	int bytes_received, error;

	LDEBUG("receiving %d bytes via %d", size, sock);

	if ((bytes_received = recv(sock, buffer, size, 0)) < 0) {
		error = errno;
		LERROR("failed to receive %d bytes from socket %d: %d: %s",
				size, sock, error, strerror(error));
		return -1;
	}

	LDEBUG("received %d bytes: '%.*s'", bytes_received, bytes_received, buffer);

	return bytes_received;
}

void net_disconnect(int sock) {
	LDEBUG("disconnecting %d", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
}
