/* P2P transaction propagation verification */

#include "verify.h"
#include "p2p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Convert hex char to nibble */
static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* Convert 64-char hex txid to 32-byte internal byte order (reversed) */
static int txid_hex_to_bytes(const char *hex, uint8_t *out)
{
	int i;

	if (strlen(hex) != 64)
		return -1;

	/* Bitcoin internal byte order is reversed from display */
	for (i = 0; i < 32; i++) {
		int hi = hex_nibble(hex[i * 2]);
		int lo = hex_nibble(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0)
			return -1;
		out[31 - i] = (uint8_t)((hi << 4) | lo);
	}

	return 0;
}

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

int verify_tx_propagation(const char *txid_hex, Network net, int num_peers)
{
	char **ips = NULL;
	int ip_count;
	uint8_t txid_bytes[32];
	uint32_t magic;
	int port;
	int confirmed = 0;
	int checked = 0;
	int i;

	/* Convert txid to bytes */
	if (txid_hex_to_bytes(txid_hex, txid_bytes) < 0) {
		fprintf(stderr, "Error: invalid txid hex\n");
		return 0;
	}

	magic = p2p_magic(net);
	port = p2p_port(net);

	/* DNS seed lookup */
	fprintf(stderr, "Looking up peers via DNS seeds...\n");
	ip_count = p2p_dns_seed_lookup(net, &ips, 64);
	if (ip_count == 0) {
		fprintf(stderr, "Error: no peers found via DNS seeds\n");
		return 0;
	}

	fprintf(stderr, "Found %d peer IPs\n", ip_count);

	/* Shuffle to randomize which peers we talk to */
	shuffle_ips(ips, ip_count);

	/* Check peers */
	for (i = 0; i < ip_count && checked < num_peers; i++) {
		P2pPeer peer;

		fprintf(stderr, "Connecting to %s:%d... ", ips[i], port);

		if (p2p_connect(&peer, ips[i], port, magic, 5) < 0) {
			fprintf(stderr, "failed (connect)\n");
			continue;
		}

		if (p2p_handshake(&peer) < 0) {
			fprintf(stderr, "failed (handshake)\n");
			p2p_disconnect(&peer);
			continue;
		}

		if (p2p_send_mempool(&peer) < 0) {
			fprintf(stderr, "failed (mempool request)\n");
			p2p_disconnect(&peer);
			continue;
		}

		checked++;

		if (p2p_scan_inv_for_tx(&peer, txid_bytes, 10)) {
			fprintf(stderr, "CONFIRMED\n");
			confirmed++;
		} else {
			fprintf(stderr, "not found\n");
		}

		p2p_disconnect(&peer);
	}

	fprintf(stderr, "\nVerified: %d/%d peers confirmed tx in mempool\n",
	        confirmed, checked);

	/* Cleanup */
	for (i = 0; i < ip_count; i++)
		free(ips[i]);
	free(ips);

	return confirmed;
}
