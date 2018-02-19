---
title: BCH工作量证明源代码分析
date: 2018-02-19 10:02:04
tags:
---
## 概述
Bitcoin Cash 源码中，POW功能模块，主要提供两个函数，供上层进行调用：

1. `GetNextWorkRequired`: 获取下个块的工作量(即难度)
2. `CheckProofOfWork`: 检查块的工作量是否合法。 TRUE：合法； false：不合法。
下面是详细分析
## 获取下个块的难度
```
uint32_t GetNextWorkRequired(const CBlockIndex *pindexPrev,
    const CBlockHeader *pblock, const Consensus::Params &params) {
    
     // Genesis block
    if (pindexPrev == nullptr) {
        return UintToArith256(params.powLimit).GetCompact();
    }

    // Special rule for regtest: we never retarget.
    if (params.fPowNoRetargeting) {
        return pindexPrev->nBits;
    }

    if (pindexPrev->GetMedianTimePast() >=
        GetArg("-newdaaactivationtime", params.cashHardForkActivationTime)) {
        return GetNextCashWorkRequired(pindexPrev, pblock, params);
    }

    return GetNextEDAWorkRequired(pindexPrev, pblock, params);
}
```
* 参数，pindexprev : 当前区块的父区块(In); pblock : 当前区块(In),主要使用了其中的时间戳字段; param : 当前的链参数
* 如果为父区块为创世块，直接返回当前链参数配置的最低难度。
* 如果当前的链为回归测试链(regtest 测试链)，返回与父区块一样的难度
* 如果父区块的MTP时间 >= CashHardWokd(硬分叉难度调整DAA)的激活时间，那采用新的难度算法
* 采用以前的难度算法。

## BCH的难度调整
```
uint32_t GetNextCashWorkRequired(const CBlockIndex *pindexPrev,
        const CBlockHeader *pblock, const Consensus::Params &params) {
    // This cannot handle the genesis block and early blocks in general.
    assert(pindexPrev);

    // Special difficulty rule for testnet:
    // If the new block's timestamp is more than 2* 10 minutes then allow
    // mining of a min-difficulty block.  //
    if (params.fPowAllowMinDifficultyBlocks &&
        (pblock->GetBlockTime() >
         pindexPrev->GetBlockTime() + 2 * params.nPowTargetSpacing)) {
        return UintToArith256(params.powLimit).GetCompact();
    }

    // Compute the difficulty based on the full adjustement interval.
    const uint32_t nHeight = pindexPrev->nHeight;
    assert(nHeight >= params.DifficultyAdjustmentInterval());

    // Get the last suitable block of the difficulty interval.
    const CBlockIndex *pindexLast = GetSuitableBlock(pindexPrev);
    assert(pindexLast);

    // Get the first suitable block of the difficulty interval.
    uint32_t nHeightFirst = nHeight - 144;
    const CBlockIndex *pindexFirst =
        GetSuitableBlock(pindexPrev->GetAncestor(nHeightFirst));
    assert(pindexFirst);

    // Compute the target based on time and work done during the interval.
    const arith_uint256 nextTarget =
        ComputeTarget(pindexFirst, pindexLast, params);

    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    if (nextTarget > powLimit) {
        return powLimit.GetCompact();
    }

    return nextTarget.GetCompact()
}
```

* 如果当前链为测试链(testnet 测试链)，并且当前块的时间与父区块的时间间隔大于nPowTargetSpacing *2，允许下个块采用当前链的最低难度
* 获取父区块的往上3个块的中值区块，作为结束位置
* 获取当前父区块的第144个祖先区块的中值区块，作为起始位置
* 依据起始位置，结束位置，和链参数计算下个块的难度(工作量)work
* 当下个块的难度低于当前链最低难度时，返回当前链的最低难度;否则返回计算后的难度
* 总结：现阶段采用的算法是：进行逐块调整难度，调整机制如下

