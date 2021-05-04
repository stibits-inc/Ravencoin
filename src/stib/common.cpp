#include <key.h>
#include <base58.h>
#include <script/standard.h>
#include <script/script.h>
#include <utilstrencodings.h>
#include <rpc/server.h>
#include <util.h>
#include <addressindex.h>
#include <streams.h>
#include <validation.h>

struct HD_XPub
{
    HD_XPub(const std::string xpub) {SetXPub(xpub);}
    HD_XPub()   {}


    void                        SetXPub         (const std::string xpub_);

    std::vector<std::string>    Derive          (uint32_t from, uint32_t count, bool internal = false);
    std::vector<std::string>    DeriveWitness   (uint32_t from, uint32_t count, bool internal = false);

    std::vector<std::string>    Derive          (uint32_t from, uint32_t count, bool internal, bool segwit)
    {
        return
            segwit ?
                DeriveWitness( from, count, internal)
            :
                Derive       ( from, count, internal);
    }

    bool IsValid() {return accountKey.pubkey.IsValid();}

private:
    CExtPubKey accountKey;
};

struct TransactionsCompare {
    bool operator()(const std::tuple<int, unsigned int, unsigned int> &lhs, const std::tuple<int, unsigned int, unsigned int> &rhs) const {
        bool b = false;
        auto [lhsIndex, lhsBlockHeight, lhsTimestamp] = lhs;
        auto [rhsIndex, rhsBlockHeight, rhsTimestamp] = rhs;

        if(lhsBlockHeight < rhsBlockHeight) b = true;
        if(lhsBlockHeight == rhsBlockHeight && lhsIndex < rhsIndex) b = true;

        return b;
    }
};

static bool DecodeBase58Check_(const char* psz, std::vector<unsigned char>& vchRet)
{
    if (!DecodeBase58(psz, vchRet) ||
        (vchRet.size() < 4)) {
        vchRet.clear();
        return false;
    }
    // re-calculate the checksum, ensure it matches the included 4-byte checksum
    uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
    if (memcmp(&hash, &vchRet[vchRet.size() - 4], 4) != 0) {
        vchRet.clear();
        return false;
    }
    vchRet.resize(vchRet.size() - 4);
    return true;
}

void HD_XPub::SetXPub(const std::string xpub)
{
    std::vector<unsigned char> ve ;
    if(DecodeBase58Check_(xpub.c_str(), ve))
    {
        ve.erase(ve.begin(), ve.begin() + 4);
        accountKey.Decode(ve.data());
    }
}

static std::string GetAddress(CPubKey& key)
{
    CKeyID id = key.GetID();
    CTxDestination d = id;
    return EncodeDestination(d);
}

/*
static std::string GetBech32Address(CPubKey& key)
{
    CKeyID id = key.GetID();

    std::vector<unsigned char> data = {0};
    data.reserve(33);
    ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
    return bech32::Encode(Params().Bech32HRP(), data);
}
*/

std::vector<std::string> HD_XPub::Derive(uint32_t from, uint32_t count, bool internal)
{
    std::vector<std::string> ret(count);

    CExtPubKey chainKey;
    CExtPubKey childKey;

    // derive M/change
    accountKey.Derive(chainKey, internal ? 1 : 0);

    for(uint32_t i = 0; i < count; i++)
    {
        // derive M/change/index
        chainKey.Derive(childKey, from );
        std::string addr = GetAddress(childKey.pubkey );

        from++;
        ret[i] = addr;
    }

    return ret;
}

std::vector<std::string> HD_XPub::DeriveWitness(uint32_t from, uint32_t count, bool internal)
{
    std::vector<std::string> ret(count);

    CExtPubKey chainKey;
    CExtPubKey childKey;

    // derive M/change
    accountKey.Derive(chainKey, internal ? 1 : 0);

    for(uint32_t i = 0; i < count; i++)
    {
        // derive M/change/index
        chainKey.Derive(childKey, from );
        std::string addr = GetAddress(childKey.pubkey );

        from++;
        ret[i] = addr;
    }

    return ret;
}

static UniValue& operator <<(UniValue& arr, const UniValue& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i]);
    }

    return arr;
}

