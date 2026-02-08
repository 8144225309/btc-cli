/* Fallback broadcast: send transactions via public APIs and P2P */

#include "fallback.h"
#include "p2p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

/* ===== Hex utilities ===== */

static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* Convert hex string to bytes. Returns number of bytes written, -1 on error. */
static int hex_to_bytes(const char *hex, uint8_t *out, size_t max_out)
{
	size_t hex_len = strlen(hex);
	size_t byte_len = hex_len / 2;
	size_t i;

	if (hex_len % 2 != 0 || byte_len > max_out)
		return -1;

	for (i = 0; i < byte_len; i++) {
		int hi = hex_nibble(hex[i * 2]);
		int lo = hex_nibble(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return -1;
		out[i] = (uint8_t)((hi << 4) | lo);
	}

	return (int)byte_len;
}

/* ===== URL parsing ===== */

static int parse_url(const char *url, char *host, size_t host_size,
                     int *port, char *path, size_t path_size, int *use_tls)
{
	const char *p = url;
	const char *slash;
	const char *colon;
	size_t hlen;

	*use_tls = 0;
	*port = 80;

	if (strncmp(p, "https://", 8) == 0) {
		*use_tls = 1;
		*port = 443;
		p += 8;
	} else if (strncmp(p, "http://", 7) == 0) {
		p += 7;
	}

	/* Find path separator */
	slash = strchr(p, '/');
	if (slash) {
		strncpy(path, slash, path_size - 1);
		path[path_size - 1] = '\0';
		hlen = (size_t)(slash - p);
	} else {
		strncpy(path, "/", path_size - 1);
		hlen = strlen(p);
	}

	if (hlen >= host_size)
		hlen = host_size - 1;
	memcpy(host, p, hlen);
	host[hlen] = '\0';

	/* Check for explicit port in host */
	colon = strchr(host, ':');
	if (colon) {
		*port = atoi(colon + 1);
		/* Truncate host at colon */
		host[colon - host] = '\0';
	}

	return 0;
}

/* ===== Plain HTTP POST client ===== */

/* Read HTTP response body from socket. Returns allocated string or NULL. */
static char *http_read_response(int sock)
{
	char *buf;
	size_t buf_size = 32768;
	size_t total = 0;
	ssize_t n;
	char *body;
	int http_status = 0;

	buf = malloc(buf_size);
	if (!buf)
		return NULL;

	while (total < buf_size - 1) {
		n = recv(sock, buf + total, buf_size - total - 1, 0);
		if (n <= 0)
			break;
		total += n;
		buf[total] = '\0';

		/* Check if we have full headers + some body */
		body = strstr(buf, "\r\n\r\n");
		if (body) {
			body += 4;

			/* Parse status */
			if (strncmp(buf, "HTTP/1.", 7) == 0)
				sscanf(buf + 9, "%d", &http_status);

			/* For Connection: close, keep reading until EOF */
			/* We already broke out of the header-scanning loop,
			 * now drain remaining data */
			while (1) {
				if (total >= buf_size - 1) {
					buf_size *= 2;
					buf = realloc(buf, buf_size);
					if (!buf)
						return NULL;
				}
				n = recv(sock, buf + total, buf_size - total - 1, 0);
				if (n <= 0)
					break;
				total += n;
			}
			buf[total] = '\0';

			/* Return just the body */
			body = strstr(buf, "\r\n\r\n");
			if (body) {
				body += 4;
				char *result = strdup(body);
				free(buf);
				return result;
			}
		}
	}

	free(buf);
	return NULL;
}

/* POST to host:port/path with given body. Plain HTTP only.
 * response_out: allocated string with response body (caller frees), or NULL.
 * Returns HTTP status code, or -1 on error.
 */
static int http_post(const char *host, int port, const char *path,
                     const char *content_type, const char *body,
                     char **response_out)
{
	struct hostent *he;
	struct sockaddr_in addr;
	int sock;
	char *request;
	size_t body_len, req_len;
	ssize_t sent;
	struct timeval tv;

	*response_out = NULL;

	he = gethostbyname(host);
	if (!he)
		return -1;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	/* 10 second timeout */
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	/* Build HTTP request */
	body_len = strlen(body);
	req_len = body_len + 512;
	request = malloc(req_len);
	if (!request) {
		close(sock);
		return -1;
	}

	snprintf(request, req_len,
		"POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		path, host, content_type, body_len, body);

	sent = send(sock, request, strlen(request), 0);
	free(request);

	if (sent < 0) {
		close(sock);
		return -1;
	}

	*response_out = http_read_response(sock);
	close(sock);

	return (*response_out) ? 0 : -1;
}

/* ===== TLS stub ===== */

static int https_post(const char *host, int port, const char *path,
                      const char *content_type, const char *body,
                      char **response_out)
{
	(void)host; (void)port; (void)path;
	(void)content_type; (void)body;
	*response_out = NULL;
	/* TLS not wired up yet — will be added with BearSSL or similar */
	return -1;
}

/* ===== Esplora API (mempool.space / blockstream / custom) ===== */

/* Get Esplora API path for network */
static const char *esplora_path(Network net)
{
	switch (net) {
	case NET_TESTNET: return "/testnet/api/tx";
	case NET_SIGNET:  return "/signet/api/tx";
	default:          return "/api/tx";
	}
}

static void fallback_esplora(const char *host, int port, const char *path,
                             int use_tls, const char *hex,
                             const char *source_name, FallbackResult *r)
{
	char *response = NULL;
	int ret;

	r->source = source_name;
	r->success = 0;

	if (use_tls) {
		ret = https_post(host, port, path, "text/plain", hex, &response);
		if (ret < 0) {
			snprintf(r->error, sizeof(r->error),
			         "TLS not available (will be added with BearSSL)");
			return;
		}
	} else {
		ret = http_post(host, port, path, "text/plain", hex, &response);
		if (ret < 0) {
			snprintf(r->error, sizeof(r->error), "HTTP POST failed");
			return;
		}
	}

	if (response) {
		/* Esplora returns the txid as plain text on success,
		 * or an error message on failure */
		size_t len = strlen(response);
		/* Strip trailing whitespace */
		while (len > 0 && (response[len-1] == '\n' || response[len-1] == '\r' ||
		       response[len-1] == ' '))
			response[--len] = '\0';

		if (len == 64) {
			/* Looks like a txid */
			strncpy(r->txid, response, sizeof(r->txid) - 1);
			r->success = 1;
		} else if (len > 0) {
			/* Error message from API */
			strncpy(r->error, response,
			        sizeof(r->error) - 1);
		} else {
			snprintf(r->error, sizeof(r->error), "empty response");
		}
		free(response);
	} else {
		snprintf(r->error, sizeof(r->error), "no response");
	}
}

/* ===== Blockchair API ===== */

static void fallback_blockchair_api(const char *hex, Network net,
                                    FallbackResult *r)
{
	const char *path;
	char *body;
	char *response = NULL;
	size_t body_len;
	int ret;

	r->source = "blockchair";
	r->success = 0;

	switch (net) {
	case NET_TESTNET:
		path = "/bitcoin/testnet/push/transaction";
		break;
	default:
		path = "/bitcoin/push/transaction";
		break;
	}

	/* Blockchair uses form-encoded body: data=<hex> */
	body_len = strlen(hex) + 8;
	body = malloc(body_len);
	if (!body) {
		snprintf(r->error, sizeof(r->error), "allocation failed");
		return;
	}
	snprintf(body, body_len, "data=%s", hex);

	ret = https_post("api.blockchair.com", 443, path,
	                 "application/x-www-form-urlencoded", body, &response);
	free(body);

	if (ret < 0) {
		snprintf(r->error, sizeof(r->error),
		         "TLS not available (will be added with BearSSL)");
		free(response);
		return;
	}

	if (response) {
		/* Blockchair returns JSON with data.transaction_hash on success,
		 * or data:null on error. Check for transaction_hash specifically. */
		if (strstr(response, "\"transaction_hash\""))
			r->success = 1;
		else
			strncpy(r->error, response, sizeof(r->error) - 1);
		free(response);
	} else {
		snprintf(r->error, sizeof(r->error), "no response");
	}
}

/* ===== blockchain.info API ===== */

static void fallback_blockchain_info(const char *hex, FallbackResult *r)
{
	char *body;
	char *response = NULL;
	size_t body_len;
	int ret;

	r->source = "blockchain.info";
	r->success = 0;

	/* blockchain.info uses form-encoded body: tx=<hex> */
	body_len = strlen(hex) + 8;
	body = malloc(body_len);
	if (!body) {
		snprintf(r->error, sizeof(r->error), "allocation failed");
		return;
	}
	snprintf(body, body_len, "tx=%s", hex);

	ret = https_post("blockchain.info", 443, "/pushtx",
	                 "application/x-www-form-urlencoded", body, &response);
	free(body);

	if (ret < 0) {
		snprintf(r->error, sizeof(r->error),
		         "TLS not available (will be added with BearSSL)");
		free(response);
		return;
	}

	if (response) {
		/* blockchain.info returns "Transaction Submitted" on success */
		if (strstr(response, "Transaction Submitted"))
			r->success = 1;
		else
			strncpy(r->error, response, sizeof(r->error) - 1);
		free(response);
	} else {
		snprintf(r->error, sizeof(r->error), "no response");
	}
}

/* ===== BlockCypher API ===== */

static void fallback_blockcypher_api(const char *hex, Network net,
                                     FallbackResult *r)
{
	const char *chain;
	char path[128];
	char *body;
	char *response = NULL;
	size_t body_len;
	int ret;

	r->source = "blockcypher";
	r->success = 0;

	switch (net) {
	case NET_TESTNET:
		chain = "btc/test3";
		break;
	default:
		chain = "btc/main";
		break;
	}

	snprintf(path, sizeof(path), "/v1/%s/txs/push", chain);

	/* BlockCypher uses JSON body: {"tx":"<hex>"} */
	body_len = strlen(hex) + 16;
	body = malloc(body_len);
	if (!body) {
		snprintf(r->error, sizeof(r->error), "allocation failed");
		return;
	}
	snprintf(body, body_len, "{\"tx\":\"%s\"}", hex);

	ret = https_post("api.blockcypher.com", 443, path,
	                 "application/json", body, &response);
	free(body);

	if (ret < 0) {
		snprintf(r->error, sizeof(r->error),
		         "TLS not available (will be added with BearSSL)");
		free(response);
		return;
	}

	if (response) {
		/* BlockCypher returns JSON with "hash" field on success */
		if (strstr(response, "\"hash\""))
			r->success = 1;
		else
			strncpy(r->error, response, sizeof(r->error) - 1);
		free(response);
	} else {
		snprintf(r->error, sizeof(r->error), "no response");
	}
}

/* ===== P2P direct broadcast ===== */

/* Fisher-Yates shuffle */
static void shuffle_ips(char **ips, int count)
{
	int i, j;
	char *tmp;

	srand((unsigned int)time(NULL));
	for (i = count - 1; i > 0; i--) {
		j = rand() % (i + 1);
		tmp = ips[i];
		ips[i] = ips[j];
		ips[j] = tmp;
	}
}

static void fallback_p2p(const char *hex, Network net, int num_peers,
                         FallbackResult *r)
{
	uint8_t *tx_data = NULL;
	size_t hex_len = strlen(hex);
	size_t tx_len = hex_len / 2;
	char **ips = NULL;
	int ip_count;
	uint32_t magic;
	int port;
	int sent = 0;
	int tried = 0;
	int i;

	r->source = "p2p-broadcast";
	r->success = 0;

	/* Convert hex to raw bytes */
	tx_data = malloc(tx_len);
	if (!tx_data) {
		snprintf(r->error, sizeof(r->error), "allocation failed");
		return;
	}

	if (hex_to_bytes(hex, tx_data, tx_len) < 0) {
		snprintf(r->error, sizeof(r->error), "invalid transaction hex");
		free(tx_data);
		return;
	}

	magic = p2p_magic(net);
	port = p2p_port(net);

	/* DNS seed lookup */
	ip_count = p2p_dns_seed_lookup(net, &ips, 64);
	if (ip_count == 0) {
		/* For regtest, try localhost */
		if (net == NET_REGTEST) {
			P2pPeer peer;
			if (p2p_connect(&peer, "127.0.0.1", port, magic, 5) == 0) {
				if (p2p_handshake(&peer) == 0) {
					if (p2p_send_tx(&peer, tx_data, tx_len) == 0)
						sent++;
				}
				usleep(100000);  /* 100ms — let TCP flush */
				p2p_disconnect(&peer);
			}
			free(tx_data);
			if (sent > 0) {
				r->success = 1;
				snprintf(r->txid, sizeof(r->txid),
				         "broadcast to %d peer(s)", sent);
			} else {
				snprintf(r->error, sizeof(r->error),
				         "no peers available");
			}
			return;
		}
		snprintf(r->error, sizeof(r->error),
		         "no peers found via DNS seeds");
		free(tx_data);
		return;
	}

	/* Shuffle for randomness */
	shuffle_ips(ips, ip_count);

	fprintf(stderr, "P2P broadcast: found %d peers, sending to %d...\n",
	        ip_count, num_peers);

	/* Connect and broadcast to N peers */
	for (i = 0; i < ip_count && sent < num_peers; i++) {
		P2pPeer peer;

		if (p2p_connect(&peer, ips[i], port, magic, 5) < 0)
			continue;

		if (p2p_handshake(&peer) < 0) {
			p2p_disconnect(&peer);
			continue;
		}

		tried++;
		if (p2p_send_tx(&peer, tx_data, tx_len) == 0) {
			sent++;
			fprintf(stderr, "  -> %s:%d OK\n", ips[i], port);
		}

		/* Brief pause before disconnect — let TCP flush the tx
		 * to the peer before we close the socket */
		usleep(100000);  /* 100ms */
		p2p_disconnect(&peer);
	}

	/* Cleanup */
	for (i = 0; i < ip_count; i++)
		free(ips[i]);
	free(ips);
	free(tx_data);

	if (sent > 0) {
		r->success = 1;
		snprintf(r->txid, sizeof(r->txid),
		         "broadcast to %d/%d peers", sent, tried);
	} else {
		snprintf(r->error, sizeof(r->error),
		         "failed to broadcast to any of %d peers", tried);
	}
}

/* ===== Public interface ===== */

int fallback_has_any(const FallbackConfig *cfg)
{
	return cfg->mempool_space || cfg->blockstream || cfg->blockchair ||
	       cfg->blockchain_info || cfg->blockcypher ||
	       cfg->esplora_url[0] || cfg->p2p_peers > 0;
}

int fallback_broadcast(const FallbackConfig *cfg, const char *hex,
                       Network net, FallbackResult *results, int *num_results)
{
	int n = 0;
	int successes = 0;

	/* API fallbacks (sequential — parallel via fork() can be added later) */

	if (cfg->mempool_space && n < MAX_FALLBACK_RESULTS) {
		fallback_esplora("mempool.space", 443, esplora_path(net),
		                 1, hex, "mempool.space", &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	if (cfg->blockstream && n < MAX_FALLBACK_RESULTS) {
		fallback_esplora("blockstream.info", 443, esplora_path(net),
		                 1, hex, "blockstream", &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	if (cfg->blockchair && n < MAX_FALLBACK_RESULTS) {
		fallback_blockchair_api(hex, net, &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	if (cfg->blockchain_info && n < MAX_FALLBACK_RESULTS) {
		fallback_blockchain_info(hex, &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	if (cfg->blockcypher && n < MAX_FALLBACK_RESULTS) {
		fallback_blockcypher_api(hex, net, &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	/* Custom Esplora (plain HTTP or HTTPS) */
	if (cfg->esplora_url[0] && n < MAX_FALLBACK_RESULTS) {
		char host[256];
		char path[512];
		int port, use_tls;

		parse_url(cfg->esplora_url, host, sizeof(host),
		          &port, path, sizeof(path), &use_tls);

		fallback_esplora(host, port, path, use_tls, hex,
		                 "esplora", &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	/* P2P direct broadcast (runs last — slowest but most resilient) */
	if (cfg->p2p_peers > 0 && n < MAX_FALLBACK_RESULTS) {
		fallback_p2p(hex, net, cfg->p2p_peers, &results[n]);
		if (results[n].success) successes++;
		n++;
	}

	*num_results = n;
	return successes;
}