## BCH采用的难度计算方式
```
/**
 * Compute the a target based on the work done between 2 blocks and the time
 * required to produce that work.
 */
static arith_uint256 ComputeTarget(const CBlockIndex *pindexFirst,
                                   const CBlockIndex *pindexLast,
                                   const Consensus::Params &params) {
    assert(pindexLast->nHeight > pindexFirst->nHeight);

    /**
     * From the total work done and the time it took to produce that much work,
     * we can deduce how much work we expect to be produced in the targeted time
     * between blocks.
     */
std::cout << "pindexLast->height : " << pindexLast->nHeight << ", pindexLast->nChainWork : " << pindexLast->nChainWork.GetCompact() <<
          ", pindexFirst->nHeight : " << pindexFirst->nHeight << ", pindexFirst->nChainWork : " << pindexFirst->nChainWork.GetCompact() << std::endl;
    arith_uint256 work = pindexLast->nChainWork - pindexFirst->nChainWork;
    work *= params.nPowTargetSpacing;

    // In order to avoid difficulty cliffs, we bound the amplitude of the
    // adjustement we are going to do.
    assert(pindexLast->nTime > pindexFirst->nTime);
    int64_t nActualTimespan = pindexLast->nTime - pindexFirst->nTime;
    if (nActualTimespan > 288 * params.nPowTargetSpacing) {
        nActualTimespan = 288 * params.nPowTargetSpacing;
    } else if (nActualTimespan < 72 * params.nPowTargetSpacing) {
        nActualTimespan = 72 * params.nPowTargetSpacing;
    }

    work /= nActualTimespan;

    /**
     * We need to compute T = (2^256 / W) - 1 but 2^256 doesn't fit in 256 bits.
     * By expressing 1 as W / W, we get (2^256 - W) / W, and we can compute
     * 2^256 - W as the complement of W.
     */
    return (-work) / work;
}
```

* 计算起始位置至结束位置累计的工作量
* 根据实际出块时间与目标出块时间进行调整
* 尽量保证在1天之内出144个块,保证10分钟一个块
* 如果一天之内超过了144个块，则需要增加难度，反之就要降低难度
* 为了保证调整算法的不出现剧烈波动，一天的出块时间最多不超过288个，最少不低于72个
* 返回将计算后的难度

## BCH以前采用的EDA难度调整算法
采用EDA的算法计算下个块的难度：
```
uint32_t GetNextEDAWorkRequired(const CBlockIndex *pindexPrev,
    const CBlockHeader *pblock, const Consensus::Params &params) {
    // Only change once per difficulty adjustment interval
    uint32_t nHeight = pindexPrev->nHeight + 1;
    if (nHeight % params.DifficultyAdjustmentInterval() == 0) {
        // Go back by what we want to be 14 days worth of blocks
        assert(nHeight >= params.DifficultyAdjustmentInterval());
        uint32_t nHeightFirst = nHeight - params.DifficultyAdjustmentInterval();
        const CBlockIndex *pindexFirst = pindexPrev->GetAncestor(nHeightFirst);
        assert(pindexFirst);

        return CalculateNextWorkRequired(pindexPrev,
                                         pindexFirst->GetBlockTime(), params);
    }

    const uint32_t nProofOfWorkLimit =
        UintToArith256(params.powLimit).GetCompact();

    if (params.fPowAllowMinDifficultyBlocks) {
        // Special difficulty rule for testnet:
        // If the new block's timestamp is more than 2* 10 minutes then allow
        // mining of a min-difficulty block.
        if (pblock->GetBlockTime() >
            pindexPrev->GetBlockTime() + 2 * params.nPowTargetSpacing) {
            return nProofOfWorkLimit;
        }

        // Return the last non-special-min-difficulty-rules-block
        const CBlockIndex *pindex = pindexPrev;
        while (pindex->pprev &&
               pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
               pindex->nBits == nProofOfWorkLimit) {
            pindex = pindex->pprev;
        }

        return pindex->nBits;
    }
    // We can't go bellow the minimum, so early bail.
    uint32_t nBits = pindexPrev->nBits;
    if (nBits == nProofOfWorkLimit) {
        return nProofOfWorkLimit;
    }

    // If producing the last 6 block took less than 12h, we keep the same
    // difficulty.
    const CBlockIndex *pindex6 = pindexPrev->GetAncestor(nHeight - 7);
    assert(pindex6);
    int64_t mtp6blocks =
        pindexPrev->GetMedianTimePast() - pindex6->GetMedianTimePast();
    if (mtp6blocks < 12 * 3600) {
        return nBits;
    }

    // If producing the last 6 block took more than 12h, increase the difficulty
    // target by 1/4 (which reduces the difficulty by 20%). This ensure the
    // chain do not get stuck in case we lose hashrate abruptly.
    arith_uint256 nPow;
    nPow.SetCompact(nBits);

    nPow += (nPow >> 2);

    // Make sure we do not go bellow allowed values.
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (nPow > bnPowLimit) nPow = bnPowLimit;

    return nPow.GetCompact();
    ......
}
```

