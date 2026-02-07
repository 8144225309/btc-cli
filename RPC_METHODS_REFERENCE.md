# Bitcoin Core RPC Methods Reference

Complete categorized list of all Bitcoin Core RPC methods for implementation.

---

## Blockchain RPCs (26 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `getbestblockhash` | none | Get hash of best block |
| `getblock` | blockhash, verbosity? | Get block data |
| `getblockchaininfo` | none | Get blockchain state |
| `getblockcount` | none | Get current height |
| `getblockfilter` | blockhash, filtertype? | Get BIP 157 filter |
| `getblockhash` | height | Get hash at height |
| `getblockheader` | blockhash, verbose? | Get block header |
| `getblockstats` | hash_or_height, stats? | Get block statistics |
| `getchaintips` | none | Get chain tip info |
| `getchaintxstats` | nblocks?, blockhash? | Get chain TX stats |
| `getdifficulty` | none | Get current difficulty |
| `getmempoolancestors` | txid, verbose? | Get mempool ancestors |
| `getmempooldescendants` | txid, verbose? | Get mempool descendants |
| `getmempoolentry` | txid | Get mempool entry |
| `getmempoolinfo` | none | Get mempool state |
| `getrawmempool` | verbose?, mempool_sequence? | Get raw mempool |
| `gettxout` | txid, n, include_mempool? | Get UTXO |
| `gettxoutproof` | txids, blockhash? | Get merkle proof |
| `gettxoutsetinfo` | hash_type?, hash_or_height?, use_index? | Get UTXO set info |
| `preciousblock` | blockhash | Mark block precious |
| `pruneblockchain` | height | Prune to height |
| `savemempool` | none | Dump mempool to disk |
| `scantxoutset` | action, scanobjects | Scan UTXO set |
| `verifychain` | checklevel?, nblocks? | Verify chain |
| `verifytxoutproof` | proof | Verify merkle proof |

---

## Control RPCs (6 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `getmemoryinfo` | mode? | Get memory usage |
| `getrpcinfo` | none | Get RPC state |
| `help` | command? | Get help |
| `logging` | include?, exclude? | Get/set logging |
| `stop` | none | Stop server |
| `uptime` | none | Get uptime |

---

## Generating RPCs (3 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `generateblock` | output, transactions, submit? | Generate block |
| `generatetoaddress` | nblocks, address, maxtries? | Generate to address |
| `generatetodescriptor` | nblocks, descriptor, maxtries? | Generate to descriptor |

---

## Mining RPCs (6 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `getblocktemplate` | template_request? | Get block template |
| `getmininginfo` | none | Get mining info |
| `getnetworkhashps` | nblocks?, height? | Get network hashrate |
| `prioritisetransaction` | txid, dummy?, fee_delta | Prioritize TX |
| `submitblock` | hexdata, dummy? | Submit block |
| `submitheader` | hexdata | Submit header |

---

## Network RPCs (13 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `addnode` | node, command | Add/remove node |
| `clearbanned` | none | Clear ban list |
| `disconnectnode` | address?, nodeid? | Disconnect peer |
| `getaddednodeinfo` | node? | Get added node info |
| `getconnectioncount` | none | Get connection count |
| `getnettotals` | none | Get network totals |
| `getnetworkinfo` | none | Get network info |
| `getnodeaddresses` | count?, network? | Get node addresses |
| `getpeerinfo` | none | Get peer info |
| `listbanned` | none | List banned |
| `ping` | none | Ping peers |
| `setban` | subnet, command, bantime?, absolute? | Set ban |
| `setnetworkactive` | state | Enable/disable network |

---

## Raw Transaction RPCs (18 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `analyzepsbt` | psbt | Analyze PSBT |
| `combinepsbt` | psbts | Combine PSBTs |
| `combinerawtransaction` | txs | Combine raw TXs |
| `converttopsbt` | hexstring, permitsigdata?, iswitness? | Convert to PSBT |
| `createpsbt` | inputs, outputs, locktime?, replaceable? | Create PSBT |
| `createrawtransaction` | inputs, outputs, locktime?, replaceable? | Create raw TX |
| `decodepsbt` | psbt | Decode PSBT |
| `decoderawtransaction` | hexstring, iswitness? | Decode raw TX |
| `decodescript` | hexstring | Decode script |
| `finalizepsbt` | psbt, extract? | Finalize PSBT |
| `fundrawtransaction` | hexstring, options?, iswitness? | Fund raw TX |
| `getrawtransaction` | txid, verbose?, blockhash? | Get raw TX |
| `joinpsbts` | psbts | Join PSBTs |
| `sendrawtransaction` | hexstring, maxfeerate? | Broadcast TX |
| `signrawtransactionwithkey` | hexstring, privkeys, prevtxs?, sighashtype? | Sign with keys |
| `testmempoolaccept` | rawtxs, maxfeerate? | Test mempool accept |
| `utxoupdatepsbt` | psbt, descriptors? | Update PSBT UTXOs |

---

## Utility RPCs (8 methods)

| Method | Params | Description |
|--------|--------|-------------|
| `createmultisig` | nrequired, keys, address_type? | Create multisig |
| `deriveaddresses` | descriptor, range? | Derive addresses |
| `estimatesmartfee` | conf_target, estimate_mode? | Estimate fee |
| `getdescriptorinfo` | descriptor | Get descriptor info |
| `getindexinfo` | index_name? | Get index info |
| `signmessagewithprivkey` | privkey, message | Sign message |
| `validateaddress` | address | Validate address |
| `verifymessage` | address, signature, message | Verify message |

---

## Wallet RPCs (57+ methods)

