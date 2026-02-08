/* Bitcoin P2P protocol for peer mempool verification */

#include "p2p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ===== SHA-256 implementation ===== */

static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
	uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = ((uint32_t)block[i*4] << 24) |
		       ((uint32_t)block[i*4+1] << 16) |
		       ((uint32_t)block[i*4+2] << 8) |
		       ((uint32_t)block[i*4+3]);

	for (i = 16; i < 64; i++)
		w[i] = SIG1(w[i-2]) + w[i-7] + SIG0(w[i-15]) + w[i-16];

	a = state[0]; b = state[1]; c = state[2]; d = state[3];
	e = state[4]; f = state[5]; g = state[6]; h = state[7];

	for (i = 0; i < 64; i++) {
		t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + w[i];
		t2 = EP0(a) + MAJ(a, b, c);
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
	uint32_t state[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	uint8_t block[64];
	size_t i, blocks, remaining;
	uint64_t bit_len = len * 8;

	/* Process full blocks */
	blocks = len / 64;
	for (i = 0; i < blocks; i++)
		sha256_transform(state, data + i * 64);

	/* Pad last block */
	remaining = len % 64;
	memset(block, 0, 64);
	memcpy(block, data + blocks * 64, remaining);
	block[remaining] = 0x80;

	if (remaining >= 56) {
		sha256_transform(state, block);
		memset(block, 0, 64);
	}

	/* Append length in bits (big-endian) */
	for (i = 0; i < 8; i++)
		block[56 + i] = (uint8_t)(bit_len >> (56 - i * 8));

	sha256_transform(state, block);

	/* Output hash (big-endian) */
	for (i = 0; i < 8; i++) {
		hash[i*4]   = (uint8_t)(state[i] >> 24);
		hash[i*4+1] = (uint8_t)(state[i] >> 16);
		hash[i*4+2] = (uint8_t)(state[i] >> 8);
		hash[i*4+3] = (uint8_t)(state[i]);
	}
}

void sha256d(const uint8_t *data, size_t len, uint8_t *hash)
{
	uint8_t tmp[32];
	sha256(data, len, tmp);
	sha256(tmp, 32, hash);
}

/* ===== Network parameters ===== */

uint32_t p2p_magic(Network net)
{
	switch (net) {
	case NET_MAINNET:  return 0xD9B4BEF9;
	case NET_TESTNET:  return 0x0709110B;
	case NET_TESTNET4: return 0x1C163F28;
	case NET_SIGNET:   return 0x0A03CF40;
	case NET_REGTEST:  return 0xDAB5BFFA;
	default:           return 0xD9B4BEF9;
	}
}

int p2p_port(Network net)
{
	switch (net) {
	case NET_MAINNET:  return 8333;
	case NET_TESTNET:  return 18333;
	case NET_TESTNET4: return 48333;
	case NET_SIGNET:   return 38333;
	case NET_REGTEST:  return 18444;
	default:           return 8333;
	}
}

/* ===== DNS seed lookup ===== */

static const char *dns_seeds_mainnet[] = {
	"seed.bitcoin.sipa.be",
	"dnsseed.bluematt.me",
	"dnsseed.bitcoin.dashjr-list-of-p2p-nodes.us",
	"seed.bitcoinstats.com",
	"seed.bitcoin.jonasschnelli.ch",
	"seed.btc.petertodd.net",
	"seed.bitcoin.sprovoost.nl",
	NULL
};

static const char *dns_seeds_signet[] = {
	"seed.signet.bitcoin.sprovoost.nl",
	"178.128.221.177",
	NULL
};

static const char *dns_seeds_testnet[] = {
	"testnet-seed.bitcoin.jonasschnelli.ch",
	"seed.tbtc.petertodd.net",
	"testnet-seed.bluematt.me",
	NULL
};

static const char *dns_seeds_testnet4[] = {
	"seed.testnet4.bitcoin.sprovoost.nl",
	"seed.testnet4.wiz.biz",
	NULL
};

int p2p_dns_seed_lookup(Network net, char ***ips_out, int max_results)
{
	const char **seeds;
	struct addrinfo hints, *res, *rp;
	char **ips;
	int count = 0;
	int i;

	switch (net) {
	case NET_MAINNET:  seeds = dns_seeds_mainnet; break;
	case NET_TESTNET:  seeds = dns_seeds_testnet; break;
	case NET_TESTNET4: seeds = dns_seeds_testnet4; break;
	case NET_SIGNET:   seeds = dns_seeds_signet; break;
	default:
		*ips_out = NULL;
		return 0;
	}

	ips = calloc(max_results, sizeof(char *));
	if (!ips)
		return 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	for (i = 0; seeds[i] && count < max_results; i++) {
		if (getaddrinfo(seeds[i], NULL, &hints, &res) != 0)
			continue;

		for (rp = res; rp && count < max_results; rp = rp->ai_next) {
			if (rp->ai_family == AF_INET) {
				struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
				char ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));

				/* Deduplicate */
				int dup = 0;
				int j;
				for (j = 0; j < count; j++) {
					if (strcmp(ips[j], ip) == 0) {
						dup = 1;
						break;
					}
				}
				if (!dup) {
					ips[count] = strdup(ip);
					if (ips[count])
						count++;
				}
			}
		}
		freeaddrinfo(res);
	}

	*ips_out = ips;
	return count;
}

