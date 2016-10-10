// Copyright (c) 2013-2016 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php

#ifndef NOVACOIN_TRANSACTION_H
#define NOVACOIN_TRANSACTION_H


#include "timestamps.h"
#include "uint256.h"
#include "serialize.h"
#include "script.h"
#include "scrypt.h"

class CBlock;
class CBlockIndex;
class CTxDB;
class CTxIndex;

//
// Global state
//

static const unsigned int MAX_BLOCK_SIZE = 1000000;
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
static const unsigned int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
static const unsigned int MAX_ORPHAN_TRANSACTIONS = MAX_BLOCK_SIZE/100;
static const unsigned int MAX_INV_SZ = 50000;

extern int nBestHeight;
extern std::map<uint256, CBlockIndex*> mapBlockIndex;

static const uint256 hashGenesisBlock("0x00000a060336cbb72fe969666d337b87198b1add2abaa59cca226820b32933a4");
static const uint256 hashGenesisBlockTestNet("0x000c763e402f2436da9ed36c7286f62c3f6e5dbafce9ff289bd43d7459327eb");

extern uint256 nBestChainTrust;
extern CBlockIndex* pindexGenesisBlock;

//settings
extern const uint256 entropyStore[38];

inline int64_t FutureDrift(int64_t nTime) { return nTime + 2 * nOneHour; } // up to 2 hours from the future

bool IsInitialBlockDownload();

typedef std::map<uint256, std::pair<CTxIndex, CTransaction> > MapPrevTx;

// An outpoint - a combination of a transaction hash and an index n into its vout
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint();
    COutPoint(uint256 hashIn, uint32_t nIn);
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull();
    bool IsNull() const;

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
    void print() const;

};


// An input of a transaction.  It contains the location of the previous
// transaction's output that it claims and a signature that matches the
// output's public key.
//
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;

    CTxIn();
    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max());
    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=std::numeric_limits<unsigned int>::max());

    IMPLEMENT_SERIALIZE
    (
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    )

    bool IsFinal() const;

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const;
    std::string ToString() const;
    void print() const;

};


// An output of a transaction.  It contains the public key that the next input
// must be able to sign with to claim it.
//
class CTxOut
{
public:
    int64_t nValue;
    CScript scriptPubKey;

    CTxOut();
    CTxOut(int64_t nValueIn, CScript scriptPubKeyIn);

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )

    void SetNull();
    bool IsNull();
    void SetEmpty();
    bool IsEmpty() const;
    uint256 GetHash() const;

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToStringShort() const;
    std::string ToString() const;
    void print() const;

};


// Position on disk for a particular transaction.
class CDiskTxPos
{
public:
    uint32_t nFile;
    uint32_t nBlockPos;
    uint32_t nTxPos;

    CDiskTxPos();
    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn);

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )

    void SetNull();
    bool IsNull() const;

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
    void print() const;

};




// Nodes collect new transactions into a block, hash them into a hash tree,
// and scan through nonce values to make the block's hash satisfy proof-of-work
// requirements.  When they solve the proof-of-work, they broadcast the block
// to everyone and the block is added to the block chain.  The first transaction
// in the block is a special one that creates a new coin owned by the creator
// of the block.
//
// Blocks are appended to blk0001.dat files on disk.  Their location on disk
// is indexed by CBlockIndex objects in memory.
//
class CBlock
{
public:
    // header
    static const int CURRENT_VERSION=6;
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable std::vector<uint256> vMerkleTree;

    // Denial-of-service detection:
    mutable int nDoS;
    bool DoS(int nDoSIn, bool fIn) const;

    CBlock();

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
        {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        }
        else if (fRead)
        {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        }
    )

    void SetNull();
    bool IsNull() const;
    uint256 GetHash() const;
    int64_t GetBlockTime() const;
    void UpdateTime(const CBlockIndex* pindexPrev);

    // ppcoin: entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit(unsigned int nHeight) const;

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const;
    bool IsProofOfWork() const;
    std::pair<COutPoint, unsigned int> GetProofOfStake() const;

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const;
    uint256 BuildMerkleTree() const;
    std::vector<uint256> GetMerkleBranch(int nIndex) const;
    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex);
    bool WriteToDisk(unsigned int& nFileRet, unsigned int& nBlockPosRet);
    bool ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions=true);
    void print() const;
    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck=false);
    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions=true);
    bool SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew);
    bool AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos);
    bool CheckBlock(bool fCheckPOW=true, bool fCheckMerkleRoot=true, bool fCheckSig=true) const;
    bool AcceptBlock();
    bool GetCoinAge(uint64_t& nCoinAge) const; // ppcoin: calculate total coin age spent in block
    bool CheckBlockSignature() const;

private:
    bool SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew);
};



//  A txdb record that contains the disk location of a transaction and the
// locations of transactions that spend its outputs.  vSpent is really only
// used as a flag, but having the location is very helpful for debugging.
//
class CTxIndex
{
public:
    CDiskTxPos pos;
    std::vector<CDiskTxPos> vSpent;

    CTxIndex();
    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs);

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull();
    bool IsNull();

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        return (a.pos    == b.pos &&
                a.vSpent == b.vSpent);
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }

    int GetDepthInMainChain() const;

};

#endif // NOVACOIN_TRANSACTION_H