/* Bitcoin Core JSON-RPC over raw TCP sockets */

#define _GNU_SOURCE  /* for strdup */
#include "rpc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

static const char b64_table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const unsigned char *input, size_t len, char *output)
{
	size_t i, j;
	for (i = 0, j = 0; i < len; i += 3, j += 4) {
		unsigned int triple = 0;
		int padding = 0;

		triple |= ((unsigned int)input[i]) << 16;
		if (i + 1 < len)
			triple |= ((unsigned int)input[i + 1]) << 8;
		else
			padding++;
		if (i + 2 < len)
			triple |= input[i + 2];
		else
			padding++;

		output[j]     = b64_table[(triple >> 18) & 0x3F];
		output[j + 1] = b64_table[(triple >> 12) & 0x3F];
		output[j + 2] = (padding >= 2) ? '=' : b64_table[(triple >> 6) & 0x3F];
		output[j + 3] = (padding >= 1) ? '=' : b64_table[triple & 0x3F];
	}
	output[j] = '\0';
}

void rpc_init(RpcClient *client, const char *host, int port)
{
	memset(client, 0, sizeof(RpcClient));
	strncpy(client->host, host, sizeof(client->host) - 1);
	client->port = port;
	client->sock = -1;
}

int rpc_auth_cookie(RpcClient *client, const char *cookie_path)
{
	FILE *f;
	char cookie[256];
	char b64[352];  /* ((256+2)/3)*4 + 1 */
	size_t cookie_len;
	size_t nread;

	f = fopen(cookie_path, "r");
	if (!f)
		return -1;

	/* Read cookie (typically ~70 bytes: __cookie__:<hex>) */
	nread = fread(cookie, 1, sizeof(cookie) - 1, f);
	fclose(f);

	if (nread == 0)
		return -1;
	cookie[nread] = '\0';

	/* Strip trailing newlines */
	cookie[strcspn(cookie, "\r\n")] = '\0';
	cookie_len = strlen(cookie);

	base64_encode((unsigned char *)cookie, cookie_len, b64);

	if (strlen(b64) + 7 <= sizeof(client->auth)) {
		snprintf(client->auth, sizeof(client->auth), "Basic %s", b64);
		return 0;
	}

	return -1;
}

void rpc_set_wallet(RpcClient *client, const char *wallet)
{
	if (wallet && wallet[0])
		strncpy(client->wallet, wallet, sizeof(client->wallet) - 1);
	else
		client->wallet[0] = '\0';
}

void rpc_auth_userpass(RpcClient *client, const char *user, const char *pass)
{
	char credentials[512];
	char b64[700];
	int cred_len;

	cred_len = snprintf(credentials, sizeof(credentials), "%s:%s", user, pass);
	if (cred_len >= (int)sizeof(credentials)) {
		client->auth[0] = '\0';
		return;
	}

	base64_encode((unsigned char *)credentials, cred_len, b64);

	if (strlen(b64) + 7 <= sizeof(client->auth))
		snprintf(client->auth, sizeof(client->auth), "Basic %s", b64);
	else
		client->auth[0] = '\0';
}

static int rpc_auth_from_config(RpcClient *client, const char *datadir)
{
	char path[1024];
	FILE *f;
	char line[1024];
	char user[256] = {0};
	char pass[256] = {0};

	snprintf(path, sizeof(path), "%s/bitcoin.conf", datadir);

	f = fopen(path, "r");
	if (!f)
		return -1;

	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
			continue;

		line[strcspn(line, "\r\n")] = '\0';

		if (strncmp(line, "rpcuser=", 8) == 0)
			strncpy(user, line + 8, sizeof(user) - 1);
		if (strncmp(line, "rpcpassword=", 12) == 0)
			strncpy(pass, line + 12, sizeof(pass) - 1);
	}
	fclose(f);

	if (user[0] && pass[0]) {
		rpc_auth_userpass(client, user, pass);
		return 0;
	}

	return -1;
}

int rpc_auth_auto(RpcClient *client, const char *datadir)
{
	char path[1024];
	const char *networks[] = {"signet", "testnet3", "testnet4", "regtest", "", NULL};
	int i;

	/* Try cookie file in each network subdir */
	for (i = 0; networks[i] != NULL; i++) {
		if (networks[i][0])
			snprintf(path, sizeof(path), "%s/%s/.cookie", datadir, networks[i]);
		else
			snprintf(path, sizeof(path), "%s/.cookie", datadir);

		if (rpc_auth_cookie(client, path) == 0)
			return 0;
	}

	/* Fallback to config file */
	if (rpc_auth_from_config(client, datadir) == 0)
		return 0;

	return -1;
}

