// Copyright (c) 2013-2016 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php

#include "transaction.h"
#include "util.h"
#include "checkpoints.h"
#include "ui_interface.h"

#include <boost/algorithm/string/replace.hpp>

using namespace std;

extern enum Checkpoints::CPMode CheckpointsMode;

//
// COutPoint
//

COutPoint::COutPoint()
{
    SetNull();
}

COutPoint::COutPoint(uint256 hashIn, uint32_t nIn) : hash(hashIn), n(nIn)
{
}

void COutPoint::SetNull()
{
    hash = 0;
    n = numeric_limits<uint32_t>::max();
}

bool COutPoint::IsNull() const
{
    return (hash == 0 && n == numeric_limits<uint32_t>::max());
}

string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %" PRIu32 ")", hash.ToString().substr(0,10).c_str(), n);
}

void COutPoint::print() const
{
    printf("%s\n", ToString().c_str());
}


//
// CTxIn
//

CTxIn::CTxIn() : nSequence(numeric_limits<unsigned int>::max())
{
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, unsigned int nSequenceIn)
        : prevout(prevoutIn), scriptSig(scriptSigIn), nSequence(nSequenceIn)
{
}

CTxIn::CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn, unsigned int nSequenceIn)
        : prevout(COutPoint(hashPrevTx, nOut)), scriptSig(scriptSigIn), nSequence(nSequenceIn)
{
}

bool CTxIn::IsFinal() const
{
    return (nSequence == numeric_limits<unsigned int>::max());
}

string CTxIn::ToStringShort() const
{
    return strprintf(" %s %d", prevout.hash.ToString().c_str(), prevout.n);
}

string CTxIn::ToString() const
{
    string str;
    str += "CTxIn(";
    str += CTxIn::prevout.ToString();
    if (CTxIn::prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(CTxIn::scriptSig).c_str());
    else
        str += strprintf(", scriptSig=%s", CTxIn::scriptSig.ToString().substr(0,24).c_str());
    if (CTxIn::nSequence != numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%" PRIu32, CTxIn::nSequence);
    str += ")";
    return str;
}

void CTxIn::print() const
{
    printf("%s\n", ToString().c_str());
}



//
// CTxOut
//

CTxOut::CTxOut()
{
    SetNull();
}

CTxOut::CTxOut(int64_t nValueIn, CScript scriptPubKeyIn) : nValue(nValueIn), scriptPubKey(scriptPubKeyIn)
{
}

void CTxOut::SetNull()
{
    nValue = -1;
    scriptPubKey.clear();
}

bool CTxOut::IsNull()
{
    return (nValue == -1);
}

void CTxOut::SetEmpty()
{
    nValue = 0;
    scriptPubKey.clear();
}

bool CTxOut::IsEmpty() const
{
    return (nValue == 0 && scriptPubKey.empty());
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

string CTxOut::ToStringShort() const
{
    return strprintf(" out %s %s", FormatMoney(nValue).c_str(), scriptPubKey.ToString(true).c_str());
}

string CTxOut::ToString() const
{
    if (IsEmpty()) return "CTxOut(empty)";
    if (scriptPubKey.size() < 6)
        return "CTxOut(error)";
    return strprintf("CTxOut(nValue=%s, scriptPubKey=%s)", FormatMoney(nValue).c_str(), scriptPubKey.ToString().c_str());
}

void CTxOut::print() const
{
    printf("%s\n", ToString().c_str());
}


//
// CDiskTxPos
//

CDiskTxPos::CDiskTxPos()
{
    SetNull();
}

CDiskTxPos::CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn) : nFile(nFileIn), nBlockPos(nBlockPosIn), nTxPos(nTxPosIn)
{
}

void CDiskTxPos::SetNull()
{
    nFile = numeric_limits<uint32_t>::max();
    nBlockPos = 0;
    nTxPos = 0;
}

bool CDiskTxPos::IsNull() const
{
    return (nFile == numeric_limits<uint32_t>::max());
}

string CDiskTxPos::ToString() const
{
    if (IsNull())
        return "null";
    else
        return strprintf("(nFile=%" PRIu32 ", nBlockPos=%" PRIu32 ", nTxPos=%" PRIu32 ")", nFile, nBlockPos, nTxPos);
}

void CDiskTxPos::print() const
{
    printf("%s", ToString().c_str());
}


//
// CTxIndex
//

CTxIndex::CTxIndex()
{
    SetNull();
}

CTxIndex::CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs) : pos(posIn)
{
    vSpent.resize(nOutputs);
}

void CTxIndex::SetNull()
{
    pos.SetNull();
    vSpent.clear();
}

