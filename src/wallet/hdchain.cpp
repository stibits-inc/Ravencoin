//
// Created by omari on 2019-05-23.
//

#include "amount.h"
#include "primitives/transaction.h"
#include "wallet/db.h"
#include "wallet/hdchain.h"
#include "key.h"
#include "chainparams.h"


bool CHDChain::SetMnemonic(const SecureVector& vchMnemonic, const SecureVector& vchMnemonicPassphrase, bool fUpdateID)
{
    return SetMnemonic(SecureString(vchMnemonic.begin(), vchMnemonic.end()), SecureString(vchMnemonicPassphrase.begin(), vchMnemonicPassphrase.end()), fUpdateID);
}

bool CHDChain::SetMnemonic(const SecureString& ssMnemonic, const SecureString& ssMnemonicPassphrase, bool fUpdateID)
{
    SecureString ssMnemonicTmp = ssMnemonic;

    if (fUpdateID) {
        // can't (re)set mnemonic if seed was already set
        if (!IsNull())
            return false;

        // empty mnemonic i.e. "generate a new one"
        if (ssMnemonic.empty()) {
            ssMnemonicTmp = CMnemonic::Generate(use_bip44 ? 128 : 256);
        }
        // NOTE: default mnemonic passphrase is an empty string

        // printf("mnemonic: %s\n", ssMnemonicTmp.c_str());
        if (!CMnemonic::Check(ssMnemonicTmp)) {
            throw std::runtime_error(std::string(__func__) + ": invalid mnemonic: `" + std::string(ssMnemonicTmp.c_str()) + "`");
        }

        CMnemonic::ToSeed(ssMnemonicTmp, ssMnemonicPassphrase, vchSeed);
        //seed.Set(vchSeed.begin(), vchSeed.end());
        id = GetSeedHash();
    }

    vchMnemonic = SecureVector(ssMnemonicTmp.begin(), ssMnemonicTmp.end());
    vchMnemonicPassphrase = SecureVector(ssMnemonicPassphrase.begin(), ssMnemonicPassphrase.end());

    return !IsNull();
}

bool CHDChain::GetMnemonic(SecureVector& vchMnemonicRet, SecureVector& vchMnemonicPassphraseRet) const
{
    // mnemonic was not set, fail
    if (vchMnemonic.empty())
        return false;

    vchMnemonicRet = vchMnemonic;
    vchMnemonicPassphraseRet = vchMnemonicPassphrase;
    return true;
}

bool CHDChain::GetMnemonic(SecureString& ssMnemonicRet, SecureString& ssMnemonicPassphraseRet) const
{
    // mnemonic was not set, fail
    if (vchMnemonic.empty())
        return false;

    ssMnemonicRet = SecureString(vchMnemonic.begin(), vchMnemonic.end());
    ssMnemonicPassphraseRet = SecureString(vchMnemonicPassphrase.begin(), vchMnemonicPassphrase.end());

    return true;
}

bool CHDChain::SetSeed(const SecureVector& vchSeedIn, bool fUpdateID)
{
    vchSeed = vchSeedIn;

    if (fUpdateID) {
        id = GetSeedHash();
    }

    return !IsNull();
}

SecureVector CHDChain::GetSeed() const
{
    return vchSeed;
}

uint256 CHDChain::GetSeedHash()
{
    return Hash(vchSeed.begin(), vchSeed.end());
}

void CHDChain::DeriveChildExtKey(uint32_t nAccountIndex, bool fInternal, uint32_t nChildIndex, CExtKey& extKeyRet)
{
    CExtKey masterKey;              //hd master key

    CExtKey purposeKey;             //key at m/purpose'
    CExtKey coinTypeKey;            //key at m/purpose'/coin_type'

    CExtKey accountKey;             //key at m/[purpose'/coin_type'/]account'
    CExtKey changeKey;              //key at m/[purpose'/coin_type'/]account'/change

    masterKey.SetSeed(vchSeed.data(), vchSeed.size());

    // Use hardened derivation for purpose, coin_type and account
    // (keys >= 0x80000000 are hardened after bip32)

    if(use_bip44)
    {
        // Use BIP44 keypath scheme i.e. m / purpose' / coin_type' / account' / change / address_index

        // derive m/purpose'
        masterKey.Derive(purposeKey, 44 | 0x80000000);
        // derive m/purpose'/coin_type'
        purposeKey.Derive(coinTypeKey, Params().ExtCoinType() | 0x80000000);
        // derive m/purpose'/coin_type'/account'
        coinTypeKey.Derive(accountKey, nAccountIndex | 0x80000000);
        // derive m/purpose'/coin_type'/account'/change
        accountKey.Derive(changeKey, fInternal ? 1 : 0);
        // derive m/purpose'/coin_type'/account'/change/address_index
        changeKey.Derive(extKeyRet, nChildIndex);
    }
    else
    {
        // Use BIP32 keypath scheme i.e. m / account' / change / address_index

        // derive m/account'
        masterKey.Derive(accountKey, nAccountIndex | 0x80000000);
        // derive m/account'/change
        accountKey.Derive(changeKey, fInternal ? 1 : 0);
        // derive m/account'/change/address_index
        changeKey.Derive(extKeyRet, nChildIndex);
    }
}
