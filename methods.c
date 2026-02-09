/* RPC Method registry and dispatch */

#include "methods.h"
#include "json.h"
#include "sendtx.h"
#include "verify.h"
#include "fallback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Global flag for named parameter mode */
static int g_named_mode = 0;

/* P2P verification settings */
static int g_verify_enabled = 0;
static int g_verify_peers = 3;
static Network g_network = NET_MAINNET;

/* Fallback broadcast settings */
static FallbackConfig g_fallback_cfg;

void method_set_named_mode(int enabled)
{
	g_named_mode = enabled;
}

void method_set_verify(int enabled, int peers, Network net)
{
	g_verify_enabled = enabled;
	g_verify_peers = peers;
	g_network = net;
}

void method_set_fallback(const FallbackConfig *cfg)
{
	g_fallback_cfg = *cfg;
}

/* Forward declarations of handlers */
static int cmd_generic(RpcClient *rpc, const char *method, int argc, char **argv, char **out);

/* Generic handler macro - most methods just pass through */
#define GENERIC_HANDLER(name) \
	static int cmd_##name(RpcClient *rpc, int argc, char **argv, char **out) { \
		return cmd_generic(rpc, #name, argc, argv, out); \
	}

/* Generate handlers for all Phase 1 methods */
/* Blockchain */
GENERIC_HANDLER(getblockchaininfo)
GENERIC_HANDLER(getblockcount)
GENERIC_HANDLER(getbestblockhash)
GENERIC_HANDLER(getblockhash)
GENERIC_HANDLER(getblock)
GENERIC_HANDLER(getblockheader)
GENERIC_HANDLER(getdifficulty)
GENERIC_HANDLER(getchaintips)
GENERIC_HANDLER(getmempoolinfo)
GENERIC_HANDLER(getrawmempool)

/* Wallet */
GENERIC_HANDLER(getbalance)
GENERIC_HANDLER(getbalances)
GENERIC_HANDLER(getwalletinfo)
GENERIC_HANDLER(getnewaddress)
GENERIC_HANDLER(getaddressinfo)
GENERIC_HANDLER(listunspent)
GENERIC_HANDLER(listtransactions)
GENERIC_HANDLER(listwallets)

/* Raw transactions */
GENERIC_HANDLER(createrawtransaction)
GENERIC_HANDLER(decoderawtransaction)
GENERIC_HANDLER(decodescript)
GENERIC_HANDLER(signrawtransactionwithwallet)
GENERIC_HANDLER(signrawtransactionwithkey)
GENERIC_HANDLER(testmempoolaccept)
GENERIC_HANDLER(getrawtransaction)

/* Network */
GENERIC_HANDLER(getnetworkinfo)
GENERIC_HANDLER(getpeerinfo)
GENERIC_HANDLER(getconnectioncount)

/* Control */
GENERIC_HANDLER(help)
GENERIC_HANDLER(stop)
GENERIC_HANDLER(uptime)

/* Utility */
GENERIC_HANDLER(validateaddress)
GENERIC_HANDLER(estimatesmartfee)

/* Phase 2: Wallet sending */
GENERIC_HANDLER(sendtoaddress)
GENERIC_HANDLER(sendmany)
GENERIC_HANDLER(send)
GENERIC_HANDLER(bumpfee)
GENERIC_HANDLER(psbtbumpfee)
GENERIC_HANDLER(settxfee)

/* Phase 2: Wallet management */
GENERIC_HANDLER(createwallet)
GENERIC_HANDLER(loadwallet)
GENERIC_HANDLER(unloadwallet)
GENERIC_HANDLER(backupwallet)
GENERIC_HANDLER(restorewallet)

/* Phase 2: Key operations */
GENERIC_HANDLER(dumpprivkey)
GENERIC_HANDLER(importprivkey)
GENERIC_HANDLER(importaddress)
GENERIC_HANDLER(importpubkey)
GENERIC_HANDLER(importdescriptors)
GENERIC_HANDLER(listdescriptors)
GENERIC_HANDLER(importmulti)
GENERIC_HANDLER(dumpwallet)
GENERIC_HANDLER(importwallet)

/* Phase 2: Encryption */
GENERIC_HANDLER(encryptwallet)
GENERIC_HANDLER(walletpassphrase)
GENERIC_HANDLER(walletlock)
GENERIC_HANDLER(walletpassphrasechange)

/* Phase 2: Additional wallet queries */
GENERIC_HANDLER(gettransaction)
GENERIC_HANDLER(listsinceblock)
GENERIC_HANDLER(getreceivedbyaddress)
GENERIC_HANDLER(getreceivedbylabel)
GENERIC_HANDLER(listreceivedbyaddress)
GENERIC_HANDLER(listreceivedbylabel)
GENERIC_HANDLER(getrawchangeaddress)
GENERIC_HANDLER(getaddressesbylabel)
GENERIC_HANDLER(listlabels)
GENERIC_HANDLER(setlabel)
GENERIC_HANDLER(signmessage)
GENERIC_HANDLER(abandontransaction)
GENERIC_HANDLER(abortrescan)
GENERIC_HANDLER(rescanblockchain)
GENERIC_HANDLER(listlockunspent)
GENERIC_HANDLER(lockunspent)
GENERIC_HANDLER(keypoolrefill)
GENERIC_HANDLER(getunconfirmedbalance)
GENERIC_HANDLER(listaddressgroupings)

/* Phase 3: PSBT */
GENERIC_HANDLER(createpsbt)
GENERIC_HANDLER(decodepsbt)
GENERIC_HANDLER(analyzepsbt)
GENERIC_HANDLER(combinepsbt)
GENERIC_HANDLER(finalizepsbt)
GENERIC_HANDLER(joinpsbts)
GENERIC_HANDLER(converttopsbt)
GENERIC_HANDLER(utxoupdatepsbt)
GENERIC_HANDLER(walletcreatefundedpsbt)
GENERIC_HANDLER(walletprocesspsbt)
GENERIC_HANDLER(combinerawtransaction)
GENERIC_HANDLER(fundrawtransaction)

/* Phase 4: Network */
GENERIC_HANDLER(addnode)
GENERIC_HANDLER(disconnectnode)
GENERIC_HANDLER(setban)
GENERIC_HANDLER(listbanned)
GENERIC_HANDLER(clearbanned)
GENERIC_HANDLER(getnettotals)
GENERIC_HANDLER(getnodeaddresses)
GENERIC_HANDLER(getaddednodeinfo)
GENERIC_HANDLER(ping)
GENERIC_HANDLER(setnetworkactive)

/* Phase 4: Mining/Generating */
GENERIC_HANDLER(getmininginfo)
GENERIC_HANDLER(getnetworkhashps)
GENERIC_HANDLER(generatetoaddress)
GENERIC_HANDLER(generateblock)
GENERIC_HANDLER(generatetodescriptor)
GENERIC_HANDLER(getblocktemplate)
GENERIC_HANDLER(submitblock)
GENERIC_HANDLER(submitheader)
GENERIC_HANDLER(prioritisetransaction)

/* Phase 5: Advanced blockchain */
GENERIC_HANDLER(getblockfilter)
GENERIC_HANDLER(getblockstats)
GENERIC_HANDLER(getchaintxstats)
GENERIC_HANDLER(getmempoolancestors)
GENERIC_HANDLER(getmempooldescendants)
GENERIC_HANDLER(getmempoolentry)
GENERIC_HANDLER(gettxout)
GENERIC_HANDLER(gettxoutproof)
GENERIC_HANDLER(gettxoutsetinfo)
GENERIC_HANDLER(verifytxoutproof)
GENERIC_HANDLER(scantxoutset)
GENERIC_HANDLER(preciousblock)
GENERIC_HANDLER(pruneblockchain)
GENERIC_HANDLER(savemempool)
GENERIC_HANDLER(verifychain)

/* Phase 5: Utility */
GENERIC_HANDLER(createmultisig)
GENERIC_HANDLER(deriveaddresses)
GENERIC_HANDLER(getdescriptorinfo)
GENERIC_HANDLER(getindexinfo)
GENERIC_HANDLER(signmessagewithprivkey)
GENERIC_HANDLER(verifymessage)