bool CTxIndex::IsNull()
{
    return pos.IsNull();
}

int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    auto mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    auto pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}


//
// CBlock
//

bool CBlock::DoS(int nDoSIn, bool fIn) const
{
    nDoS += nDoSIn;
    return fIn;
}

CBlock::CBlock()
{
    SetNull();
}

void CBlock::SetNull()
{
    nVersion = CBlock::CURRENT_VERSION;
    hashPrevBlock = 0;
    hashMerkleRoot = 0;
    nTime = 0;
    nBits = 0;
    nNonce = 0;
    vtx.clear();
    vchBlockSig.clear();
    vMerkleTree.clear();
    nDoS = 0;
}

bool CBlock::IsNull() const
{
    return (nBits == 0);
}

uint256 CBlock::GetHash() const
{
    return scrypt_blockhash((const uint8_t*)&nVersion);
}

int64_t CBlock::GetBlockTime() const
{
    return (int64_t)nTime;
}

void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = max(GetBlockTime(), GetAdjustedTime());
}

unsigned int CBlock::GetStakeEntropyBit(unsigned int nHeight) const
{
    // Protocol switch to support p2pool at novacoin block #9689
    if (nHeight >= 9689 || fTestNet)
    {
        // Take last bit of block hash as entropy bit
        auto nEntropyBit = (GetHash().Get32()) & (uint32_t)1;
        if (fDebug && GetBoolArg("-printstakemodifier"))
            printf("GetStakeEntropyBit: nTime=%" PRIu32 " hashBlock=%s nEntropyBit=%" PRIu32 "\n", nTime, GetHash().ToString().c_str(), nEntropyBit);
        return nEntropyBit;
    }

    // Before novacoin block #9689 - get from pregenerated table
    int nBitNum = nHeight & 0xFF;
    int nItemNum = nHeight / 0xFF;

    auto nEntropyBit = ((entropyStore[nItemNum] & (uint256(1) << nBitNum)) >> nBitNum).Get32();
    if (fDebug && GetBoolArg("-printstakemodifier"))
        printf("GetStakeEntropyBit: from pregenerated table, nHeight=%" PRIu32 " nEntropyBit=%" PRIu32 "\n", nHeight, nEntropyBit);
    return nEntropyBit;
}

bool CBlock::IsProofOfStake() const
{
    return (vtx.size() > 1 && vtx[1].IsCoinStake());
}

bool CBlock::IsProofOfWork() const
{
    return !IsProofOfStake();
}

pair<COutPoint, unsigned int> CBlock::GetProofOfStake() const
{
    if (IsProofOfStake())
        return { vtx[1].vin[0].prevout, vtx[1].nTime };
    return { COutPoint(), 0u };
}

int64_t CBlock::GetMaxTransactionTime() const
{
    int64_t maxTransactionTime = 0;
    for(const auto& tx : vtx)
        maxTransactionTime = max(maxTransactionTime, (int64_t)tx.nTime);
    return maxTransactionTime;
}

uint256 CBlock::BuildMerkleTree() const
{
    vMerkleTree.clear();
    for(const auto& tx :  vtx)
        vMerkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int nSize = (int)vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = min(i+1, nSize-1);
            vMerkleTree.push_back(Hash(vMerkleTree[j+i].begin(),  vMerkleTree[j+i].end(),
                                       vMerkleTree[j+i2].begin(), vMerkleTree[j+i2].end()));
        }
        j += nSize;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = (int)vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    for(const uint256& otherside :  vMerkleBranch)
    {
        if (nIndex & 1)
            hash = Hash(otherside.begin(), otherside.end(), hash.begin(), hash.end());
        else
            hash = Hash(hash.begin(), hash.end(), otherside.begin(), otherside.end());
        nIndex >>= 1;
    }
    return hash;
}

