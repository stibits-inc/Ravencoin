#include <net.h>
#include <txdb.h>
#include <validation.h>
#include <chainparams.h>


void GenerateFromXPUB(std::string xpubkey, int from, int count, CDataStream& ss);  // defined in src/stib/common.cpp
uint32_t RecoverFromXPUB(std::string xpubkey, CDataStream& out); // defined in src/stib/common.cpp
void RecoverTxsFromXPUB(std::string xpubkey, unsigned int lastBlockHeight, std::vector<std::tuple<uint256, unsigned int, unsigned int>>& out);  // defined in src/stib/common.cpp

std::string ProcessStbts(CDataStream& vRecv)
{
    unsigned char cmd;

    BCLog::LogFlags logFlag = BCLog::NET;

    if(vRecv.size() == 0)
    {
        LogPrintf("Stbts Custom message Error.\n");
        return tinyformat::format(R"({"result":{"error":"Empty payload not autorized"}})");
    }
    vRecv.read((char*)&cmd, 1);


    switch(cmd)
    {
        case 'G' :
            {
                uint32_t from, count;

                if(vRecv.size() != 119)
                {
                    LogPrintf( "STBTS Custom message : G, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"G command size is 120 byte, not %d"}})", vRecv.size() );
                }

                vRecv.read((char*)&from, 4);
                vRecv.read((char*)&count, 4);

                std::string req = vRecv.str();
                LogPrint(logFlag, "STBTS Custom message : Gen from = %d, count = %d, k = %s\n", from, count, req.c_str());

                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                GenerateFromXPUB(req, (int)from, (int)count, ss);

                return ss.str();
                break;
            }

        case 'R' :
            {
                std::string req = vRecv.str();
                LogPrint(logFlag, "STBTS Custom message : Recover Utxos k = %s\n",  req.c_str());

                if(vRecv.size() != 111)
                {
                    LogPrintf( "STBTS Custom message : R, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"R command size is 111 byte, not %d"}})", vRecv.size() );
                }

                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                uint32_t count = RecoverFromXPUB(req, ss);

                CDataStream tmp(SER_NETWORK, PROTOCOL_VERSION);
                WriteCompactSize(tmp, count);
                ss.insert(ss.begin(), (const char*)tmp.data(), (const char*)tmp.data() + tmp.size());

                return ss.str();
                break;
            }

        case 'T' :
            {
                extern bool fTxIndex;
                if(!fTxIndex)
                {
                    LogPrintf("STBTS Custom message : T, Error, bitcoind is not started with -txindex option.\n");
                    return tinyformat::format(R"({"result":{"error":"bitcoind is not started with -txindex option"}})");
                }

                std::string req = vRecv.str();
                std::vector<std::tuple<uint256, unsigned int, unsigned int>> out;
                std::vector<std::string> outHex;
                RecoverTxsFromXPUB(req, 0, out);//TODO : set second parameter (lastBlockHeight) to the correct value

                CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
                WriteCompactSize(ssTx, out.size());

                if(out.size())
                    LogPrint(logFlag, "STBTS Custom message : T, %d, Transactions found.\n", out.size());

                for(auto [txhash, blockHeight, timestamp]: out)
                {
                    CTransactionRef tx;
                    uint256 hash_block;

                    if(GetTransaction(txhash, tx, GetParams().GetConsensus(), hash_block))
                    {
                        ssTx << *tx;
                    }
                    else
                    {
                        //  TODO throw exception
                    }
                }

                LogPrint(logFlag, "STBTS Custom message : Recover Txs k = %s\n",  req.c_str());

                return ssTx.str();
                break;
            }

            default:
                break;
    }

    LogPrint(logFlag, "STBTS Custom message, command id (%d) not found.\n", cmd);
    std::string msg = tinyformat::format(R"(Error: STBTS custom command, command id (%d) not found")", cmd);
    CDataStream tmp(SER_NETWORK, PROTOCOL_VERSION);
    tmp << msg;
    return tmp.str();;
}