/* Phase 5: Control */
GENERIC_HANDLER(getmemoryinfo)
GENERIC_HANDLER(getrpcinfo)
GENERIC_HANDLER(logging)

/* Phase 6: Bitcoin Core 30.x new methods */
GENERIC_HANDLER(createwalletdescriptor)
GENERIC_HANDLER(descriptorprocesspsbt)
GENERIC_HANDLER(dumptxoutset)
GENERIC_HANDLER(enumeratesigners)
GENERIC_HANDLER(getaddrmaninfo)
GENERIC_HANDLER(getblockfrompeer)
GENERIC_HANDLER(getchainstates)
GENERIC_HANDLER(getdeploymentinfo)
GENERIC_HANDLER(getdescriptoractivity)
GENERIC_HANDLER(gethdkeys)
GENERIC_HANDLER(getprioritisedtransactions)
GENERIC_HANDLER(gettxspendingprevout)
GENERIC_HANDLER(getzmqnotifications)
GENERIC_HANDLER(importmempool)
GENERIC_HANDLER(importprunedfunds)
GENERIC_HANDLER(listwalletdir)
GENERIC_HANDLER(loadtxoutset)
GENERIC_HANDLER(migratewallet)
GENERIC_HANDLER(removeprunedfunds)
GENERIC_HANDLER(scanblocks)
GENERIC_HANDLER(sendall)
GENERIC_HANDLER(setwalletflag)
GENERIC_HANDLER(simulaterawtransaction)
GENERIC_HANDLER(submitpackage)
GENERIC_HANDLER(waitforblock)
GENERIC_HANDLER(waitforblockheight)
GENERIC_HANDLER(waitfornewblock)
GENERIC_HANDLER(walletdisplayaddress)

/* Missing blockchain methods */
GENERIC_HANDLER(invalidateblock)
GENERIC_HANDLER(reconsiderblock)

/* Missing wallet methods */
GENERIC_HANDLER(addmultisigaddress)
GENERIC_HANDLER(newkeypool)
GENERIC_HANDLER(upgradewallet)
GENERIC_HANDLER(sethdseed)

/* Custom sendrawtransaction handler with retry + verify + fallback */
static int cmd_sendrawtransaction(RpcClient *rpc, int argc, char **argv, char **out)
{
	SendTxResult result;
	const char *hexstring = (argc >= 1) ? argv[0] : NULL;
	const char *maxfeerate = (argc >= 2) ? argv[1] : NULL;
	int rpc_ok;
	int fallback_ok = 0;

	if (!hexstring) {
		*out = strdup("error: sendrawtransaction requires hexstring");
		return 1;
	}

	/* Layer 1: Local RPC with retry */
	rpc_ok = (sendtx_submit(rpc, hexstring, maxfeerate, &result) == 0);

	if (rpc_ok) {
		*out = strdup(result.txid);
		if (result.in_local_mempool)
			fprintf(stderr, "Confirmed in local mempool\n");
	}

	/* Layer 2: Fallback broadcast (always fires if configured) */
	if (fallback_has_any(&g_fallback_cfg)) {
		FallbackResult fb_results[MAX_FALLBACK_RESULTS];
		int fb_count = 0;
		int fb_ok;
		int i;

		fprintf(stderr, "\nFallback broadcast:\n");
		fb_ok = fallback_broadcast(&g_fallback_cfg, hexstring,
		                            g_network, fb_results, &fb_count);

		for (i = 0; i < fb_count; i++) {
			if (fb_results[i].success) {
				fprintf(stderr, "  [%s] OK", fb_results[i].source);
				if (fb_results[i].txid[0])
					fprintf(stderr, " (%s)", fb_results[i].txid);
				fprintf(stderr, "\n");
				fallback_ok = 1;
			} else {
				fprintf(stderr, "  [%s] FAILED: %s\n",
				        fb_results[i].source, fb_results[i].error);
			}
		}

		if (fb_ok > 0)
			fprintf(stderr, "  %d fallback(s) succeeded\n", fb_ok);
	}

	/* If RPC failed, check if a fallback saved us */
	if (!rpc_ok) {
		if (fallback_ok) {
			if (!*out)
				*out = strdup(result.txid[0] ? result.txid :
				              "broadcast via fallback");
		} else {
			*out = strdup(result.error_msg);
			return 1;
		}
	}

	/* Layer 3: P2P verification (opt-in, separate from fallback) */
	if (g_verify_enabled) {
		fprintf(stderr, "\nVerifying transaction propagation...\n");
		int confirmed = verify_tx_propagation(result.txid, g_network,
		                                       g_verify_peers);
		if (confirmed == 0)
			fprintf(stderr, "Warning: tx not found in any peer mempool\n");
	}

	return 0;
}

