/* P2P transaction propagation verification */

#ifndef VERIFY_H
#define VERIFY_H

#include "config.h"

/* Verify transaction propagation via P2P peers.
 * Connects to random peers and checks if txid is in their mempools.
 * txid_hex: 64-char hex txid string
 * net: network to use for DNS seeds and magic bytes
 * num_peers: how many peers to check (1-10)
 * Returns: number of peers that confirmed the tx
 */
int verify_tx_propagation(const char *txid_hex, Network net, int num_peers);

#endif
