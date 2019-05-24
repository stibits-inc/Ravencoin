//
// Created by omari on 2019-05-23.
//

#ifndef RAVEN_HDCHAIN_H
#define RAVEN_HDCHAIN_H

#include "amount.h"
#include "primitives/transaction.h"
#include "wallet/db.h"
#include "wallet/hdchain.h"
#include "wallet/bip39.h"
#include "key.h"


/* simple HD chain data model */
class CHDChain
{
public:
    uint32_t nExternalChainCounter;
    uint32_t nInternalChainCounter;

    uint256 id; // TODO

    bool use_bip44;

    SecureVector vchSeed;
    SecureVector vchMnemonic;
    SecureVector vchMnemonicPassphrase;

    static const int VERSION_HD_BASE        = 1;
    static const int VERSION_HD_CHAIN_SPLIT = 2;
    static const int CURRENT_VERSION        = VERSION_HD_CHAIN_SPLIT;
    int nVersion;

    CHDChain() { SetNull(); }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(nExternalChainCounter);
        READWRITE(nInternalChainCounter);

        READWRITE(use_bip44);

        READWRITE(id);
        READWRITE(vchSeed);
        READWRITE(vchMnemonic);
        READWRITE(vchMnemonicPassphrase);
    }

    CHDChain(const CHDChain& other) :
            nVersion(other.nVersion),
            id(other.id),
            use_bip44(other.use_bip44),
            vchSeed(other.vchSeed),
            vchMnemonic(other.vchMnemonic),
            vchMnemonicPassphrase(other.vchMnemonicPassphrase)
    {}

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        nInternalChainCounter = 0;
        use_bip44 = true;

        id = uint256();
        vchSeed.clear();
        vchMnemonic.clear();
        vchMnemonicPassphrase.clear();
    }

    bool IsNull() const {return vchSeed.empty() || id == uint256();}

    void UseBip44( bool b = true)    { use_bip44 = b;}
    bool IsBip44()       const       { return use_bip44 == true;}

    void Debug(const std::string& strName) const{};


    bool SetMnemonic(const SecureVector& vchMnemonic, const SecureVector& vchMnemonicPassphrase, bool fUpdateID);
    bool SetMnemonic(const SecureString& ssMnemonic, const SecureString& ssMnemonicPassphrase, bool fUpdateID);
    bool GetMnemonic(SecureVector& vchMnemonicRet, SecureVector& vchMnemonicPassphraseRet) const;
    bool GetMnemonic(SecureString& ssMnemonicRet, SecureString& ssMnemonicPassphraseRet) const;

    bool SetSeed(const SecureVector& vchSeedIn, bool fUpdateID);
    SecureVector GetSeed() const;

    uint256 GetSeedHash();

    void DeriveChildExtKey(uint32_t nAccountIndex, bool fInternal, uint32_t& nChildIndex, CExtKey& extKeyRet);

};



/* hd pubkey data model */
class CHDPubKey
{
private:
    static const int CURRENT_VERSION = 1;
    int nVersion;

public:
    CExtPubKey extPubKey;
    uint256 hdchainID;
    uint32_t nAccountIndex;
    uint32_t nChangeIndex;

    CHDPubKey() : nVersion(CHDPubKey::CURRENT_VERSION), nAccountIndex(0), nChangeIndex(0) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(this->nVersion);
        READWRITE(extPubKey);
        READWRITE(hdchainID);
        READWRITE(nAccountIndex);
        READWRITE(nChangeIndex);
    }

    std::string GetKeyPath() const;
};

#endif //RAVEN_HDCHAIN_H