bool CBlock::WriteToDisk(unsigned int& nFileRet, unsigned int& nBlockPosRet)
{
    // Open history file to append
    auto fileout = CAutoFile(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("CBlock::WriteToDisk() : AppendBlockFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << nNetworkID << nSize;

    // Write block
    long fileOutPos = ftell(fileout);
    if (fileOutPos < 0)
        return error("CBlock::WriteToDisk() : ftell failed");
    nBlockPosRet = fileOutPos;
    fileout << *this;

    // Flush stdio buffers and commit to disk before returning
    fflush(fileout);
    if (!IsInitialBlockDownload() || (nBestHeight+1) % 500 == 0)
        FileCommit(fileout);

    return true;
}

bool CBlock::ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions)
{
    SetNull();

    // Open history file to read
    auto filein = CAutoFile(OpenBlockFile(nFile, nBlockPos, "rb"), SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("CBlock::ReadFromDisk() : OpenBlockFile failed");
    if (!fReadTransactions)
        filein.nType |= SER_BLOCKHEADERONLY;

    // Read block
    try {
        filein >> *this;
    }
    catch (const std::exception&) {
        return error("%s() : deserialize or I/O error", BOOST_CURRENT_FUNCTION);
    }

    // Check the header
    if (fReadTransactions && IsProofOfWork() && !CheckProofOfWork(GetHash(), nBits))
        return error("CBlock::ReadFromDisk() : errors in block header");

    return true;
}

void CBlock::print() const
{
    printf("CBlock(hash=%s, ver=%" PRId32 ", hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%" PRIu32 ", nBits=%08x, nNonce=%" PRIu32 ", vtx=%" PRIszu ", vchBlockSig=%s)\n",
        GetHash().ToString().c_str(),
        nVersion,
        hashPrevBlock.ToString().c_str(),
        hashMerkleRoot.ToString().c_str(),
        nTime, nBits, nNonce,
        vtx.size(),
        HexStr(vchBlockSig.begin(), vchBlockSig.end()).c_str());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        printf("  ");
        vtx[i].print();
    }
    printf("  vMerkleTree: ");
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        printf("%s ", vMerkleTree[i].ToString().substr(0,10).c_str());
    printf("\n");
}

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for(CTransaction& tx :  vtx)
        SyncWithWallets(tx, this, false, false);

    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(!fJustCheck, !fJustCheck, false))
        return false;

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.
    bool fEnforceBIP30 = true; // Always active in NovaCoin
    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate();

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    else
        nTxPos = pindex->nBlockPos + ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - (2 * GetSizeOfCompactSize(0)) + GetSizeOfCompactSize(vtx.size());

    map<uint256, CTxIndex> mapQueuedChanges;
    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    unsigned int nSigOps = 0;
    for(auto& tx :  vtx)
    {
        auto hashTx = tx.GetHash();

        if (fEnforceBIP30) {
            CTxIndex txindexOld;
            if (txdb.ReadTxIndex(hashTx, txindexOld)) {
                for(CDiskTxPos &pos :  txindexOld.vSpent)
                    if (pos.IsNull())
                        return false;
            }
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else
        {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS)
                return DoS(100, error("ConnectBlock() : too many sigops"));

            auto nTxValueIn = tx.GetValueIn(mapInputs);
            auto nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;

            unsigned int nFlags = SCRIPT_VERIFY_NOCACHE | SCRIPT_VERIFY_P2SH;

            if (tx.nTime >= CHECKLOCKTIMEVERIFY_SWITCH_TIME) {
                nFlags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
                // OP_CHECKSEQUENCEVERIFY is senseless without BIP68, so we're going disable it for now.
                // nFlags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
            }

            std::vector<CScriptCheck> vChecks;
            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false, fScriptChecks, nFlags, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }

    if (!control.Wait())
        return DoS(100, false);

    if (IsProofOfWork())
    {
        auto nBlockReward = GetProofOfWorkReward(nBits, nFees);

        // Check coinbase reward
        if (vtx[0].GetValueOut() > nBlockReward)
            return error("CheckBlock() : coinbase reward exceeded (actual=%" PRId64 " vs calculated=%" PRId64 ")",
                   vtx[0].GetValueOut(),
                   nBlockReward);
    }

    // track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    // fees are not collected by proof-of-stake miners
    // fees are destroyed to compensate the entire network
    if (fDebug && IsProofOfStake() && GetBoolArg("-printcreation"))
        printf("ConnectBlock() : destroy=%s nFees=%" PRId64 "\n", FormatMoney(nFees).c_str(), nFees);

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (auto mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
    {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
    for(CTransaction& tx :  vtx)
        SyncWithWallets(tx, this, true);

    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    auto hash = GetHash();

    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet))
    {
        txdb.WriteHashBestChain(hash);
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (hashPrevBlock == hashBestChain)
    {
        if (!SetBestChainInner(txdb, pindexNew))
            return error("SetBestChain() : SetBestChainInner failed");
    }
    else
    {
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndex *pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndex*> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev && pindexIntermediate->pprev->nChainTrust > pindexBest->nChainTrust)
        {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
            printf("Postponing %" PRIszu " reconnects\n", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate))
        {
            txdb.TxnAbort();
            InvalidChainFound(pindexNew);
            return error("SetBestChain() : Reorganize failed");
        }

        // Connect further blocks
        for (std::vector<CBlockIndex*>::reverse_iterator rit = vpindexSecondary.rbegin(); rit != vpindexSecondary.rend(); ++rit)
        {
            CBlock block;
            if (!block.ReadFromDisk(*rit))
            {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (!txdb.TxnBegin()) {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, *rit))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload)
    {
        const CBlockLocator locator(pindexNew);
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    pblockindexFBBHLast = NULL;
    nBestHeight = pindexBest->nHeight;
    nBestChainTrust = pindexNew->nChainTrust;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;

    uint256 nBestBlockTrust = pindexBest->nHeight != 0 ? (pindexBest->nChainTrust - pindexBest->pprev->nChainTrust) : pindexBest->nChainTrust;

    printf("SetBestChain: new best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
      hashBestChain.ToString().substr(0,20).c_str(), nBestHeight,
      CBigNum(nBestChainTrust).ToString().c_str(),
      nBestBlockTrust.Get64(),
      DateTimeStrFormat("%x %H:%M:%S", pindexBest->GetBlockTime()).c_str());

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload)
    {
        int nUpgraded = 0;
        const CBlockIndex* pindex = pindexBest;
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded, CBlock::CURRENT_VERSION);
        if (nUpgraded > 100/2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    auto strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos)
{
    // Check for duplicate
    auto hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,20).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new(nothrow) CBlockIndex(nFile, nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock = &hash;
    auto miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->nChainTrust = (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit(pindexNew->nHeight)))
        return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // ppcoin: record proof-of-stake hash value
    if (pindexNew->IsProofOfStake())
    {
        if (!mapProofOfStake.count(hash))
            return error("AddToBlockIndex() : hashProofOfStake not found in map");
        pindexNew->hashProofOfStake = mapProofOfStake[hash];
    }

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pindexNew, nStakeModifier, fGeneratedStakeModifier))
        return error("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew);
    if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
        return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d, modifier=0x%016" PRIx64, pindexNew->nHeight, nStakeModifier);

    // Add to mapBlockIndex
    auto mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));
    if (!txdb.TxnCommit())
        return false;

    // New best
    if (pindexNew->nChainTrust > nBestChainTrust)
        if (!SetBestChain(txdb, pindexNew))
            return false;

    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    static int8_t counter = 0;
    if( (++counter & 0x0F) == 0 || !IsInitialBlockDownload()) // repaint every 16 blocks if not in initial block download
        uiInterface.NotifyBlocksChanged();
    return true;
}

