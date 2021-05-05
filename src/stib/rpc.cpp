#include <key.h>
#include <base58.h>
#include <thread>
#include <script/standard.h>

#include <script/script.h>
#include <utilstrencodings.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <util.h>
#include <addressindex.h>
#include <validation.h>
#include <streams.h>

void GenerateFromXPUB(std::string xpubkey, int from, int count, std::vector<std::string>& out);
void RecoverFromXPUB(std::string xpubkey, UniValue& out);
void RecoverTxsFromXPUB(std::string xpubkey, unsigned int lastBlockHeight, std::vector<std::tuple<uint256, unsigned int, unsigned int>>& out);
int GetLastUsedExternalSegWitIndex(std::string xpub);
int GetFirstUsedBlock(std::string xpub);

inline static void UInt32SetLE(void *b4, uint32_t u)
{
    static_cast<uint8_t*>(b4)[3] = ((u >> 24) & 0xff);
    static_cast<uint8_t*>(b4)[2] = ((u >> 16) & 0xff);
    static_cast<uint8_t*>(b4)[1] = ((u >> 8) & 0xff);
    static_cast<uint8_t*>(b4)[0] = (u & 0xff );
}

// RPC
UniValue stbtsgenxpubaddresses(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgenxpubaddresses\n"
            "\nReturns 'count' HD generated address for an 'xpub', starting  from 'start' index.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "  \"start\", (optional default to 0) index of the first address to generate\n"
            "  \"count\", (optional default to 100) numbre of addresses to generate\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stbtsgenxpubaddresses", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stbtsgenxpubaddresses", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );

    std::string xpubkey;
    int  from = 0;
    int  count = 100;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }

        val = find_value(request.params[0].get_obj(), "from");
        if (val.isNum()) {
            from = val.get_int();
        }

        val = find_value(request.params[0].get_obj(), "count");
        if (val.isNum()) {
            count = val.get_int();
        }
    }

    std::vector<std::string> v;
    GenerateFromXPUB(xpubkey, from, count, v);

    UniValue addrs(UniValue::VARR);

    for(auto addr : v)
    {
        addrs.push_back(addr);
    }

    return addrs;
}

UniValue stbtsgetxpubutxos(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgetxpubutxos\n"
            "\nReturns 'count' HD generated address for an 'xpub', starting  from 'start' index.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stbtsgetxpubutxos", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stbtsgetxpubutxos", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );

    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing!!");
    }

    if(xpubkey.size() < 4
       || xpubkey[1] != 'p' || xpubkey[2] != 'u' || xpubkey[3] != 'b')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    UniValue utxos(UniValue::VARR);
    RecoverFromXPUB(xpubkey, utxos);
    return utxos;

}


UniValue stbtsgetxpubtxs(const JSONRPCRequest& request)
{
	BCLog::LogFlags logFlag = BCLog::ALL; //::RPC;
    if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
         throw std::runtime_error(
             "stbtsgetxpubtxs\n"
             "\nReturns 'count' HD generated address for an 'xpub', starting  from 'start' index.\n"
             "\nArguments:\n"
             "{\n"
             "  \"xpubkey\",  account extended public key ExtPubKey\n"
             "  \"lastblockheight\",  (unsigned int) Txs befor BlockHeight\n"
             "}\n"
             "\nResult\n"
             "[\n"
             "  {\n"
             "  \"addresses\"\n"
             "    [\n"
             "      \"address\"  (string) The base58check encoded address\n"
             "      ,...\n"
             "    ]\n"
             "  }\n"
             "]\n"
             "\nExamples:\n"
             + HelpExampleCli("stbtsgetxpubtxs", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\", \"lastblockheight\": 0}'")
             + HelpExampleRpc("stbtsgetxpubtxs", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\", \"lastblockheight\": 0}'")
             );

    extern bool fTxIndex;
    if(!fTxIndex)
    {
        LogPrintf("stbtsgetxpubtxs: Error, bitcoind is not started with -txindex option.\n");
        return tinyformat::format(R"({"result":null,"error":"bitcoind is not started with -txindex option"})");
    }

    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing!!");
    }

    if(xpubkey.size() < 4
       || xpubkey[1] != 'p' || xpubkey[2] != 'u' || xpubkey[3] != 'b')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }


	LogPrintf("xpub found.\n");

    std::vector<std::tuple<uint256, unsigned int, unsigned int>> out;
    unsigned int lastBlockHeight = 0;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "lastblockheight");
        if (val.isNum()) {
            lastBlockHeight = val.get_uint();
        }
    }
    else
    {
        throw JSONRPCError(-1, "lastblockheight is missing!!");
    }

    RecoverTxsFromXPUB(xpubkey, lastBlockHeight, out);

    LogPrint(logFlag, "stbtsgetxpubtxs : %d transactions found.\n", out.size());

    UniValue txs(UniValue::VARR);
    
    // txs.reserve(out.size());

    for(auto [txhash, blockHeight, timestamp]: out)
    {
        CTransactionRef tx;
        uint256 hash_block;

        if(GetTransaction(txhash, tx, GetParams().GetConsensus(), hash_block))
        {
		    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
		    ssTx << tx;

		    uint8_t buf[8];
            UInt32SetLE(&buf[0], timestamp);
            UInt32SetLE(&buf[4], blockHeight);

            std::vector<uint8_t>ser_data;
            ser_data.assign(ssTx.begin(), ssTx.end());
            ser_data.insert(ser_data.end(), buf, buf+8);

            std::string stx = EncodeBase58((const uint8_t*)ser_data.data(), (const uint8_t*)(ser_data.data() + ser_data.size()));
            txs.push_back(stx);
        }
        else
        {
            // throw : error Transaction not found!!!!!!

        }
    }

    return txs;
}

UniValue stbtsgetlastusedhdindex(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgetlastusedhdindex\n"
            "\nReturns the last used index, the index of the last used address.\n"
            "\nReturns -1 if no address is used.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {lastindex:val}\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stbtsgetlastusedhdindex", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stbtsgetlastusedhdindex", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );

    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    if(xpubkey.size() < 4
       || xpubkey[1] != 'p' || xpubkey[2] != 'u' || xpubkey[3] != 'b')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    int r = GetLastUsedExternalSegWitIndex(xpubkey);

    if(r == -1)
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("lastindex", r);

    return obj;
}


UniValue stbtsgetfirstusedblock(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgetfirstusedblock\n"
            "\nReturns the last used index, the index of the last used address.\n"
            "\nReturns -1 if no address is used.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {firstusedblock:val}\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stbtsgetfirstusedblock", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stbtsgetfirstusedblock", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );

    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing!!!");
    }

    if(xpubkey.size() < 4
       || xpubkey[1] != 'p' || xpubkey[2] != 'u' || xpubkey[3] != 'b')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    int r = GetFirstUsedBlock(xpubkey);

    if(r == -2)
    {
        throw JSONRPCError(-2, "the xpub is invalid!!!");
    }

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("firstusedblock", r);

    return obj;
}

