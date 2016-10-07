// Copyright (c) 2013-2016 The NovaCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php

#include "transaction.h"
#include "util.h"

using namespace std;

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

#include "transaction.h"