bool CBlock::CheckBlock(bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    set<uint256> uniqueTx; // tx hashes
    unsigned int nSigOps = 0; // total sigops

    // Size limits
    if (vtx.empty() || vtx.size() > MAX_BLOCK_SIZE || ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return DoS(100, error("CheckBlock() : size limits failed"));

    bool fProofOfStake = IsProofOfStake();

    // First transaction must be coinbase, the rest must not be
    if (!vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock() : first tx is not coinbase"));

    if (!vtx[0].CheckTransaction())
        return DoS(vtx[0].nDoS, error("CheckBlock() : CheckTransaction failed on coinbase"));

    uniqueTx.insert(vtx[0].GetHash());
    nSigOps += vtx[0].GetLegacySigOpCount();

    if (fProofOfStake)
    {
        // Proof-of-STake related checkings. Note that we know here that 1st transactions is coinstake. We don't need 
        //   check the type of 1st transaction because it's performed earlier by IsProofOfStake()

        // nNonce must be zero for proof-of-stake blocks
        if (nNonce != 0)
            return DoS(100, error("CheckBlock() : non-zero nonce in proof-of-stake block"));

        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100, error("CheckBlock() : coinbase output not empty for proof-of-stake block"));

        // Check coinstake timestamp
        if (GetBlockTime() != (int64_t)vtx[1].nTime)
            return DoS(50, error("CheckBlock() : coinstake timestamp violation nTimeBlock=%" PRId64 " nTimeTx=%u", GetBlockTime(), vtx[1].nTime));

        // NovaCoin: check proof-of-stake block signature
        if (fCheckSig && !CheckBlockSignature())
            return DoS(100, error("CheckBlock() : bad proof-of-stake block signature"));

        if (!vtx[1].CheckTransaction())
            return DoS(vtx[1].nDoS, error("CheckBlock() : CheckTransaction failed on coinstake"));

        uniqueTx.insert(vtx[1].GetHash());
        nSigOps += vtx[1].GetLegacySigOpCount();
    }
    else
    {
        // Check proof of work matches claimed amount
        if (fCheckPOW && !CheckProofOfWork(GetHash(), nBits))
            return DoS(50, error("CheckBlock() : proof of work failed"));

        // Check timestamp
        if (GetBlockTime() > FutureDrift(GetAdjustedTime()))
            return error("CheckBlock() : block timestamp too far in the future");

        // Check coinbase timestamp
        if (GetBlockTime() < PastDrift((int64_t)vtx[0].nTime))
            return DoS(50, error("CheckBlock() : coinbase timestamp is too late"));
    }

    // Iterate all transactions starting from second for proof-of-stake block 
    //    or first for proof-of-work block
    for (unsigned int i = fProofOfStake ? 2 : 1; i < vtx.size(); i++)
    {
        const CTransaction& tx = vtx[i];

        // Reject coinbase transactions at non-zero index
        if (tx.IsCoinBase())
            return DoS(100, error("CheckBlock() : coinbase at wrong index"));

        // Reject coinstake transactions at index != 1
        if (tx.IsCoinStake())
            return DoS(100, error("CheckBlock() : coinstake at wrong index"));

        // Check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, error("CheckBlock() : block timestamp earlier than transaction timestamp"));

        // Check transaction consistency
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));

        // Add transaction hash into list of unique transaction IDs
        uniqueTx.insert(tx.GetHash());

        // Calculate sigops count
        nSigOps += tx.GetLegacySigOpCount();
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    // Reject block if validation would consume too much resources.
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

    return true;
}

