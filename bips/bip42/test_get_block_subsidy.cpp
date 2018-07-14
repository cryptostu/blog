#include <cstdint>
#include <cstdio>

// from file amount.h
/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

//static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

// from file params.h after BIP 42: A finite monetary supply for Bitcoin
/*
CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;
    // Force block reward to zero when right shift is undefined.
    if (halvings >= 64)
        return 0;

    CAmount nSubsidy = 50 * COIN;
    // Subsidy is cut in half every 210,000 blocks which will occur
approximately every 4 years. nSubsidy >>= halvings; return nSubsidy;
}
*/

// to make the following code from Bitcoin 0.8.0 work
typedef int64_t int64;

// code from 0.8.0 version Bitcoin Core
// https://github.com/bitcoin/bitcoin/blob/v0.8.0/src/main.cpp#L1053
int64 static GetBlockValue(int nHeight, int64 nFees) {
    int64 nSubsidy = 50 * COIN;

    // Subsidy is cut in half every 210000 blocks, which will occur
    // approximately every 4 years
    nSubsidy >>= (nHeight / 210000);

    return nSubsidy + nFees;
}

void GetBlockValueTest() {
    int64 sum = 0;
    for (int height = 0; height < 14000000; height += 1000) {
        int64 reward = GetBlockValue(height, 0);
        sum += reward;
    }
}

static const CAmount COIN = 100000000;
int main() {
    int64 total = 50 * COIN;
    for (int i = 0; i < 30; ++i) {
        int64 r1 = (total >> i);
        int64 r2 = (total >> (i + 30));
        int64 r3 = (total >> (i + 60));
        printf("%2d: %10lld || %2d: %10lld || %2d: %10lld\n", i, r1, i + 30, r2,
               i + 60, r3);
    }
}
