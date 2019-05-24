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
    CKeyID seed_id; //!< seed hash160
    CKey seed;

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
        READWRITE(seed_id);
        READWRITE(use_bip44);
        if (this->nVersion >= VERSION_HD_CHAIN_SPLIT)
            READWRITE(nInternalChainCounter);
    }

    void SetNull()
    {
        nVersion = CHDChain::CURRENT_VERSION;
        nExternalChainCounter = 0;
        nInternalChainCounter = 0;
        seed_id.SetNull();
        use_bip44 = false;
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

#endif //RAVEN_HDCHAIN_H
