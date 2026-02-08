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
	char *cookie = NULL;
	char *b64 = NULL;
	long file_size;
	size_t cookie_len, b64_len;
	int ret = -1;

	f = fopen(cookie_path, "r");
	if (!f)
		return -1;

	/* Get file size */
	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (file_size <= 0 || file_size > 4096) {
		fclose(f);
		return -1;
	}

	/* Read entire cookie file */
	cookie = malloc(file_size + 1);
	if (!cookie) {
		fclose(f);
		return -1;
	}

	if (fread(cookie, 1, file_size, f) != (size_t)file_size) {
		fclose(f);
		free(cookie);
		return -1;
	}
	fclose(f);
	cookie[file_size] = '\0';

	/* Strip trailing newlines */
	cookie[strcspn(cookie, "\r\n")] = '\0';
	cookie_len = strlen(cookie);

	/* Calculate base64 output size: ((len + 2) / 3) * 4 + 1 */
	b64_len = ((cookie_len + 2) / 3) * 4 + 1;
	b64 = malloc(b64_len);
	if (!b64) {
		free(cookie);
		return -1;
	}

	base64_encode((unsigned char *)cookie, cookie_len, b64);

	/* Check if result fits in auth buffer */
	if (strlen(b64) + 7 <= sizeof(client->auth)) {  /* 7 = strlen("Basic ") + null */
		snprintf(client->auth, sizeof(client->auth), "Basic %s", b64);
		ret = 0;
	}

	free(cookie);
	free(b64);
	return ret;
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
	size_t cred_len, b64_len;
	char *credentials;
	char *b64;

	/* Calculate required sizes */
	cred_len = strlen(user) + 1 + strlen(pass) + 1;  /* user + ":" + pass + null */
	b64_len = ((cred_len + 2) / 3) * 4 + 1;

	credentials = malloc(cred_len);
	b64 = malloc(b64_len);

	if (!credentials || !b64) {
		free(credentials);
		free(b64);
		client->auth[0] = '\0';
		return;
	}

	snprintf(credentials, cred_len, "%s:%s", user, pass);
	base64_encode((unsigned char *)credentials, strlen(credentials), b64);

	/* Check if result fits in auth buffer */
	if (strlen(b64) + 7 <= sizeof(client->auth)) {
		snprintf(client->auth, sizeof(client->auth), "Basic %s", b64);
	} else {
		client->auth[0] = '\0';  /* Auth too long */
	}

	free(credentials);
	free(b64);
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
	struct hostent *he;
	struct sockaddr_in addr;

	he = gethostbyname(client->host);
	if (!he)
		return -1;

	client->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (client->sock < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(client->port);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

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

static char *read_http_response(int sock)
{
	char *buffer;
	size_t buf_size = 1024 * 1024;
	size_t total = 0;
	ssize_t n;
	char *body_start;
	char *cl_header;
	int content_length = 0;
	size_t header_len;
	size_t body_received;
	int http_status = 0;

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
		if (body_start) {
			body_start += 4;

			/* Check HTTP status code */
			if (strncmp(buffer, "HTTP/1.", 7) == 0)
				sscanf(buffer + 9, "%d", &http_status);

			/* For non-2xx: only return body for HTTP 500 — Bitcoin Core
			 * sends JSON-RPC error objects via HTTP 500.
			 * 401/403/404 have no useful JSON body. */
			if (http_status < 200 || http_status >= 300) {
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
				sscanf(cl_header + 15, "%d", &content_length);

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

			body_start = buffer + header_len;
			char *result = strdup(body_start);
			free(buffer);
			return result;
		}
	}

	free(buffer);
	return NULL;
}

char *rpc_call(RpcClient *client, const char *method, const char *params)
{
	char *body = NULL;
	char *request = NULL;
	char path[1024];
	ssize_t sent;
	size_t body_len, request_len;

	if (client->sock < 0)
		return NULL;

	/* Build path: / or /wallet/<name> */
	if (client->wallet[0])
		snprintf(path, sizeof(path), "/wallet/%s", client->wallet);
	else
		strcpy(path, "/");

	/* Calculate body size: fixed overhead + method + params */
	const char *params_str = params ? params : "[]";
	body_len = strlen(method) + strlen(params_str) + 64;
	body = malloc(body_len);
	if (!body)
		return NULL;

	snprintf(body, body_len,
		"{\"jsonrpc\":\"1.0\",\"id\":\"1\",\"method\":\"%s\",\"params\":%s}",
		method, params_str);

	/* Calculate request size: headers + body */
	request_len = strlen(body) + 512;
	request = malloc(request_len);
	if (!request) {
		free(body);
		return NULL;
	}

	snprintf(request, request_len,
		"POST %s HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Authorization: %s\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %zu\r\n"
		"Connection: keep-alive\r\n"
		"\r\n"
		"%s",
		path,
		client->host, client->port,
		client->auth,
		strlen(body),
		body);

	sent = send(client->sock, request, strlen(request), 0);
	free(body);
	free(request);

	if (sent < 0)
		return NULL;

	return read_http_response(client->sock);
}

void rpc_disconnect(RpcClient *client)
{
	if (client->sock >= 0) {
		close(client->sock);
		client->sock = -1;
	}
}
