// Copyright (c) 2013-2016 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php

#ifndef NOVACOIN_TRANSACTION_H
#define NOVACOIN_TRANSACTION_H


#include "uint256.h"
#include "serialize.h"

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

#endif // NOVACOIN_TRANSACTION_H