/* Method registry */
static const MethodDef methods[] = {
	/* === Blockchain === */
	{"getblockchaininfo", "blockchain", "Returns blockchain state info",
	 cmd_getblockchaininfo, {}, 0},

	{"getblockcount", "blockchain", "Returns the height of the most-work chain",
	 cmd_getblockcount, {}, 0},

	{"getbestblockhash", "blockchain", "Returns the hash of the best block",
	 cmd_getbestblockhash, {}, 0},

	{"getblockhash", "blockchain", "Returns hash of block at height",
	 cmd_getblockhash,
	 {{"height", PARAM_INT, 1, "Block height"}}, 1},

	{"getblock", "blockchain", "Returns block data",
	 cmd_getblock,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"},
	  {"verbosity", PARAM_INT, 0, "0=hex, 1=json, 2=json+tx"}}, 2},

	{"getblockheader", "blockchain", "Returns block header",
	 cmd_getblockheader,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"},
	  {"verbose", PARAM_BOOL, 0, "true=json, false=hex"}}, 2},

	{"getdifficulty", "blockchain", "Returns proof-of-work difficulty",
	 cmd_getdifficulty, {}, 0},

	{"getchaintips", "blockchain", "Returns info about all chain tips",
	 cmd_getchaintips, {}, 0},

	{"getmempoolinfo", "blockchain", "Returns mempool state",
	 cmd_getmempoolinfo, {}, 0},

	{"getrawmempool", "blockchain", "Returns all txids in mempool",
	 cmd_getrawmempool,
	 {{"verbose", PARAM_BOOL, 0, "true=detailed info"},
	  {"mempool_sequence", PARAM_BOOL, 0, "Include sequence"}}, 2},

	/* === Wallet === */
	{"getbalance", "wallet", "Returns wallet balance",
	 cmd_getbalance,
	 {{"dummy", PARAM_STRING, 0, "Remains for backwards compat"},
	  {"minconf", PARAM_INT, 0, "Minimum confirmations"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"},
	  {"avoid_reuse", PARAM_BOOL, 0, "Avoid reused addresses"}}, 4},

	{"getbalances", "wallet", "Returns all balances",
	 cmd_getbalances, {}, 0},

	{"getwalletinfo", "wallet", "Returns wallet state info",
	 cmd_getwalletinfo, {}, 0},

	{"getnewaddress", "wallet", "Returns new address for receiving",
	 cmd_getnewaddress,
	 {{"label", PARAM_STRING, 0, "Address label"},
	  {"address_type", PARAM_STRING, 0, "legacy/p2sh-segwit/bech32/bech32m"}}, 2},

	{"getaddressinfo", "wallet", "Returns info about address",
	 cmd_getaddressinfo,
	 {{"address", PARAM_ADDRESS, 1, "Bitcoin address"}}, 1},

	{"listunspent", "wallet", "Returns unspent outputs",
	 cmd_listunspent,
	 {{"minconf", PARAM_INT, 0, "Minimum confirmations"},
	  {"maxconf", PARAM_INT, 0, "Maximum confirmations"},
	  {"addresses", PARAM_ARRAY, 0, "Filter by addresses"},
	  {"include_unsafe", PARAM_BOOL, 0, "Include unsafe outputs"},
	  {"query_options", PARAM_OBJECT, 0, "Query options"}}, 5},

	{"listtransactions", "wallet", "Returns recent transactions",
	 cmd_listtransactions,
	 {{"label", PARAM_STRING, 0, "Filter by label"},
	  {"count", PARAM_INT, 0, "Number of transactions"},
	  {"skip", PARAM_INT, 0, "Number to skip"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"}}, 4},

	{"listwallets", "wallet", "Returns list of loaded wallets",
	 cmd_listwallets, {}, 0},

	/* === Raw Transactions === */
	{"createrawtransaction", "rawtransactions", "Creates unsigned raw transaction",
	 cmd_createrawtransaction,
	 {{"inputs", PARAM_ARRAY, 1, "Transaction inputs"},
	  {"outputs", PARAM_ARRAY, 1, "Transaction outputs"},
	  {"locktime", PARAM_INT, 0, "Locktime"},
	  {"replaceable", PARAM_BOOL, 0, "RBF signal"}}, 4},

	{"decoderawtransaction", "rawtransactions", "Decodes raw transaction hex",
	 cmd_decoderawtransaction,
	 {{"hexstring", PARAM_HEX, 1, "Transaction hex"},
	  {"iswitness", PARAM_BOOL, 0, "SegWit transaction"}}, 2},

	{"decodescript", "rawtransactions", "Decodes script hex",
	 cmd_decodescript,
	 {{"hexstring", PARAM_HEX, 1, "Script hex"}}, 1},

	{"signrawtransactionwithwallet", "rawtransactions", "Signs raw transaction with wallet keys",
	 cmd_signrawtransactionwithwallet,
	 {{"hexstring", PARAM_HEX, 1, "Transaction hex"},
	  {"prevtxs", PARAM_ARRAY, 0, "Previous outputs"},
	  {"sighashtype", PARAM_STRING, 0, "Signature hash type"}}, 3},

	{"signrawtransactionwithkey", "rawtransactions", "Signs raw transaction with provided keys",
	 cmd_signrawtransactionwithkey,
	 {{"hexstring", PARAM_HEX, 1, "Transaction hex"},
	  {"privkeys", PARAM_ARRAY, 1, "Private keys"},
	  {"prevtxs", PARAM_ARRAY, 0, "Previous outputs"},
	  {"sighashtype", PARAM_STRING, 0, "Signature hash type"}}, 4},

	{"sendrawtransaction", "rawtransactions", "Submits raw transaction to network",
	 cmd_sendrawtransaction,
	 {{"hexstring", PARAM_HEX, 1, "Signed transaction hex"},
	  {"maxfeerate", PARAM_AMOUNT, 0, "Maximum fee rate"}}, 2},

	{"testmempoolaccept", "rawtransactions", "Tests if transactions would be accepted",
	 cmd_testmempoolaccept,
	 {{"rawtxs", PARAM_ARRAY, 1, "Array of transaction hex strings"},
	  {"maxfeerate", PARAM_AMOUNT, 0, "Maximum fee rate"}}, 2},

	{"getrawtransaction", "rawtransactions", "Returns raw transaction data",
	 cmd_getrawtransaction,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"verbose", PARAM_BOOL, 0, "Return JSON instead of hex"},
	  {"blockhash", PARAM_HEX, 0, "Block to look in"}}, 3},

	/* === Network === */
	{"getnetworkinfo", "network", "Returns network state info",
	 cmd_getnetworkinfo, {}, 0},

	{"getpeerinfo", "network", "Returns info about connected peers",
	 cmd_getpeerinfo, {}, 0},

	{"getconnectioncount", "network", "Returns number of connections",
	 cmd_getconnectioncount, {}, 0},

	/* === Control === */
	{"help", "control", "List commands or get help for command",
	 cmd_help,
	 {{"command", PARAM_STRING, 0, "Command name"}}, 1},

	{"stop", "control", "Stops the Bitcoin server",
	 cmd_stop, {}, 0},

	{"uptime", "control", "Returns server uptime in seconds",
	 cmd_uptime, {}, 0},

	/* === Utility === */
	{"validateaddress", "util", "Validates a bitcoin address",
	 cmd_validateaddress,
	 {{"address", PARAM_ADDRESS, 1, "Address to validate"}}, 1},

	{"estimatesmartfee", "util", "Estimates fee for confirmation target",
	 cmd_estimatesmartfee,
	 {{"conf_target", PARAM_INT, 1, "Confirmation target in blocks"},
	  {"estimate_mode", PARAM_STRING, 0, "UNSET/ECONOMICAL/CONSERVATIVE"}}, 2},

	/* === Phase 2: Wallet Sending === */
	{"sendtoaddress", "wallet", "Send to a bitcoin address",
	 cmd_sendtoaddress,
	 {{"address", PARAM_ADDRESS, 1, "Recipient address"},
	  {"amount", PARAM_AMOUNT, 1, "Amount in BTC"},
	  {"comment", PARAM_STRING, 0, "Comment for transaction"},
	  {"comment_to", PARAM_STRING, 0, "Comment for recipient"},
	  {"subtractfeefromamount", PARAM_BOOL, 0, "Deduct fee from amount"},
	  {"replaceable", PARAM_BOOL, 0, "Allow RBF"},
	  {"conf_target", PARAM_INT, 0, "Confirmation target"},
	  {"estimate_mode", PARAM_STRING, 0, "Fee estimate mode"}}, 8},

	{"sendmany", "wallet", "Send to multiple addresses",
	 cmd_sendmany,
	 {{"dummy", PARAM_STRING, 1, "Must be empty string"},
	  {"amounts", PARAM_OBJECT, 1, "Address:amount pairs"},
	  {"minconf", PARAM_INT, 0, "Minimum confirmations"},
	  {"comment", PARAM_STRING, 0, "Comment"},
	  {"subtractfeefrom", PARAM_ARRAY, 0, "Addresses to subtract fee from"},
	  {"replaceable", PARAM_BOOL, 0, "Allow RBF"},
	  {"conf_target", PARAM_INT, 0, "Confirmation target"},
	  {"estimate_mode", PARAM_STRING, 0, "Fee estimate mode"}}, 8},

	{"send", "wallet", "Send bitcoin (modern interface)",
	 cmd_send,
	 {{"outputs", PARAM_ARRAY, 1, "Output specifications"},
	  {"conf_target", PARAM_INT, 0, "Confirmation target"},
	  {"estimate_mode", PARAM_STRING, 0, "Fee estimate mode"},
	  {"fee_rate", PARAM_AMOUNT, 0, "Fee rate in sat/vB"},
	  {"options", PARAM_OBJECT, 0, "Additional options"}}, 5},

	{"bumpfee", "wallet", "Bump fee of a transaction (RBF)",
	 cmd_bumpfee,
	 {{"txid", PARAM_TXID, 1, "Transaction to bump"},
	  {"options", PARAM_OBJECT, 0, "Options (fee_rate, replaceable, etc)"}}, 2},

	{"psbtbumpfee", "wallet", "Bump fee via PSBT",
	 cmd_psbtbumpfee,
	 {{"txid", PARAM_TXID, 1, "Transaction to bump"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 2},

	{"settxfee", "wallet", "Set default transaction fee",
	 cmd_settxfee,
	 {{"amount", PARAM_AMOUNT, 1, "Fee in BTC/kvB"}}, 1},

	/* === Phase 2: Wallet Management === */
	{"createwallet", "wallet", "Create a new wallet",
	 cmd_createwallet,
	 {{"wallet_name", PARAM_STRING, 1, "Wallet name"},
	  {"disable_private_keys", PARAM_BOOL, 0, "Disable private keys"},
	  {"blank", PARAM_BOOL, 0, "Create blank wallet"},
	  {"passphrase", PARAM_STRING, 0, "Encryption passphrase"},
	  {"avoid_reuse", PARAM_BOOL, 0, "Avoid address reuse"},
	  {"descriptors", PARAM_BOOL, 0, "Use descriptors"},
	  {"load_on_startup", PARAM_BOOL, 0, "Load on startup"},
	  {"external_signer", PARAM_BOOL, 0, "Use external signer"}}, 8},

	{"loadwallet", "wallet", "Load a wallet",
	 cmd_loadwallet,
	 {{"filename", PARAM_STRING, 1, "Wallet file or directory"},
	  {"load_on_startup", PARAM_BOOL, 0, "Load on startup"}}, 2},

	{"unloadwallet", "wallet", "Unload a wallet",
	 cmd_unloadwallet,
	 {{"wallet_name", PARAM_STRING, 0, "Wallet to unload"},
	  {"load_on_startup", PARAM_BOOL, 0, "Update load on startup"}}, 2},

	{"backupwallet", "wallet", "Backup wallet to file",
	 cmd_backupwallet,
	 {{"destination", PARAM_STRING, 1, "Backup file path"}}, 1},

	{"restorewallet", "wallet", "Restore wallet from backup",
	 cmd_restorewallet,
	 {{"wallet_name", PARAM_STRING, 1, "New wallet name"},
	  {"backup_file", PARAM_STRING, 1, "Backup file path"},
	  {"load_on_startup", PARAM_BOOL, 0, "Load on startup"}}, 3},

	/* === Phase 2: Key Operations === */
	{"dumpprivkey", "wallet", "Dump private key for address",
	 cmd_dumpprivkey,
	 {{"address", PARAM_ADDRESS, 1, "Address to dump key for"}}, 1},

	{"importprivkey", "wallet", "Import private key",
	 cmd_importprivkey,
	 {{"privkey", PARAM_STRING, 1, "Private key in WIF"},
	  {"label", PARAM_STRING, 0, "Label"},
	  {"rescan", PARAM_BOOL, 0, "Rescan blockchain"}}, 3},

	{"importaddress", "wallet", "Import watch-only address",
	 cmd_importaddress,
	 {{"address", PARAM_STRING, 1, "Address or script"},
	  {"label", PARAM_STRING, 0, "Label"},
	  {"rescan", PARAM_BOOL, 0, "Rescan blockchain"},
	  {"p2sh", PARAM_BOOL, 0, "Add P2SH version"}}, 4},

	{"importpubkey", "wallet", "Import public key",
	 cmd_importpubkey,
	 {{"pubkey", PARAM_HEX, 1, "Public key hex"},
	  {"label", PARAM_STRING, 0, "Label"},
	  {"rescan", PARAM_BOOL, 0, "Rescan blockchain"}}, 3},

	{"importdescriptors", "wallet", "Import descriptors",
	 cmd_importdescriptors,
	 {{"requests", PARAM_ARRAY, 1, "Descriptor import requests"}}, 1},

	{"listdescriptors", "wallet", "List wallet descriptors",
	 cmd_listdescriptors,
	 {{"private", PARAM_BOOL, 0, "Include private keys"}}, 1},

	{"importmulti", "wallet", "Import multiple addresses/scripts",
	 cmd_importmulti,
	 {{"requests", PARAM_ARRAY, 1, "Import requests"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 2},

	{"dumpwallet", "wallet", "Dump all wallet keys to file",
	 cmd_dumpwallet,
	 {{"filename", PARAM_STRING, 1, "Output file"}}, 1},

	{"importwallet", "wallet", "Import wallet from dump file",
	 cmd_importwallet,
	 {{"filename", PARAM_STRING, 1, "Dump file to import"}}, 1},

	/* === Phase 2: Encryption === */
	{"encryptwallet", "wallet", "Encrypt wallet with passphrase",
	 cmd_encryptwallet,
	 {{"passphrase", PARAM_STRING, 1, "Encryption passphrase"}}, 1},

	{"walletpassphrase", "wallet", "Unlock wallet",
	 cmd_walletpassphrase,
	 {{"passphrase", PARAM_STRING, 1, "Wallet passphrase"},
	  {"timeout", PARAM_INT, 1, "Seconds to keep unlocked"}}, 2},

	{"walletlock", "wallet", "Lock wallet",
	 cmd_walletlock, {}, 0},

	{"walletpassphrasechange", "wallet", "Change wallet passphrase",
	 cmd_walletpassphrasechange,
	 {{"oldpassphrase", PARAM_STRING, 1, "Current passphrase"},
	  {"newpassphrase", PARAM_STRING, 1, "New passphrase"}}, 2},

	/* === Phase 2: Additional Wallet Queries === */
	{"gettransaction", "wallet", "Get detailed transaction info",
	 cmd_gettransaction,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"},
	  {"verbose", PARAM_BOOL, 0, "Include decoded transaction"}}, 3},

	{"listsinceblock", "wallet", "List transactions since block",
	 cmd_listsinceblock,
	 {{"blockhash", PARAM_HEX, 0, "Block hash to start from"},
	  {"target_confirmations", PARAM_INT, 0, "Min confirmations"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"},
	  {"include_removed", PARAM_BOOL, 0, "Include removed txs"}}, 4},

	{"getreceivedbyaddress", "wallet", "Get amount received by address",
	 cmd_getreceivedbyaddress,
	 {{"address", PARAM_ADDRESS, 1, "Address to query"},
	  {"minconf", PARAM_INT, 0, "Minimum confirmations"}}, 2},

	{"getreceivedbylabel", "wallet", "Get amount received by label",
	 cmd_getreceivedbylabel,
	 {{"label", PARAM_STRING, 1, "Label to query"},
	  {"minconf", PARAM_INT, 0, "Minimum confirmations"}}, 2},

	{"listreceivedbyaddress", "wallet", "List received by address",
	 cmd_listreceivedbyaddress,
	 {{"minconf", PARAM_INT, 0, "Minimum confirmations"},
	  {"include_empty", PARAM_BOOL, 0, "Include empty addresses"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"},
	  {"address_filter", PARAM_ADDRESS, 0, "Filter by address"}}, 4},

	{"listreceivedbylabel", "wallet", "List received by label",
	 cmd_listreceivedbylabel,
	 {{"minconf", PARAM_INT, 0, "Minimum confirmations"},
	  {"include_empty", PARAM_BOOL, 0, "Include empty labels"},
	  {"include_watchonly", PARAM_BOOL, 0, "Include watch-only"}}, 3},

	{"getrawchangeaddress", "wallet", "Get new change address",
	 cmd_getrawchangeaddress,
	 {{"address_type", PARAM_STRING, 0, "Address type"}}, 1},

	{"getaddressesbylabel", "wallet", "Get addresses by label",
	 cmd_getaddressesbylabel,
	 {{"label", PARAM_STRING, 1, "Label to query"}}, 1},

	{"listlabels", "wallet", "List all labels",
	 cmd_listlabels,
	 {{"purpose", PARAM_STRING, 0, "Filter by purpose"}}, 1},

	{"setlabel", "wallet", "Set label for address",
	 cmd_setlabel,
	 {{"address", PARAM_ADDRESS, 1, "Address"},
	  {"label", PARAM_STRING, 1, "Label"}}, 2},

	{"signmessage", "wallet", "Sign message with address key",
	 cmd_signmessage,
	 {{"address", PARAM_ADDRESS, 1, "Address to sign with"},
	  {"message", PARAM_STRING, 1, "Message to sign"}}, 2},

	{"abandontransaction", "wallet", "Abandon unconfirmed transaction",
	 cmd_abandontransaction,
	 {{"txid", PARAM_TXID, 1, "Transaction to abandon"}}, 1},

	{"abortrescan", "wallet", "Abort ongoing rescan",
	 cmd_abortrescan, {}, 0},

	{"rescanblockchain", "wallet", "Rescan blockchain for wallet txs",
	 cmd_rescanblockchain,
	 {{"start_height", PARAM_INT, 0, "Start height"},
	  {"stop_height", PARAM_INT, 0, "Stop height"}}, 2},

	{"listlockunspent", "wallet", "List locked unspent outputs",
	 cmd_listlockunspent, {}, 0},

	{"lockunspent", "wallet", "Lock/unlock unspent outputs",
	 cmd_lockunspent,
	 {{"unlock", PARAM_BOOL, 1, "True to unlock, false to lock"},
	  {"transactions", PARAM_ARRAY, 0, "Outputs to lock/unlock"}}, 2},

	{"keypoolrefill", "wallet", "Refill keypool",
	 cmd_keypoolrefill,
	 {{"newsize", PARAM_INT, 0, "New keypool size"}}, 1},

	{"getunconfirmedbalance", "wallet", "Get unconfirmed balance",
	 cmd_getunconfirmedbalance, {}, 0},

	{"listaddressgroupings", "wallet", "List address groupings",
	 cmd_listaddressgroupings, {}, 0},

	/* === Phase 3: PSBT === */
	{"createpsbt", "rawtransactions", "Create PSBT",
	 cmd_createpsbt,
	 {{"inputs", PARAM_ARRAY, 1, "Transaction inputs"},
	  {"outputs", PARAM_ARRAY, 1, "Transaction outputs"},
	  {"locktime", PARAM_INT, 0, "Locktime"},
	  {"replaceable", PARAM_BOOL, 0, "Allow RBF"}}, 4},

	{"decodepsbt", "rawtransactions", "Decode PSBT",
	 cmd_decodepsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"}}, 1},

	{"analyzepsbt", "rawtransactions", "Analyze PSBT",
	 cmd_analyzepsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"}}, 1},

	{"combinepsbt", "rawtransactions", "Combine PSBTs",
	 cmd_combinepsbt,
	 {{"txs", PARAM_ARRAY, 1, "Array of base64 PSBTs"}}, 1},

	{"finalizepsbt", "rawtransactions", "Finalize PSBT",
	 cmd_finalizepsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"},
	  {"extract", PARAM_BOOL, 0, "Extract final tx"}}, 2},

	{"joinpsbts", "rawtransactions", "Join PSBTs",
	 cmd_joinpsbts,
	 {{"txs", PARAM_ARRAY, 1, "Array of base64 PSBTs"}}, 1},

	{"converttopsbt", "rawtransactions", "Convert raw tx to PSBT",
	 cmd_converttopsbt,
	 {{"hexstring", PARAM_HEX, 1, "Raw transaction hex"},
	  {"permitsigdata", PARAM_BOOL, 0, "Allow signatures"},
	  {"iswitness", PARAM_BOOL, 0, "SegWit transaction"}}, 3},

	{"utxoupdatepsbt", "rawtransactions", "Update PSBT with UTXO data",
	 cmd_utxoupdatepsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"},
	  {"descriptors", PARAM_ARRAY, 0, "Descriptors"}}, 2},

	{"walletcreatefundedpsbt", "wallet", "Create and fund PSBT",
	 cmd_walletcreatefundedpsbt,
	 {{"inputs", PARAM_ARRAY, 1, "Inputs (can be empty)"},
	  {"outputs", PARAM_ARRAY, 1, "Outputs"},
	  {"locktime", PARAM_INT, 0, "Locktime"},
	  {"options", PARAM_OBJECT, 0, "Funding options"},
	  {"bip32derivs", PARAM_BOOL, 0, "Include BIP32 derivation"}}, 5},

	{"walletprocesspsbt", "wallet", "Sign PSBT with wallet",
	 cmd_walletprocesspsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"},
	  {"sign", PARAM_BOOL, 0, "Sign inputs"},
	  {"sighashtype", PARAM_STRING, 0, "Signature hash type"},
	  {"bip32derivs", PARAM_BOOL, 0, "Include BIP32 derivation"},
	  {"finalize", PARAM_BOOL, 0, "Finalize if complete"}}, 5},

	{"combinerawtransaction", "rawtransactions", "Combine raw transactions",
	 cmd_combinerawtransaction,
	 {{"txs", PARAM_ARRAY, 1, "Array of raw transaction hex"}}, 1},

	{"fundrawtransaction", "rawtransactions", "Fund raw transaction",
	 cmd_fundrawtransaction,
	 {{"hexstring", PARAM_HEX, 1, "Raw transaction hex"},
	  {"options", PARAM_OBJECT, 0, "Funding options"},
	  {"iswitness", PARAM_BOOL, 0, "SegWit transaction"}}, 3},

	/* === Phase 4: Network === */
	{"addnode", "network", "Add/remove node",
	 cmd_addnode,
	 {{"node", PARAM_STRING, 1, "Node address"},
	  {"command", PARAM_STRING, 1, "add/remove/onetry"}}, 2},

	{"disconnectnode", "network", "Disconnect peer",
	 cmd_disconnectnode,
	 {{"address", PARAM_STRING, 0, "Node address"},
	  {"nodeid", PARAM_INT, 0, "Node ID"}}, 2},

	{"setban", "network", "Add/remove from ban list",
	 cmd_setban,
	 {{"subnet", PARAM_STRING, 1, "IP/subnet"},
	  {"command", PARAM_STRING, 1, "add/remove"},
	  {"bantime", PARAM_INT, 0, "Ban duration"},
	  {"absolute", PARAM_BOOL, 0, "Absolute timestamp"}}, 4},

	{"listbanned", "network", "List banned nodes",
	 cmd_listbanned, {}, 0},

	{"clearbanned", "network", "Clear ban list",
	 cmd_clearbanned, {}, 0},

	{"getnettotals", "network", "Get network traffic stats",
	 cmd_getnettotals, {}, 0},

	{"getnodeaddresses", "network", "Get known node addresses",
	 cmd_getnodeaddresses,
	 {{"count", PARAM_INT, 0, "Number of addresses"},
	  {"network", PARAM_STRING, 0, "Filter by network"}}, 2},

	{"getaddednodeinfo", "network", "Get added node info",
	 cmd_getaddednodeinfo,
	 {{"node", PARAM_STRING, 0, "Node to query"}}, 1},

	{"ping", "network", "Ping all peers",
	 cmd_ping, {}, 0},

	{"setnetworkactive", "network", "Enable/disable network",
	 cmd_setnetworkactive,
	 {{"state", PARAM_BOOL, 1, "Network state"}}, 1},

	/* === Phase 4: Mining/Generating === */
	{"getmininginfo", "mining", "Get mining info",
	 cmd_getmininginfo, {}, 0},

	{"getnetworkhashps", "mining", "Get network hash rate",
	 cmd_getnetworkhashps,
	 {{"nblocks", PARAM_INT, 0, "Blocks to average"},
	  {"height", PARAM_INT, 0, "Height to calculate at"}}, 2},

	{"generatetoaddress", "generating", "Generate blocks to address",
	 cmd_generatetoaddress,
	 {{"nblocks", PARAM_INT, 1, "Number of blocks"},
	  {"address", PARAM_ADDRESS, 1, "Mining address"},
	  {"maxtries", PARAM_INT, 0, "Max tries"}}, 3},

	{"generateblock", "generating", "Generate block with transactions",
	 cmd_generateblock,
	 {{"output", PARAM_STRING, 1, "Coinbase output"},
	  {"transactions", PARAM_ARRAY, 1, "Transactions to include"},
	  {"submit", PARAM_BOOL, 0, "Submit block"}}, 3},

	{"generatetodescriptor", "generating", "Generate blocks to descriptor",
	 cmd_generatetodescriptor,
	 {{"nblocks", PARAM_INT, 1, "Number of blocks"},
	  {"descriptor", PARAM_STRING, 1, "Output descriptor"},
	  {"maxtries", PARAM_INT, 0, "Max tries"}}, 3},

	{"getblocktemplate", "mining", "Get block template",
	 cmd_getblocktemplate,
	 {{"template_request", PARAM_OBJECT, 0, "Template request"}}, 1},

	{"submitblock", "mining", "Submit a block",
	 cmd_submitblock,
	 {{"hexdata", PARAM_HEX, 1, "Block hex"}}, 1},

	{"submitheader", "mining", "Submit a block header",
	 cmd_submitheader,
	 {{"hexdata", PARAM_HEX, 1, "Header hex"}}, 1},

	{"prioritisetransaction", "mining", "Prioritize transaction",
	 cmd_prioritisetransaction,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"dummy", PARAM_FLOAT, 0, "Unused"},
	  {"fee_delta", PARAM_INT, 1, "Fee delta in satoshis"}}, 3},

	/* === Phase 5: Advanced Blockchain === */
	{"getblockfilter", "blockchain", "Get BIP157 block filter",
	 cmd_getblockfilter,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"},
	  {"filtertype", PARAM_STRING, 0, "Filter type"}}, 2},

	{"getblockstats", "blockchain", "Get block statistics",
	 cmd_getblockstats,
	 {{"hash_or_height", PARAM_HEIGHT_OR_HASH, 1, "Block hash or height"},
	  {"stats", PARAM_ARRAY, 0, "Stats to return"}}, 2},

	{"getchaintxstats", "blockchain", "Get chain TX statistics",
	 cmd_getchaintxstats,
	 {{"nblocks", PARAM_INT, 0, "Block window size"},
	  {"blockhash", PARAM_HEX, 0, "End block"}}, 2},

	{"getmempoolancestors", "blockchain", "Get mempool ancestors",
	 cmd_getmempoolancestors,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"verbose", PARAM_BOOL, 0, "Verbose output"}}, 2},

	{"getmempooldescendants", "blockchain", "Get mempool descendants",
	 cmd_getmempooldescendants,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"verbose", PARAM_BOOL, 0, "Verbose output"}}, 2},

	{"getmempoolentry", "blockchain", "Get mempool entry",
	 cmd_getmempoolentry,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"}}, 1},

	{"gettxout", "blockchain", "Get UTXO info",
	 cmd_gettxout,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"},
	  {"n", PARAM_INT, 1, "Output index"},
	  {"include_mempool", PARAM_BOOL, 0, "Include mempool"}}, 3},

	{"gettxoutproof", "blockchain", "Get merkle proof",
	 cmd_gettxoutproof,
	 {{"txids", PARAM_ARRAY, 1, "Transaction IDs"},
	  {"blockhash", PARAM_HEX, 0, "Block to search in"}}, 2},

	{"gettxoutsetinfo", "blockchain", "Get UTXO set info",
	 cmd_gettxoutsetinfo,
	 {{"hash_type", PARAM_STRING, 0, "Hash type"},
	  {"hash_or_height", PARAM_HEIGHT_OR_HASH, 0, "Block reference"},
	  {"use_index", PARAM_BOOL, 0, "Use coinstats index"}}, 3},

	{"verifytxoutproof", "blockchain", "Verify merkle proof",
	 cmd_verifytxoutproof,
	 {{"proof", PARAM_STRING, 1, "Merkle proof hex"}}, 1},

	{"scantxoutset", "blockchain", "Scan UTXO set",
	 cmd_scantxoutset,
	 {{"action", PARAM_STRING, 1, "start/abort/status"},
	  {"scanobjects", PARAM_ARRAY, 0, "Scan objects"}}, 2},

	{"preciousblock", "blockchain", "Mark block as precious",
	 cmd_preciousblock,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"}}, 1},

	{"invalidateblock", "blockchain", "Permanently mark a block as invalid",
	 cmd_invalidateblock,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"}}, 1},

	{"reconsiderblock", "blockchain", "Remove invalidity status of a block",
	 cmd_reconsiderblock,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"}}, 1},

	{"pruneblockchain", "blockchain", "Prune blockchain",
	 cmd_pruneblockchain,
	 {{"height", PARAM_INT, 1, "Prune to height"}}, 1},

	{"savemempool", "blockchain", "Save mempool to disk",
	 cmd_savemempool, {}, 0},

	{"verifychain", "blockchain", "Verify blockchain",
	 cmd_verifychain,
	 {{"checklevel", PARAM_INT, 0, "Check level 0-4"},
	  {"nblocks", PARAM_INT, 0, "Blocks to check"}}, 2},

	/* === Phase 5: Utility === */
	{"createmultisig", "util", "Create multisig address",
	 cmd_createmultisig,
	 {{"nrequired", PARAM_INT, 1, "Required signatures"},
	  {"keys", PARAM_ARRAY, 1, "Public keys"},
	  {"address_type", PARAM_STRING, 0, "Address type"}}, 3},

	{"deriveaddresses", "util", "Derive addresses from descriptor",
	 cmd_deriveaddresses,
	 {{"descriptor", PARAM_STRING, 1, "Output descriptor"},
	  {"range", PARAM_ARRAY, 0, "Derivation range"}}, 2},

	{"getdescriptorinfo", "util", "Get descriptor info",
	 cmd_getdescriptorinfo,
	 {{"descriptor", PARAM_STRING, 1, "Output descriptor"}}, 1},

	{"getindexinfo", "util", "Get index info",
	 cmd_getindexinfo,
	 {{"index_name", PARAM_STRING, 0, "Index name"}}, 1},

	{"signmessagewithprivkey", "util", "Sign message with private key",
	 cmd_signmessagewithprivkey,
	 {{"privkey", PARAM_STRING, 1, "Private key WIF"},
	  {"message", PARAM_STRING, 1, "Message to sign"}}, 2},

	{"verifymessage", "util", "Verify signed message",
	 cmd_verifymessage,
	 {{"address", PARAM_ADDRESS, 1, "Signing address"},
	  {"signature", PARAM_STRING, 1, "Signature"},
	  {"message", PARAM_STRING, 1, "Original message"}}, 3},

	/* === Phase 5: Control === */
	{"getmemoryinfo", "control", "Get memory usage info",
	 cmd_getmemoryinfo,
	 {{"mode", PARAM_STRING, 0, "stats/mallocinfo"}}, 1},

	{"getrpcinfo", "control", "Get RPC server info",
	 cmd_getrpcinfo, {}, 0},

	{"logging", "control", "Get/set logging categories",
	 cmd_logging,
	 {{"include", PARAM_ARRAY, 0, "Categories to include"},
	  {"exclude", PARAM_ARRAY, 0, "Categories to exclude"}}, 2},

	/* === Phase 6: Bitcoin Core 30.x === */
	{"createwalletdescriptor", "wallet", "Create descriptor for wallet",
	 cmd_createwalletdescriptor,
	 {{"type", PARAM_STRING, 1, "Descriptor type"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 2},

	{"descriptorprocesspsbt", "rawtransactions", "Process PSBT with descriptors",
	 cmd_descriptorprocesspsbt,
	 {{"psbt", PARAM_STRING, 1, "Base64 PSBT"},
	  {"descriptors", PARAM_ARRAY, 1, "Descriptors"},
	  {"sighashtype", PARAM_STRING, 0, "Signature hash type"},
	  {"bip32derivs", PARAM_BOOL, 0, "Include BIP32 derivation"},
	  {"finalize", PARAM_BOOL, 0, "Finalize if complete"}}, 5},

	{"dumptxoutset", "blockchain", "Dump UTXO set to file",
	 cmd_dumptxoutset,
	 {{"path", PARAM_STRING, 1, "Output file path"},
	  {"type", PARAM_STRING, 0, "Dump type"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 3},

	{"enumeratesigners", "wallet", "List external signers",
	 cmd_enumeratesigners, {}, 0},

	{"getaddrmaninfo", "network", "Get address manager info",
	 cmd_getaddrmaninfo, {}, 0},

	{"getblockfrompeer", "blockchain", "Request block from peer",
	 cmd_getblockfrompeer,
	 {{"blockhash", PARAM_HEX, 1, "Block hash"},
	  {"peer_id", PARAM_INT, 1, "Peer ID"}}, 2},

	{"getchainstates", "blockchain", "Get chain states info",
	 cmd_getchainstates, {}, 0},

	{"getdeploymentinfo", "blockchain", "Get deployment info",
	 cmd_getdeploymentinfo,
	 {{"blockhash", PARAM_HEX, 0, "Block hash"}}, 1},

	{"getdescriptoractivity", "blockchain", "Get descriptor activity",
	 cmd_getdescriptoractivity,
	 {{"blockhashes", PARAM_ARRAY, 0, "Block hashes"},
	  {"scanobjects", PARAM_ARRAY, 0, "Scan objects"},
	  {"include_mempool", PARAM_BOOL, 0, "Include mempool"}}, 3},

	{"gethdkeys", "wallet", "Get HD keys",
	 cmd_gethdkeys,
	 {{"options", PARAM_OBJECT, 0, "Options"}}, 1},

	{"getprioritisedtransactions", "mining", "Get prioritised transactions",
	 cmd_getprioritisedtransactions, {}, 0},

	{"gettxspendingprevout", "blockchain", "Get tx spending prevout",
	 cmd_gettxspendingprevout,
	 {{"outputs", PARAM_ARRAY, 1, "Outputs to check"}}, 1},

	{"getzmqnotifications", "control", "Get ZMQ notification info",
	 cmd_getzmqnotifications, {}, 0},

	{"importmempool", "blockchain", "Import mempool from file",
	 cmd_importmempool,
	 {{"filepath", PARAM_STRING, 1, "Mempool file path"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 2},

	{"importprunedfunds", "wallet", "Import pruned funds",
	 cmd_importprunedfunds,
	 {{"rawtransaction", PARAM_HEX, 1, "Raw transaction"},
	  {"txoutproof", PARAM_STRING, 1, "TX out proof"}}, 2},

	{"listwalletdir", "wallet", "List wallet directory",
	 cmd_listwalletdir, {}, 0},

	{"loadtxoutset", "blockchain", "Load UTXO set from file",
	 cmd_loadtxoutset,
	 {{"path", PARAM_STRING, 1, "UTXO set file path"}}, 1},

	{"migratewallet", "wallet", "Migrate wallet to descriptor",
	 cmd_migratewallet,
	 {{"wallet_name", PARAM_STRING, 0, "Wallet name"},
	  {"passphrase", PARAM_STRING, 0, "Passphrase"}}, 2},

	{"removeprunedfunds", "wallet", "Remove pruned funds",
	 cmd_removeprunedfunds,
	 {{"txid", PARAM_TXID, 1, "Transaction ID"}}, 1},

	{"scanblocks", "blockchain", "Scan blocks for descriptors",
	 cmd_scanblocks,
	 {{"action", PARAM_STRING, 1, "start/abort/status"},
	  {"scanobjects", PARAM_ARRAY, 0, "Scan objects"},
	  {"start_height", PARAM_INT, 0, "Start height"},
	  {"stop_height", PARAM_INT, 0, "Stop height"},
	  {"filtertype", PARAM_STRING, 0, "Filter type"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 6},

	{"sendall", "wallet", "Send entire wallet balance",
	 cmd_sendall,
	 {{"recipients", PARAM_ARRAY, 1, "Recipients"},
	  {"conf_target", PARAM_INT, 0, "Confirmation target"},
	  {"estimate_mode", PARAM_STRING, 0, "Fee estimate mode"},
	  {"fee_rate", PARAM_AMOUNT, 0, "Fee rate"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 5},

	{"setwalletflag", "wallet", "Set wallet flag",
	 cmd_setwalletflag,
	 {{"flag", PARAM_STRING, 1, "Flag name"},
	  {"value", PARAM_BOOL, 0, "Flag value"}}, 2},

	{"simulaterawtransaction", "rawtransactions", "Simulate raw transaction",
	 cmd_simulaterawtransaction,
	 {{"rawtxs", PARAM_ARRAY, 0, "Raw transactions"},
	  {"options", PARAM_OBJECT, 0, "Options"}}, 2},

	{"submitpackage", "rawtransactions", "Submit transaction package",
	 cmd_submitpackage,
	 {{"package", PARAM_ARRAY, 1, "Package of raw transactions"},
	  {"maxfeerate", PARAM_AMOUNT, 0, "Max fee rate"},
	  {"maxburnamount", PARAM_AMOUNT, 0, "Max burn amount"}}, 3},

	{"waitforblock", "blockchain", "Wait for specific block",
	 cmd_waitforblock,
	 {{"blockhash", PARAM_HEX, 1, "Block hash to wait for"},
	  {"timeout", PARAM_INT, 0, "Timeout in ms"}}, 2},

	{"waitforblockheight", "blockchain", "Wait for block height",
	 cmd_waitforblockheight,
	 {{"height", PARAM_INT, 1, "Block height"},
	  {"timeout", PARAM_INT, 0, "Timeout in ms"}}, 2},

	{"waitfornewblock", "blockchain", "Wait for new block",
	 cmd_waitfornewblock,
	 {{"timeout", PARAM_INT, 0, "Timeout in ms"},
	  {"current_tip", PARAM_HEX, 0, "Current tip hash"}}, 2},

	{"walletdisplayaddress", "wallet", "Display address on external signer",
	 cmd_walletdisplayaddress,
	 {{"address", PARAM_ADDRESS, 1, "Address to display"}}, 1},

	/* === Missing Wallet Methods === */
	{"addmultisigaddress", "wallet", "Add multisig address to wallet",
	 cmd_addmultisigaddress,
	 {{"nrequired", PARAM_INT, 1, "Required signatures"},
	  {"keys", PARAM_ARRAY, 1, "Public keys or addresses"},
	  {"label", PARAM_STRING, 0, "Label"},
	  {"address_type", PARAM_STRING, 0, "Address type"}}, 4},

	{"newkeypool", "wallet", "Flush and refill keypool",
	 cmd_newkeypool, {}, 0},

	{"upgradewallet", "wallet", "Upgrade wallet to latest format",
	 cmd_upgradewallet,
	 {{"version", PARAM_INT, 0, "Target wallet version"}}, 1},

	{"sethdseed", "wallet", "Set HD seed (deprecated)",
	 cmd_sethdseed,
	 {{"newkeypool", PARAM_BOOL, 0, "Flush old keypool"},
	  {"seed", PARAM_STRING, 0, "WIF private key for seed"}}, 2},

	/* Sentinel */
	{NULL, NULL, NULL, NULL, {}, 0}
};

/* Find method by name */
const MethodDef *method_find(const char *name)
{
	const MethodDef *m;
	for (m = methods; m->name != NULL; m++) {
		if (strcmp(m->name, name) == 0)
			return m;
	}
	return NULL;
}

/* List all methods */
void method_list_all(void)
{
	const char *current_cat = NULL;
	const MethodDef *m;

	printf("Available commands:\n\n");

	for (m = methods; m->name != NULL; m++) {
		if (current_cat == NULL || strcmp(current_cat, m->category) != 0) {
			current_cat = m->category;
			printf("== %s ==\n", current_cat);
		}
		printf("  %-30s %s\n", m->name, m->description);
	}
}

/* List methods by category */
void method_list_category(const char *category)
{
	const MethodDef *m;

	printf("== %s ==\n", category);
	for (m = methods; m->name != NULL; m++) {
		if (strcmp(m->category, category) == 0) {
			printf("  %-30s %s\n", m->name, m->description);
		}
	}
}

/* Print help for specific method */
void method_print_help(const MethodDef *method)
{
	int i;

	printf("%s\n\n", method->name);
	printf("%s\n\n", method->description);

	if (method->param_count > 0) {
		printf("Arguments:\n");
		for (i = 0; i < method->param_count; i++) {
			const ParamDef *p = &method->params[i];
			printf("  %d. %-20s (%s%s)\n",
			       i + 1,
			       p->name,
			       p->required ? "required" : "optional",
			       p->description[0] ? ", " : "");
			if (p->description[0])
				printf("      %s\n", p->description);
		}
	} else {
		printf("No arguments.\n");
	}
}

/* Check if string looks like a number */
static int is_number(const char *s)
{
	if (*s == '-') s++;
	if (!*s) return 0;
	while (*s) {
		if (!isdigit(*s) && *s != '.') return 0;
		s++;
	}
	return 1;
}

/* Check if string is boolean */
static int is_bool(const char *s)
{
	return strcmp(s, "true") == 0 || strcmp(s, "false") == 0;
}

/* Build JSON params array from argv */
char *method_build_params(const MethodDef *method, int argc, char **argv)
{
	char *buf;
	size_t bufsize = 8192;
	size_t pos = 0;
	int i;

	buf = malloc(bufsize);
	if (!buf) return NULL;

	buf[pos++] = '[';

	for (i = 0; i < argc && i < method->param_count; i++) {
		const ParamDef *p = &method->params[i];
		const char *arg = argv[i];
		size_t arglen = strlen(arg);

		/* Ensure buffer has enough space: arg + quotes + comma + some margin */
		while (pos + arglen + 64 > bufsize) {
			bufsize *= 2;
			char *newbuf = realloc(buf, bufsize);
			if (!newbuf) { free(buf); return NULL; }
			buf = newbuf;
		}

		if (i > 0) buf[pos++] = ',';

		/* Format based on type */
		switch (p->type) {
		case PARAM_INT:
		case PARAM_FLOAT:
		case PARAM_AMOUNT:
			/* Numbers go unquoted */
			pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
			break;

		case PARAM_BOOL:
			/* Booleans go unquoted */
			if (is_bool(arg))
				pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
			else
				pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", arg);
			break;

		case PARAM_ARRAY:
		case PARAM_OBJECT:
			/* Already JSON, pass through */
			pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
			break;

		case PARAM_HEIGHT_OR_HASH:
			/* If all digits, send as number; otherwise quote as string */
			if (is_number(arg) && strchr(arg, '.') == NULL)
				pos += snprintf(buf + pos, bufsize - pos, "%s", arg);
			else
				pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", arg);
			break;

		default:
			/* Strings get quoted */
			pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", arg);
			break;
		}
	}

	buf[pos++] = ']';
	buf[pos] = '\0';

	return buf;
}

/* Build JSON params object for named parameters */
char *method_build_named_params(const MethodDef *method, int argc, char **argv)
{
	char *buf;
	size_t bufsize = 8192;
	size_t pos = 0;
	int i, first = 1;

	/* Collect positional (non-key=value) args */
	char *positional[64];
	int pos_count = 0;

	buf = malloc(bufsize);
	if (!buf) return NULL;

	buf[pos++] = '{';

	for (i = 0; i < argc; i++) {
		char *eq = strchr(argv[i], '=');
		if (!eq) {
			/* Positional arg — collect for "args" array */
			if (pos_count < 64)
				positional[pos_count++] = argv[i];
			continue;
		}

		char key[256];
		size_t keylen = eq - argv[i];
		if (keylen >= sizeof(key)) keylen = sizeof(key) - 1;
		memcpy(key, argv[i], keylen);
		key[keylen] = '\0';

		const char *value = eq + 1;
		size_t valuelen = strlen(value);

		/* Ensure buffer has enough space */
		while (pos + keylen + valuelen + 64 > bufsize) {
			bufsize *= 2;
			char *newbuf = realloc(buf, bufsize);
			if (!newbuf) { free(buf); return NULL; }
			buf = newbuf;
		}

		if (!first) buf[pos++] = ',';
		first = 0;

		/* Find param type */
		const ParamDef *p = NULL;
		int j;
		for (j = 0; j < method->param_count; j++) {
			if (strcmp(method->params[j].name, key) == 0) {
				p = &method->params[j];
				break;
			}
		}

		pos += snprintf(buf + pos, bufsize - pos, "\"%s\":", key);

		/* Format value based on type or inference */
		if (p && (p->type == PARAM_INT || p->type == PARAM_FLOAT ||
		          p->type == PARAM_AMOUNT)) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else if (p && p->type == PARAM_BOOL) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else if (p && (p->type == PARAM_ARRAY || p->type == PARAM_OBJECT)) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else if (is_number(value)) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else if (is_bool(value)) {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else if (value[0] == '[' || value[0] == '{') {
			pos += snprintf(buf + pos, bufsize - pos, "%s", value);
		} else {
			pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", value);
		}
	}

	/* Append positional args as "args" array */
	if (pos_count > 0) {
		/* Ensure buffer space */
		size_t needed = 32;
		for (i = 0; i < pos_count; i++)
			needed += strlen(positional[i]) + 16;
		while (pos + needed > bufsize) {
			bufsize *= 2;
			char *newbuf = realloc(buf, bufsize);
			if (!newbuf) { free(buf); return NULL; }
			buf = newbuf;
		}

		if (!first) buf[pos++] = ',';
		pos += snprintf(buf + pos, bufsize - pos, "\"args\":[");
		for (i = 0; i < pos_count; i++) {
			if (i > 0) buf[pos++] = ',';
			/* Type inference for positional args */
			if (is_number(positional[i]))
				pos += snprintf(buf + pos, bufsize - pos, "%s", positional[i]);
			else if (is_bool(positional[i]))
				pos += snprintf(buf + pos, bufsize - pos, "%s", positional[i]);
			else if (positional[i][0] == '[' || positional[i][0] == '{')
				pos += snprintf(buf + pos, bufsize - pos, "%s", positional[i]);
			else
				pos += snprintf(buf + pos, bufsize - pos, "\"%s\"", positional[i]);
		}
		buf[pos++] = ']';
	}

	buf[pos++] = '}';
	buf[pos] = '\0';

	return buf;
}

/* Extract result from JSON-RPC response */
char *method_extract_result(const char *response, int *error_code)
{
	const char *result;
	const char *error;
	char *out;

	*error_code = 0;

	if (!response)
		return NULL;

	/* Check for error */
	error = json_find_object(response, "error");
	if (error) {
		int code = (int)json_get_int(error, "code");
		if (code != 0) {
			*error_code = code;
			char msg[2048];
			if (json_get_string(error, "message", msg, sizeof(msg)) > 0) {
				size_t needed = strlen(msg) + 64;
				out = malloc(needed);
				snprintf(out, needed, "error code: %d\nerror message:\n%s", code, msg);
				return out;
			}
		}
	}

	/* Check if error is null */
	if (!json_is_null(response, "error")) {
		/* error field exists and is not null - check for error object */
		const char *err_obj = json_find_value(response, "error");
		if (err_obj && *err_obj == '{') {
			int code = (int)json_get_int(err_obj, "code");
			if (code != 0) {
				*error_code = code;
				char msg[2048];
				if (json_get_string(err_obj, "message", msg, sizeof(msg)) > 0) {
					size_t needed = strlen(msg) + 64;
					out = malloc(needed);
					snprintf(out, needed, "error code: %d\nerror message:\n%s", code, msg);
					return out;
				}
			}
		}
	}

	/* Find result field */
	result = json_find_value(response, "result");
	if (!result)
		return strdup(response);  /* Fallback to raw response */

	/* Determine result type and extract */
	if (*result == '"') {
		/* String result - extract without quotes */
		/* First pass: find string length (accounting for escapes) */
		const char *p = result + 1;
		size_t len = 0;
		while (*p && *p != '"') {
			if (*p == '\\' && *(p+1)) {
				p += 2;  /* Skip escape sequence */
			} else {
				p++;
			}
			len++;
		}

		/* Allocate exact size needed */
		char *buf = malloc(len + 1);
		if (!buf) return NULL;

		/* Second pass: copy with escape processing */
		p = result + 1;
		size_t i = 0;
		while (*p && *p != '"') {
			if (*p == '\\' && *(p+1)) {
				p++;
				switch (*p) {
				case 'n': buf[i++] = '\n'; break;
				case 't': buf[i++] = '\t'; break;
				case '"': buf[i++] = '"'; break;
				case '\\': buf[i++] = '\\'; break;
				default: buf[i++] = *p; break;
				}
			} else {
				buf[i++] = *p;
			}
			p++;
		}
		buf[i] = '\0';
		return buf;  /* Already dynamically allocated, no need for strdup */
	} else if (*result == '{' || *result == '[') {
		/* Object or array - find closing bracket */
		const char *end = json_find_closing(result);
		if (end) {
			size_t len = end - result + 1;
			out = malloc(len + 1);
			memcpy(out, result, len);
			out[len] = '\0';
			return out;
		}
	} else if (strncmp(result, "null", 4) == 0) {
		return NULL;
	} else {
		/* Number or boolean - find end */
		const char *end = result;
		while (*end && *end != ',' && *end != '}' && *end != ']' &&
		       *end != ' ' && *end != '\n' && *end != '\r')
			end++;
		size_t len = end - result;
		out = malloc(len + 1);
		memcpy(out, result, len);
		out[len] = '\0';
		return out;
	}

	return strdup(response);
}

/* Generic command handler - builds params and calls RPC */
static int cmd_generic(RpcClient *rpc, const char *method, int argc, char **argv, char **out)
{
	const MethodDef *m = method_find(method);
	char *params;
	char *response;
	int error_code;

	if (!m) {
		*out = strdup("Unknown method");
		return 1;
	}

	/* Build params - use named mode if enabled */
	if (g_named_mode)
		params = method_build_named_params(m, argc, argv);
	else
		params = method_build_params(m, argc, argv);

	if (!params) {
		*out = strdup("Failed to build parameters");
		return 1;
	}

	/* Make RPC call */
	response = rpc_call(rpc, method, params);
	free(params);

	if (!response) {
		*out = strdup("RPC call failed");
		return 1;
	}

	/* Extract result */
	*out = method_extract_result(response, &error_code);
	free(response);

	return error_code != 0 ? abs(error_code) : 0;
}