static std::vector<std::string>& operator <<(std::vector<std::string>& arr, const UniValue& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i].write());
    }

    return arr;
}

static std::vector<uint256>& operator <<(std::vector<uint256>& arr, const std::vector<uint256>& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i]);
    }

    return arr;
}

bool AddressToHashType(std::string addr_str, uint160& hashBytes, int& type)
{
    CRavenAddress address(addr_str);
    if (!address.GetIndexKey(hashBytes, type)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    return true;
}


bool HashTypeToAddress(const uint160 &hash, const int &type, std::string &address)
{
    if (type == 2) {
        address = CRavenAddress(CScriptID(hash)).ToString();
    } else if (type == 1) {
        address = CRavenAddress(CKeyID(hash)).ToString();
    } else {
        return false;
    }
    return true;
}

static bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}


int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses)
{
    int r = -1;
    int index = 0;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, txOutputs)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
        if(txOutputs.size() > 0)
        {
            r = index;
            txOutputs.clear();
        }


        index++;
    }

    return r;
}

bool GetAddressesTxs(std::map<const std::tuple<int, unsigned int, unsigned int>, uint256, TransactionsCompare> *TxsMap, std::vector<std::pair<uint160, int>> &addresses) {

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }
    }
    if(addressIndex.size() == 0)
        return false;


    for (auto it = addressIndex.begin(); it != addressIndex.end(); it++) {

        CBlockIndex* pblockindex = chainActive[it->first.blockHeight];

        TxsMap->insert({std::make_tuple(it->first.txindex, it->first.blockHeight, pblockindex->nTime) , it->first.txhash});
        printf("MEHDIEG : %d\t%d\n", it->first.blockHeight, pblockindex->nTime);
    }

    return true;
}


int  GetFirstBlockHeightForAddresses(std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto& a: addresses) {
        if (!GetAddressIndex(a.first, a.second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }

    if (addressIndex.size() == 0 ) return -1;

    int blockHeight = addressIndex[0].first.blockHeight;

    for (auto& a: addressIndex) {
        int height = a.first.blockHeight;
		if (height < blockHeight)
            blockHeight = height;
    }

    return blockHeight;
}

bool IsAddressesHasTxs(std::vector<std::pair<uint160, int>> &addresses)
{

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }

        if(addressIndex.size() > 0) return true;
    }

    return addressIndex.size() > 0;

}

bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs);

UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);

        std::string address;

        if (!HashTypeToAddress(it->first.hashBytes, it->first.type, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.pushKV("address", address);
        output.pushKV("txid", it->first.txhash.GetHex());
        output.pushKV("outputIndex", (int)it->first.index);
        output.pushKV("script", HexStr(it->second.script.begin(), it->second.script.end()));
        output.pushKV("satoshis", it->second.satoshis);
        output.pushKV("height", it->second.blockHeight);
        utxos.push_back(output);
    }

    return utxos;
}


bool GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses, CDataStream& ss, uint32_t& count)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    count += unspentOutputs.size();

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {

        std::string address;

        if (!HashTypeToAddress(it->first.hashBytes, it->first.type, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        ss << address;

        it->first.txhash.Serialize(ss);
        ser_writedata32(ss, it->first.index);

        ss << it->second;
    }

    return unspentOutputs.size() > 0;
}

#define BLOCK_SIZE 100

int GetLastUsedExternalSegWitIndex(std::string xpub)
{
     int ret = -1;
     uint32_t last =  0;
     HD_XPub hd(xpub);

     if(!hd.IsValid())
     {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return ret;
     }

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, false, true);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
             LogPrintf("%s\n", a.data());
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         int r = GetLastUsedIndex(addresses);

         if(r < 0) return ret+1;
         ret = last + r;

         last += BLOCK_SIZE;

     } while(true);

     return ret;
}

// return 0 when no block found
// if found, return the blocknumber
int GetFirstUsedBlock(std::string xpub)
{
     int ret = -1;
     uint32_t last =  0;
     HD_XPub hd(xpub);

     if(!hd.IsValid())
     {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return -2;
     }

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, false, true);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
             LogPrintf("%s\n", a.data());
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         int r = GetFirstBlockHeightForAddresses(addresses);

         if(r == -1) return ret;

         if(ret == -1 || r < ret) ret = r;

         last += BLOCK_SIZE;

     } while(true);

     return ret;
}

