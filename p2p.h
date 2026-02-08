/* Bitcoin P2P protocol for peer mempool verification */

#ifndef P2P_H
#define P2P_H

#include "config.h"
#include <stdint.h>
#include <stddef.h>

/* P2P message header size */
#define P2P_HDR_SIZE 24

/* P2P protocol version (BIP 339, no wtxidrelay) */
#define P2P_PROTOCOL_VERSION 70016

/* Inventory types */
#define MSG_TX  1
#define MSG_WTX 5

typedef struct {
	int sock;
	uint32_t magic;
	char ip[64];
	int port;
} P2pPeer;

/* Get magic bytes for network */
uint32_t p2p_magic(Network net);

/* Get default P2P port for network */
int p2p_port(Network net);

/* DNS seed lookup — resolve seed hostnames to IP addresses.
 * Returns number of IPs found, fills ips_out with allocated strings.
 * Caller must free each string and the array.
 */
int p2p_dns_seed_lookup(Network net, char ***ips_out, int max_results);

/* Connect to peer with timeout */
int p2p_connect(P2pPeer *peer, const char *ip, int port,
                uint32_t magic, int timeout_sec);

/* Perform version/verack handshake */
int p2p_handshake(P2pPeer *peer);

/* Send BIP 35 mempool request */
int p2p_send_mempool(P2pPeer *peer);

/* Send a raw transaction to peer via P2P "tx" message.
 * tx_data: raw serialized transaction bytes (NOT hex)
 * tx_len: length of tx_data
 * Returns 0 on success, -1 on failure.
 */
int p2p_send_tx(P2pPeer *peer, const uint8_t *tx_data, size_t tx_len);

/* Scan incoming inv messages for a specific txid.
 * txid is 32 bytes in internal byte order.
 * Returns 1 if found, 0 if timeout.
 */
int p2p_scan_inv_for_tx(P2pPeer *peer, const uint8_t *txid, int timeout_sec);

/* Disconnect and cleanup */
void p2p_disconnect(P2pPeer *peer);

/* SHA-256 hash functions */
void sha256(const uint8_t *data, size_t len, uint8_t *hash);
void sha256d(const uint8_t *data, size_t len, uint8_t *hash);

#endif