* 每2016个块调整一次难度`nHeight % params.DifficultyAdjustmentInterval() == 0`, 符合难度条件，则进入难度判断:
    * 获取计算起始位置的块索引，依据：起始位置，结束位置，链参数 计算下个块的难度
* 如果当前链为测试链(testnet)，进入下面逻辑
    1. 当前块与父区块的时间间隔大于 nPowTargetSpacing * 2，返回最低难度。
    2. 返回最后一个不等于最低难度的块的难度
* 如果难度不在调整周期，并且父区块的难度为当前链参数的最低难度，直接返回最低难度
* 如果6个祖先块的MTP时间间隔小于12小时，直接返回上个区块的难度
* 不然就降低到当前难度1/4的难度：` nPow += (nPow >> 2);`
* 当下个块的难度低于当前链最低难度时，返回当前链的最低难度;否则返回计算后的难度
总结：以前的难度调节机制是，主要分为两种：每隔2016个块`params.DifficultyAdjustmentInterval()`进行一次大的难度调整。在难度稳定期间(相对来说)，每6个块进行一次判断，看是否需要进行难度调整，如果6个块的出块时间大于12小时，将上个区块的难度降低1/4，作为下个块的难度。

## EDA所采用的难度计算方法
依据起始位置，结束位置，链参数，计算下个块的难度
```
uint32_t CalculateNextWorkRequired(const CBlockIndex *pindexPrev,
        if (params.fPowNoRetargeting) {
        return pindexPrev->nBits;
    }
    // Limit adjustment step
    int64_t nActualTimespan = pindexPrev->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan / 4) {
        nActualTimespan = params.nPowTargetTimespan / 4;
    }
    if (nActualTimespan > params.nPowTargetTimespan * 4) {
        nActualTimespan = params.nPowTargetTimespan * 4;
    }
    std::cout << "nActualTimespan : " << nActualTimespan << std::endl;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit) bnNew = bnPowLimit;

    return bnNew.GetCompact();
}
```

* 如果为回归测试链,直接返回父区块的难度
* 计算实际的时间间隔
* 当实际时间间隔 < 预定目标的1/4时，将下阶段的时间间隔设为预定目标的1/4；或当实际时间间隔 > 预定目标的4倍时，将下阶段的时间间隔设为预定目标的4倍。
* 计算新的难度`bnNew *= nActualTimespan;`;
* 当新难度低于当前链最低难度时，直接返回最低难度；否则返回计算后的新难度。
可以看出以前的难度调整算法：以4基础进行调整。当难度太小时，即出块的时间变短，将下阶段的时间增加至目标时间的1/4，进行缓慢增加难度；当难度太大时，即出块的时间变长，将下阶段时间降低至目标时间的4倍，缓慢降低难度；上述调节措施可以避免难度的剧烈波动。

## 块头工作量的检查
```
bool CheckProofOfWork(uint256 hash, uint32_t nBits,
                      const Consensus::Params &params) {
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow ||
        bnTarget > UintToArith256(params.powLimit)) {
        return false;
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget) {
        return false;
    }

    return true;
}
```

* 参数，hash 将要检查的区块哈希；nBits 该区块中的难度字段；param：当前链参数
* 将难度编码为BCH中指定大数类型，判断编码过程中是否有溢出，负数，或难度小于当前链的最低难度情况，如果存在，返回false。
* 将hash转换为BCH中指定的大数类型，与块头难度编码后的值进行比较。如果大于块头难度，返回false。否则返回true。
该函数用来判断：块头哈希与块中声明的难度是否吻合(即该区块的工作量是否正确，不依赖于上下文)。

***
本文由 `Copernicus团队 姚永芯`写作，转载无需授权。