### Balance & Info
| Method | Params | Description |
|--------|--------|-------------|
| `getbalance` | dummy?, minconf?, include_watchonly?, avoid_reuse? | Get balance |
| `getbalances` | none | Get detailed balances |
| `getwalletinfo` | none | Get wallet info |
| `listwallets` | none | List loaded wallets |

### Addresses
| Method | Params | Description |
|--------|--------|-------------|
| `getnewaddress` | label?, address_type? | Get new address |
| `getrawchangeaddress` | address_type? | Get change address |
| `getaddressinfo` | address | Get address info |
| `getaddressesbylabel` | label | Get addresses by label |
| `listlabels` | purpose? | List labels |
| `setlabel` | address, label | Set label |

### Transactions
| Method | Params | Description |
|--------|--------|-------------|
| `listtransactions` | label?, count?, skip?, include_watchonly? | List TXs |
| `listunspent` | minconf?, maxconf?, addresses?, include_unsafe?, query_options? | List UTXOs |
| `gettransaction` | txid, include_watchonly?, verbose? | Get TX |
| `listsinceblock` | blockhash?, confirmations?, include_watchonly?, include_removed? | List since block |
| `listreceivedbyaddress` | minconf?, include_empty?, include_watchonly?, address_filter? | List received |
| `listreceivedbylabel` | minconf?, include_empty?, include_watchonly? | List received by label |

### Sending
| Method | Params | Description |
|--------|--------|-------------|
| `sendtoaddress` | address, amount, comment?, comment_to?, subtractfee?, replaceable?, conf_target?, estimate_mode?, avoid_reuse? | Send to address |
| `sendmany` | dummy, amounts, minconf?, comment?, subtractfeefrom?, replaceable?, conf_target?, estimate_mode? | Send to many |
| `send` | outputs, conf_target?, estimate_mode?, fee_rate?, options? | Send (modern) |
| `settxfee` | amount | Set TX fee |
| `bumpfee` | txid, options? | Bump fee (RBF) |
| `psbtbumpfee` | txid, options? | Bump fee via PSBT |

### Keys & Import
| Method | Params | Description |
|--------|--------|-------------|
| `dumpprivkey` | address | Dump private key |
| `dumpwallet` | filename | Dump wallet |
| `importaddress` | address, label?, rescan?, p2sh? | Import address |
| `importdescriptors` | requests | Import descriptors |
| `importmulti` | requests, options? | Import multiple |
| `importprivkey` | privkey, label?, rescan? | Import private key |
| `importprunedfunds` | rawtransaction, txoutproof | Import pruned funds |
| `importpubkey` | pubkey, label?, rescan? | Import public key |
| `importwallet` | filename | Import wallet |
| `listdescriptors` | private? | List descriptors |

### Wallet Management
| Method | Params | Description |
|--------|--------|-------------|
| `createwallet` | wallet_name, disable_private_keys?, blank?, passphrase?, avoid_reuse?, descriptors?, load_on_startup?, external_signer? | Create wallet |
| `loadwallet` | filename, load_on_startup? | Load wallet |
| `unloadwallet` | wallet_name?, load_on_startup? | Unload wallet |
| `backupwallet` | destination | Backup wallet |
| `restorewallet` | wallet_name, backup_file, load_on_startup? | Restore wallet |
| `upgradewallet` | version? | Upgrade wallet |

### Encryption
| Method | Params | Description |
|--------|--------|-------------|
| `encryptwallet` | passphrase | Encrypt wallet |
| `walletpassphrase` | passphrase, timeout | Unlock wallet |
| `walletpassphrasechange` | oldpassphrase, newpassphrase | Change passphrase |
| `walletlock` | none | Lock wallet |

### Signing
| Method | Params | Description |
|--------|--------|-------------|
| `signmessage` | address, message | Sign message |
| `signrawtransactionwithwallet` | hexstring, prevtxs?, sighashtype? | Sign TX |
| `walletprocesspsbt` | psbt, sign?, sighashtype?, bip32derivs?, finalize? | Process PSBT |
| `walletcreatefundedpsbt` | inputs, outputs, locktime?, options?, bip32derivs? | Create funded PSBT |

### Other
| Method | Params | Description |
|--------|--------|-------------|
| `abandontransaction` | txid | Abandon TX |
| `abortrescan` | none | Abort rescan |
| `getreceivedbyaddress` | address, minconf? | Get received |
| `getreceivedbylabel` | label, minconf? | Get received by label |
| `getunconfirmedbalance` | none | Get unconfirmed balance |
| `keypoolrefill` | newsize? | Refill keypool |
| `listaddressgroupings` | none | List address groupings |
| `listlockunspent` | none | List locked UTXOs |
| `lockunspent` | unlock, transactions? | Lock/unlock UTXOs |
| `removeprunedfunds` | txid | Remove pruned funds |
| `rescanblockchain` | start_height?, stop_height? | Rescan blockchain |
| `sethdseed` | newkeypool?, seed? | Set HD seed |
| `setwalletflag` | flag, value? | Set wallet flag |
| `walletdisplayaddress` | address | Display on HW wallet |

---

## Implementation Priority

### Must Have (Phase 1)
```
getblockchaininfo, getblockcount, getbestblockhash, getblockhash, getblock
getbalance, getnewaddress, listunspent, listtransactions
createrawtransaction, decoderawtransaction, signrawtransactionwithwallet
sendrawtransaction, testmempoolaccept, getrawtransaction
help, stop
```

### Should Have (Phase 2)
```
sendtoaddress, sendmany, getwalletinfo, listwallets
dumpprivkey, importprivkey, importdescriptors
walletpassphrase, walletlock, estimatesmartfee
```

### Nice to Have (Phase 3+)
```
PSBT methods, network methods, mining methods
Advanced wallet methods, signer RPCs
```