bool CBlock::AcceptBlock()
{
    // Check for duplicate
    auto hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    auto mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = (*mi).second;
    int nHeight = pindexPrev->nHeight+1;

    // Check proof-of-work or proof-of-stake
    if (nBits != GetNextTargetRequired(pindexPrev, IsProofOfStake()))
        return DoS(100, error("AcceptBlock() : incorrect %s", IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));

    int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();
    int nMaxOffset = 12 * nOneHour; // 12 hours
    if (fTestNet || pindexPrev->nTime < 1450569600)
        nMaxOffset = 7 * nOneWeek; // One week (permanently on testNet or until 20 Dec, 2015 on mainNet)

    // Check timestamp against prev
    if (GetBlockTime() <= nMedianTimePast || FutureDrift(GetBlockTime()) < pindexPrev->GetBlockTime())
        return error("AcceptBlock() : block's timestamp is too early");

    // Don't accept blocks with future timestamps
    if (pindexPrev->nHeight > 1 && nMedianTimePast  + nMaxOffset < GetBlockTime())
        return error("AcceptBlock() : block's timestamp is too far in the future");

    // Check that all transactions are finalized
    for(const CTransaction& tx :  vtx)
        if (!tx.IsFinal(nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    bool cpSatisfies = Checkpoints::CheckSync(hash, pindexPrev);

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::CP_STRICT && !cpSatisfies)
        return error("AcceptBlock() : rejected by synchronized checkpoint");

    if (CheckpointsMode == Checkpoints::CP_ADVISORY && !cpSatisfies)
        strMiscWarning = _("WARNING: syncronized checkpoint violation detected, but skipped!");

    // Enforce rule that the coinbase starts with serialized block height
    auto expect = CScript() << nHeight;
    if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
        !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = std::numeric_limits<unsigned int>::max();
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        for(CNode* pnode :  vNodes)
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    // ppcoin: check pending sync-checkpoint
    Checkpoints::AcceptPendingSyncCheckpoint();

    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    for(const CTransaction& tx :  vtx)
    {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTxCoinAge))
            nCoinAge += nTxCoinAge;
        else
            return false;
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug && GetBoolArg("-printcoinage"))
        printf("block coin age total nCoinDays=%" PRId64 "\n", nCoinAge);
    return true;
}

// ppcoin: check block signature
bool CBlock::CheckBlockSignature() const
{
    if (vchBlockSig.empty())
        return false;

    txnouttype whichType;
    vector<valtype> vSolutions;
    if (!Solver(vtx[1].vout[1].scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        auto& vchPubKey = vSolutions[0];
        CPubKey key(vchPubKey);
        if (!key.IsValid())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }

    return false;
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew)
{
    auto hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
    {
        txdb.TxnAbort();
        InvalidChainFound(pindexNew);
        return false;
    }
    if (!txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    for(CTransaction& tx :  vtx)
        mempool.remove(tx);

    return true;
}

bool IsInitialBlockDownload()
{
    if (pindexBest == NULL || nBestHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static int64_t nLastUpdate;
    static CBlockIndex* pindexLastBest;
    auto nCurrentTime = GetTime();
    if (pindexBest != pindexLastBest)
    {
        pindexLastBest = pindexBest;
        nLastUpdate = nCurrentTime;
    }
    return (nCurrentTime - nLastUpdate < 10 &&
            pindexBest->GetBlockTime() < nCurrentTime - nOneDay);
}