/* ===== TCP connect with timeout ===== */

int p2p_connect(P2pPeer *peer, const char *ip, int port,
                uint32_t magic, int timeout_sec)
{
	struct sockaddr_in addr;
	int flags, ret;
	fd_set wset;
	struct timeval tv;
	int err;
	socklen_t errlen = sizeof(err);

	memset(peer, 0, sizeof(P2pPeer));
	strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
	peer->port = port;
	peer->magic = magic;
	peer->sock = -1;

	peer->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (peer->sock < 0)
		return -1;

	/* Set non-blocking for connect timeout */
	flags = fcntl(peer->sock, F_GETFL, 0);
	fcntl(peer->sock, F_SETFL, flags | O_NONBLOCK);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &addr.sin_addr);

	ret = connect(peer->sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0 && errno != EINPROGRESS) {
		close(peer->sock);
		peer->sock = -1;
		return -1;
	}

	if (ret < 0) {
		/* Wait for connect with timeout */
		FD_ZERO(&wset);
		FD_SET(peer->sock, &wset);
		tv.tv_sec = timeout_sec;
		tv.tv_usec = 0;

		ret = select(peer->sock + 1, NULL, &wset, NULL, &tv);
		if (ret <= 0) {
			close(peer->sock);
			peer->sock = -1;
			return -1;
		}

		/* Check for connect error */
		getsockopt(peer->sock, SOL_SOCKET, SO_ERROR, &err, &errlen);
		if (err) {
			close(peer->sock);
			peer->sock = -1;
			return -1;
		}
	}

	/* Restore blocking mode */
	fcntl(peer->sock, F_SETFL, flags);

	/* Set recv timeout */
	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0;
	setsockopt(peer->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(peer->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	return 0;
}

/* ===== P2P message I/O ===== */

/* Write a P2P message with header: magic(4) + command(12) + length(4) + checksum(4) */
static int p2p_send_msg(P2pPeer *peer, const char *command,
                         const uint8_t *payload, uint32_t payload_len)
{
	uint8_t header[P2P_HDR_SIZE];
	uint8_t checksum_hash[32];
	ssize_t n;

	/* Magic (little-endian) */
	header[0] = (uint8_t)(peer->magic);
	header[1] = (uint8_t)(peer->magic >> 8);
	header[2] = (uint8_t)(peer->magic >> 16);
	header[3] = (uint8_t)(peer->magic >> 24);

	/* Command (12 bytes, zero-padded) */
	memset(header + 4, 0, 12);
	strncpy((char *)header + 4, command, 12);

	/* Payload length (little-endian) */
	header[16] = (uint8_t)(payload_len);
	header[17] = (uint8_t)(payload_len >> 8);
	header[18] = (uint8_t)(payload_len >> 16);
	header[19] = (uint8_t)(payload_len >> 24);

	/* Checksum: first 4 bytes of SHA256d(payload) */
	if (payload_len > 0)
		sha256d(payload, payload_len, checksum_hash);
	else
		sha256d((const uint8_t *)"", 0, checksum_hash);
	memcpy(header + 20, checksum_hash, 4);

	/* Send header */
	n = send(peer->sock, header, P2P_HDR_SIZE, 0);
	if (n != P2P_HDR_SIZE)
		return -1;

	/* Send payload */
	if (payload_len > 0) {
		n = send(peer->sock, payload, payload_len, 0);
		if (n != (ssize_t)payload_len)
			return -1;
	}

	return 0;
}

/* Read exactly n bytes */
static int recv_exact(int sock, uint8_t *buf, size_t n)
{
	size_t total = 0;
	while (total < n) {
		ssize_t r = recv(sock, buf + total, n - total, 0);
		if (r <= 0)
			return -1;
		total += r;
	}
	return 0;
}

/* Read a P2P message header + payload. Caller frees *payload_out. */
static int p2p_recv_msg(P2pPeer *peer, char *cmd_out,
                         uint8_t **payload_out, uint32_t *len_out)
{
	uint8_t header[P2P_HDR_SIZE];
	uint32_t payload_len;

	if (recv_exact(peer->sock, header, P2P_HDR_SIZE) < 0)
		return -1;

	/* Extract command */
	memcpy(cmd_out, header + 4, 12);
	cmd_out[12] = '\0';

	/* Extract payload length (little-endian) */
	payload_len = (uint32_t)header[16] |
	              ((uint32_t)header[17] << 8) |
	              ((uint32_t)header[18] << 16) |
	              ((uint32_t)header[19] << 24);

	/* Sanity check */
	if (payload_len > 4 * 1024 * 1024) {
		*payload_out = NULL;
		*len_out = 0;
		return -1;
	}

	*len_out = payload_len;

	if (payload_len == 0) {
		*payload_out = NULL;
		return 0;
	}

	*payload_out = malloc(payload_len);
	if (!*payload_out)
		return -1;

	if (recv_exact(peer->sock, *payload_out, payload_len) < 0) {
		free(*payload_out);
		*payload_out = NULL;
		return -1;
	}

	return 0;
}

/* ===== Version/Verack handshake ===== */

/* Encode a 64-bit little-endian value */
static void le64(uint8_t *buf, uint64_t v)
{
	int i;
	for (i = 0; i < 8; i++)
		buf[i] = (uint8_t)(v >> (i * 8));
}

/* Encode a 32-bit little-endian value */
static void le32(uint8_t *buf, uint32_t v)
{
	buf[0] = (uint8_t)(v);
	buf[1] = (uint8_t)(v >> 8);
	buf[2] = (uint8_t)(v >> 16);
	buf[3] = (uint8_t)(v >> 24);
}

int p2p_handshake(P2pPeer *peer)
{
	/* Build version message payload */
	uint8_t payload[86];  /* Minimal version message */
	char cmd[13];
	uint8_t *recv_payload;
	uint32_t recv_len;
	int got_verack = 0;
	int attempts = 0;

	memset(payload, 0, sizeof(payload));

	/* Protocol version (4 bytes LE) */
	le32(payload, P2P_PROTOCOL_VERSION);

	/* Services (8 bytes LE) — 0 = no services */
	le64(payload + 4, 0);

	/* Timestamp (8 bytes LE) */
	le64(payload + 12, (uint64_t)time(NULL));

	/* Addr recv: services(8) + IPv6-mapped-IPv4(16) + port(2) */
	/* All zeros is fine */

	/* Addr from: same, offset 46 */
	/* All zeros is fine */

	/* Nonce (8 bytes) at offset 72 */
	le64(payload + 72, (uint64_t)time(NULL) ^ 0x1234567890ABCDEFULL);

	/* User agent: varint(0) at offset 80 = no user agent */
	payload[80] = 0;

	/* Start height (4 bytes LE) at offset 81 */
	le32(payload + 81, 0);

	/* Relay (1 byte) at offset 85 — 0 = don't relay tx unsolicited */
	payload[85] = 0;

	/* Send version */
	if (p2p_send_msg(peer, "version", payload, sizeof(payload)) < 0)
		return -1;

	/* Read messages until we get verack */
	while (!got_verack && attempts < 20) {
		attempts++;
		if (p2p_recv_msg(peer, cmd, &recv_payload, &recv_len) < 0)
			return -1;

		if (strcmp(cmd, "version") == 0) {
			/* Send verack in response */
			free(recv_payload);
			if (p2p_send_msg(peer, "verack", NULL, 0) < 0)
				return -1;
		} else if (strcmp(cmd, "verack") == 0) {
			free(recv_payload);
			got_verack = 1;
		} else {
			/* Ignore other messages during handshake
			 * (sendheaders, sendcmpct, wtxidrelay, etc.) */
			free(recv_payload);
		}
	}

	return got_verack ? 0 : -1;
}

/* ===== BIP 35 mempool ===== */

int p2p_send_mempool(P2pPeer *peer)
{
	return p2p_send_msg(peer, "mempool", NULL, 0);
}

/* ===== P2P tx broadcast ===== */

int p2p_send_tx(P2pPeer *peer, const uint8_t *tx_data, size_t tx_len)
{
	return p2p_send_msg(peer, "tx", tx_data, (uint32_t)tx_len);
}

/* ===== Inv scanning ===== */

/* Read a CompactSize uint from buffer */
static uint64_t read_compact_size(const uint8_t *buf, size_t len, size_t *offset)
{
	if (*offset >= len)
		return 0;

	uint8_t first = buf[*offset];
	(*offset)++;

	if (first < 0xFD)
		return first;

	if (first == 0xFD && *offset + 2 <= len) {
		uint16_t v = (uint16_t)buf[*offset] |
		             ((uint16_t)buf[*offset + 1] << 8);
		*offset += 2;
		return v;
	}

	if (first == 0xFE && *offset + 4 <= len) {
		uint32_t v = (uint32_t)buf[*offset] |
		             ((uint32_t)buf[*offset + 1] << 8) |
		             ((uint32_t)buf[*offset + 2] << 16) |
		             ((uint32_t)buf[*offset + 3] << 24);
		*offset += 4;
		return v;
	}

	return 0;
}

int p2p_scan_inv_for_tx(P2pPeer *peer, const uint8_t *txid, int timeout_sec)
{
	char cmd[13];
	uint8_t *payload;
	uint32_t payload_len;
	time_t start = time(NULL);

	while (time(NULL) - start < timeout_sec) {
		if (p2p_recv_msg(peer, cmd, &payload, &payload_len) < 0)
			return 0;

		if (strcmp(cmd, "inv") == 0 && payload && payload_len > 0) {
			size_t offset = 0;
			uint64_t count = read_compact_size(payload, payload_len, &offset);
			uint64_t i;

			for (i = 0; i < count && offset + 36 <= payload_len; i++) {
				uint32_t inv_type = (uint32_t)payload[offset] |
				                    ((uint32_t)payload[offset + 1] << 8) |
				                    ((uint32_t)payload[offset + 2] << 16) |
				                    ((uint32_t)payload[offset + 3] << 24);
				offset += 4;

				/* Check for MSG_TX(1) or MSG_WTX(5) */
				if (inv_type == MSG_TX || inv_type == MSG_WTX) {
					if (memcmp(payload + offset, txid, 32) == 0) {
						free(payload);
						return 1;
					}
				}
				offset += 32;
			}
		}

		free(payload);

		/* Send pong for any ping */
		if (strcmp(cmd, "ping") == 0) {
			/* pong with same nonce — we already freed payload,
			 * but for simplicity just send empty pong */
			p2p_send_msg(peer, "pong", NULL, 0);
		}
	}

	return 0;
}

/* ===== Disconnect ===== */

void p2p_disconnect(P2pPeer *peer)
{
	if (peer->sock >= 0) {
		close(peer->sock);
		peer->sock = -1;
	}
}