int rpc_connect(RpcClient *client)
{
	struct sockaddr_in addr;
	in_addr_t ip;

	/* Skip DNS for numeric IP addresses */
	ip = inet_addr(client->host);
	if (ip == INADDR_NONE) {
		struct hostent *he = gethostbyname(client->host);
		if (!he)
			return -1;
		memcpy(&ip, he->h_addr_list[0], sizeof(ip));
	}

	client->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client->sock < 0)
		return -1;

	/* Disable Nagle — send small requests immediately */
	{
		int flag = 1;
		setsockopt(client->sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(client->port);
	addr.sin_addr.s_addr = ip;

	if (connect(client->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(client->sock);
		client->sock = -1;
		return -1;
	}

	/* Apply socket timeout if set */
	if (client->timeout > 0) {
		struct timeval tv;
		tv.tv_sec = client->timeout;
		tv.tv_usec = 0;
		setsockopt(client->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(client->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}

	return 0;
}

static char *read_http_response(int sock, int *http_status_out)
{
	char *buffer;
	size_t buf_size = 4096;
	size_t total = 0;
	ssize_t n;
	char *body_start;
	char *cl_header;
	int content_length = 0;
	size_t header_len;
	size_t body_received;
	int http_status = 0;

	if (http_status_out)
		*http_status_out = 0;

	buffer = malloc(buf_size);
	if (!buffer)
		return NULL;

	while (total < buf_size - 1) {
		n = recv(sock, buffer + total, buf_size - total - 1, 0);
		if (n <= 0)
			break;
		total += n;
		buffer[total] = '\0';

		body_start = strstr(buffer, "\r\n\r\n");
		if (!body_start && total >= buf_size - 1) {
			/* Headers didn't fit — grow buffer */
			buf_size *= 2;
			buffer = realloc(buffer, buf_size);
			if (!buffer)
				return NULL;
			continue;
		}
		if (body_start) {
			body_start += 4;

			/* Check HTTP status code */
			if (strncmp(buffer, "HTTP/1.", 7) == 0)
				http_status = atoi(buffer + 9);

			if (http_status_out)
				*http_status_out = http_status;

			/* For non-2xx: return JSON body for HTTP 500 (RPC errors),
			 * synthetic error for 401 (auth failure),
			 * and NULL for everything else */
			if (http_status < 200 || http_status >= 300) {
				if (http_status == 401) {
					free(buffer);
					return NULL;
				}
				if (http_status != 500) {
					free(buffer);
					return NULL;
				}
				/* HTTP 500: fall through to read JSON error body */
			}

			cl_header = strstr(buffer, "Content-Length:");
			if (!cl_header)
				cl_header = strstr(buffer, "content-length:");
			if (cl_header)
				content_length = atoi(cl_header + 15);

			header_len = body_start - buffer;
			body_received = total - header_len;

			while (body_received < (size_t)content_length) {
				if (total >= buf_size - 1) {
					buf_size *= 2;
					buffer = realloc(buffer, buf_size);
					if (!buffer)
						return NULL;
				}

				n = recv(sock, buffer + total, buf_size - total - 1, 0);
				if (n <= 0)
					break;
				total += n;
				body_received += n;
			}
			buffer[total] = '\0';

			/* Move body to start of buffer to avoid strdup */
			body_start = buffer + header_len;
			body_received = total - header_len;
			memmove(buffer, body_start, body_received + 1);
			return buffer;
		}
	}

	free(buffer);
	return NULL;
}

char *rpc_call(RpcClient *client, const char *method, const char *params)
{
	char body[1024];
	char request[2048];
	char path[512];
	int body_len, req_len;
	ssize_t sent;

	client->last_http_error = 0;

	/* Auto-reconnect if socket was closed (e.g. after wallet switch) */
	if (client->sock < 0) {
		if (rpc_connect(client) < 0)
			return NULL;
	}

	/* Build path: / or /wallet/<name> */
	if (client->wallet[0])
		snprintf(path, sizeof(path), "/wallet/%s", client->wallet);
	else
		path[0] = '/', path[1] = '\0';

	/* Build JSON-RPC body on stack */
	const char *params_str = params ? params : "[]";
	body_len = snprintf(body, sizeof(body),
		"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"%s\",\"params\":%s}",
		method, params_str);

	/* Handle oversized bodies (rare — large params) */
	if (body_len >= (int)sizeof(body)) {
		char *heap_body = malloc(body_len + 1);
		char *heap_req;
		int heap_req_len;
		if (!heap_body) return NULL;
		snprintf(heap_body, body_len + 1,
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"%s\",\"params\":%s}",
			method, params_str);
		heap_req = malloc(body_len + 512);
		if (!heap_req) { free(heap_body); return NULL; }
		heap_req_len = snprintf(heap_req, body_len + 512,
			"POST %s HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Authorization: %s\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %d\r\n"
			"Connection: keep-alive\r\n"
			"\r\n"
			"%s",
			path, client->host, client->port, client->auth,
			body_len, heap_body);
		free(heap_body);
		sent = send(client->sock, heap_req, heap_req_len, 0);
		if (sent < 0) {
			close(client->sock);
			client->sock = -1;
			if (rpc_connect(client) < 0) { free(heap_req); return NULL; }
			sent = send(client->sock, heap_req, heap_req_len, 0);
			if (sent < 0) { free(heap_req); return NULL; }
		}
		free(heap_req);
		goto read_response;
	}

	/* Build full HTTP request on stack — zero heap allocation */
	req_len = snprintf(request, sizeof(request),
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Authorization: %s\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"%s",
		path,
		client->host, client->port,
		client->auth,
		body_len,
		body);

	sent = send(client->sock, request, req_len, 0);

	if (sent < 0) {
		/* Connection may have been closed by server; reconnect and retry once */
		close(client->sock);
		client->sock = -1;
		if (rpc_connect(client) < 0)
			return NULL;
		sent = send(client->sock, request, req_len, 0);
		if (sent < 0)
			return NULL;
	}

read_response:
	{
		int http_status = 0;
		char *result = read_http_response(client->sock, &http_status);
		if (http_status >= 400)
			client->last_http_error = http_status;
		return result;
	}
}

void rpc_disconnect(RpcClient *client)
{
	if (client->sock >= 0) {
		close(client->sock);
		client->sock = -1;
	}
}