UniValue Recover_(HD_XPub& hd, bool internal, bool segwit)
{
    /*
     * repeat
     *    derive 100 next address
     *    get their utxos
     *    if no utxo found
     *       get their txs
     * while there is at least ( one utxo or one tx)
     *
     */

     UniValue ret(UniValue::VARR);

     uint32_t last =  0;

     int not_found = 0;

     bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         UniValue utxos = GetAddressesUtxos(addresses);

         if(utxos.size() == 0)
         {
             found = IsAddressesHasTxs(addresses);

         }
         else
         {
             ret << utxos;
             found = true;
         }


         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;


     } while(not_found < 100);

     return ret;
}


uint32_t Recover_(HD_XPub& hd, bool internal, bool segwit, CDataStream& ss)
{
    /*
     * repeat
     *    derive 100 next address
     *    get their utxos
     *    if no utxo found
     *       get their txs
     * while there is at least ( one utxo or one tx)
     *
     */

     uint32_t last =  0;

     int not_found = 0;

     uint32_t count = 0;

     bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         if(!GetAddressesUtxos(addresses, ss, count))
         {
             found = IsAddressesHasTxs(addresses);

         }
         else
         {
             found = true;
         }

         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;

     } while(not_found < 100);

     return count;
}

void RecoverTxs_(std::map<const std::tuple<int, unsigned int, unsigned int>, uint256, TransactionsCompare> *TxsMap, HD_XPub& hd, bool internal, bool segwit)
{
    /*
        * repeat
        *    derive 100 next address
        *    gget their txs
        * while there is at least ( one tx)
        *
    */
    uint32_t last =  0;

    int not_found = 0;
    bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         found = GetAddressesTxs(TxsMap, addresses);

         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;

     } while(not_found < 100);
}


void GenerateFromXPUB(std::string xpubkey, int from, int count, UniValue& out)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }

    std::vector<std::string> v = xpub.Derive(from, count, false, false);

    for(auto addr : v)
    {
        out.push_back(addr);
    }
}

void GenerateFromXPUB(std::string xpubkey, int from, int count, std::vector<std::string>& out)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }
    std::vector<std::string> v = xpub.Derive(from, count, false, false);

    for(auto addr : v)
    {
        out.push_back(addr);
    }
}

void GenerateFromXPUB(std::string xpubkey, int from, int count, CDataStream& ss)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }

    std::vector<std::string> v = xpub.Derive(from, count, false, false);

    ss << v;
}

void RecoverFromXPUB(std::string xpubkey, UniValue& out)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }

    out   // << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          // << Recover_(xpub, true, true)
          ;
}

void RecoverFromXPUB(std::string xpubkey, std::vector<std::string>& out)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }

    out   // << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          // << Recover_(xpub, true, true)
          ;
}

uint32_t RecoverFromXPUB(std::string xpubkey, CDataStream& ss)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return 0;
    }

    uint32_t count =  // << Recover_(xpub, false, true)
            Recover_(xpub, false, false, ss)
          + Recover_(xpub, true, false, ss)
          // << Recover_(xpub, true, true)
          ;
    return count;
}


void RecoverTxsFromXPUB(std::string xpubkey, std::vector<std::tuple<uint256, unsigned int, unsigned int>>& out)
{
    HD_XPub xpub(xpubkey);

    if(!xpub.IsValid())
    {
        LogPrintf("provided xpub is not valid !!!!!\n");
        return;
    }

    std::map<const std::tuple<int, unsigned int, unsigned int>, uint256, TransactionsCompare> TxsMap;

    //RecoverTxs_(xpub, true, true);//witness
    //RecoverTxs_(xpub, false, true);//witness

    RecoverTxs_(&TxsMap, xpub, false, false);
    RecoverTxs_(&TxsMap, xpub, true, false);


    for(auto it = TxsMap.begin(); it != TxsMap.end(); ++it ) {
        auto [index, blockHeight, timestamp] = it->first;
        out.push_back(std::make_tuple(it->second, blockHeight, timestamp));
        //LogPrintf("Txsid : %d, %d, %s\n", blockHeight, timestamp, it->second.ToString());
    }
}
