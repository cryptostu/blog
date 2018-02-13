---
title: 'Bitcoin UTXO代码分析(三):和其它模块的交互'
date: 2018-02-14 06:16:54
tags:
---
前两篇介绍了UXTO表示以及`CCoinViewCache`的使用:[Bitcoin UTXO代码分析(一):UTXO的相关表示](https://mp.weixin.qq.com/s/UKFW05QFTz9mGMG7-tCXXw)和[Bitcoin UTXO代码分析(二):CCoinsViewCache](https://mp.weixin.qq.com/s/kShEKSZM2ppVfPdc2rpOPg)，这篇文章主要介绍UTXO和其他模块的交互：新块被激活的时候如何更新UTXO，内存池中的交易和UTXO如何交互，以及UTXO的存储。
## Blockchain激活
Blockchain发生变化时UTXO相关的逻辑：
```c++
bool CChainState::ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions &disconnectpool)
{
   ....
   {
        CCoinsViewCache view(pcoinsTip.get());
        bool rv = ConnectBlock(blockConnecting, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(blockConnecting, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);
            return error("ConnectTip(): ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        nTime3 = GetTimeMicros(); nTimeConnectTotal += nTime3 - nTime2;
        LogPrint(BCLog::BENCH, "  - Connect total: %.2fms [%.2fs (%.2fms/blk)]\n", (nTime3 - nTime2) * MILLI, nTimeConnectTotal * MICRO, nTimeConnectTotal * MILLI / nBlocksTotal);
        bool flushed = view.Flush();
        assert(flushed);
    }

   .....
}

bool CChainState::DisconnectTip(CValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions *disconnectpool)
{
          ...
          
          {
        CCoinsViewCache view(pcoinsTip.get());
        assert(view.GetBestBlock() == pindexDelete->GetBlockHash());
        if (DisconnectBlock(block, pindexDelete, view) != DISCONNECT_OK)
            return error("DisconnectTip(): DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        bool flushed = view.Flush();
        assert(flushed);
    }

          ...
}

```
在blockchain结构重新组织，当前`activeChain`发生变化时，某些block会链接上activeChain, 某些block会断掉跟activeChain的链接，内部会调用`CChainState::ConnectTip`和`CChainState::DisconnectTip`，这里会生成临时的CCoinsViewCache对象，后端连接上全局的另一个CCoinsViewCache实例`pcoinsTip`, 在调用ConnectBlock，DisconnectBlock后，更新自己的状态到`pcoinsTip`。
## MemPool相关
有一个对象专门用来处理MemPool相关的UTXO，对象为`CCoinsViewMemPool`:
```c++

class CCoinsViewMemPool : public CCoinsViewBacked
{
protected:
    const CTxMemPool& mempool;

public:
    CCoinsViewMemPool(CCoinsView* baseIn, const CTxMemPool& mempoolIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
};

{
   ...
   CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : mtx.vin) {
            view.AccessCoin(txin.prevout); // Load entries from viewChain into view; can fail.
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

   ...
}
```
在rpc请求时，比如:*signrawtransaction*，*combinerawtransaction*，构造了临时viewcache对象, 临时viewMempool对象`CCoinsViewMemPool`，`CCoinsViewMemPool`被设为view的后端，这样确保mtx交易的父交易，数据来源包括当前activeChain的内存部分，磁盘部分，还有mempool。
`gettxout`请求：
```c++
UniValue gettxout(const JSONRPCRequest& request)
{
   ...
   if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip.get(), mempool);
        if (!view.GetCoin(out, coin) || mempool.isSpent(out)) {
            return NullUniValue;
        }
    } else {
        if (!pcoinsTip->GetCoin(out, coin)) {
            return NullUniValue;
        }
    }
   ...
}
```
rpc请求gettxout处理时，如果`include_mempool` 为true，会构造临时ViewMemPool，方便外部用户查询utxo时，引入mempool中的utxo。
检查timelock交易的代码：
```c++
bool CheckSequenceLocks(const CTransaction &tx, int flags, LockPoints* lp, bool useExistingLockPoints)
{
      ...
      {
          CCoinsViewMemPool viewMemPool(pcoinsTip.get(), mempool);
        std::vector<int> prevheights;
        prevheights.resize(tx.vin.size());
        for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
            const CTxIn& txin = tx.vin[txinIndex];
            Coin coin;
            if (!viewMemPool.GetCoin(txin.prevout, coin)) {
                return error("%s: Missing input", __func__);
            }
            if (coin.nHeight == MEMPOOL_HEIGHT) {
                // Assume all mempool transaction confirm in the next block
                prevheights[txinIndex] = tip->nHeight + 1;
            } else {
                prevheights[txinIndex] = coin.nHeight;
            }
        }

      }
      ...
}
```
评估使用`timelock`的交易时，需要再次构造临时*ViewMemPool*，查询当前blockchain tip对应的coin集合与内存池的coin集合。
有交易进入内存池时：
```c++
static bool AcceptToMemoryPoolWorker(const CChainParams& chainparams, CTxMemPool& pool, CValidationState& state, const CTransactionRef& ptx,
                              bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced,
                              bool bypass_limits, const CAmount& nAbsurdFee, std::vector<COutPoint>& coins_to_uncache)
{
       .....
       CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        LockPoints lp;
        {
        LOCK(pool.cs);
        CCoinsViewMemPool viewMemPool(pcoinsTip.get(), pool);
        view.SetBackend(viewMemPool);

        // do all inputs exist?
        for (const CTxIn txin : tx.vin) {
            if (!pcoinsTip->HaveCoinInCache(txin.prevout)) {
                coins_to_uncache.push_back(txin.prevout);
            }
            if (!view.HaveCoin(txin.prevout)) {
                // Are inputs missing because we already have the tx?
                for (size_t out = 0; out < tx.vout.size(); out++) {
                    // Optimistically just do efficient check of cache for outputs
                    if (pcoinsTip->HaveCoinInCache(COutPoint(hash, out))) {
                        return state.Invalid(false, REJECT_DUPLICATE, "txn-already-known");
                    }
                }
                // Otherwise assume this might be an orphan tx for which we just haven't seen parents yet
                if (pfMissingInputs) {
                    *pfMissingInputs = true;
                }
                return false; // fMissingInputs and !state.IsInvalid() is used to detect this condition, don't set state.Invalid()
            }
        }

        // Bring the best block into scope
        view.GetBestBlock();
        view.SetBackend(dummy);
        ......
}
```
在`AcceptToMemoryPoolWorker`中，构造了临时ccoinsviewcache对象view， 
临时ccoinsviewMemPool对象viewmempool，view 后端数据来源是viewmempool。

## UTXO的存储
utxo 的磁盘存储使用了leveldb 键值对数据库， 键的序列化格式：
>C[32 bytes of outpoint->hash][varint(outpoint->n]

```c++
struct CoinEntry {
    COutPoint* outpoint;
    char key;
    explicit CoinEntry(const COutPoint* ptr) : outpoint(const_cast<COutPoint*>(ptr)), key(DB_COIN)  {}

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << key;
        s << outpoint->hash;
        s << VARINT(outpoint->n);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> key;
        s >> outpoint->hash;
        s >> VARINT(outpoint->n);
    }
};
```
value 对应的存储格式是就是coin 对象的序列化形式:
> VARINT((coinbase?1: 0) | (height <<1))

CTxout 的序列化格式(使用CTxOutCompressor 类定制特殊压缩方式)。

```c++
template<typename Stream>
    void Serialize(Stream &s) const {
        assert(!IsSpent());
        uint32_t code = nHeight * 2 + fCoinBase;
        ::Serialize(s, VARINT(code));
        ::Serialize(s, CTxOutCompressor(REF(out)));
}
```
在每一个CTxout 对象本身之前， 加上了一个变长编码的`code = nHeight * 2 + fCoinBase `数字，接着就是txcout 本身:

```c++
class CTxOutCompressor
{
     inline void SerializationOp(Stream& s, Operation ser_action) {
        if (!ser_action.ForRead()) {
            uint64_t nVal = CompressAmount(txout.nValue);
            READWRITE(VARINT(nVal));
        } else {
            uint64_t nVal = 0;
            READWRITE(VARINT(nVal));
            txout.nValue = DecompressAmount(nVal);
        }
        CScriptCompressor cscript(REF(txout.scriptPubKey));
        READWRITE(cscript);
    }
}
```
接着是被压缩的数目, 使用`CompressAmount`成员函数中的算法:

```c++
uint64_t CTxOutCompressor::CompressAmount(uint64_t n)
{
    if (n == 0)
        return 0;
    int e = 0;
    while (((n % 10) == 0) && e < 9) {
        n /= 10;
        e++;
    }
    if (e < 9) {
        int d = (n % 10);
        assert(d >= 1 && d <= 9);
        n /= 10;
        return 1 + (n*9 + d - 1)*10 + e;
    } else {
        return 1 + (n - 1)*10 + 9;
    }
}
```

CTxout 中的锁定脚本使用CScriptCompressor 对象压缩存储:
```c++
class CScriptCompressor
{
    template<typename Stream>
    void Serialize(Stream &s) const {
        std::vector<unsigned char> compr;
        if (Compress(compr)) {
            s << CFlatData(compr);
            return;
        }
        unsigned int nSize = script.size() + nSpecialScripts;
        s << VARINT(nSize);
        s << CFlatData(script);
    }
}
```

基本思路， 对于p2pkh 类型的脚本, 比如:
>OP_DUP OP_HASH160 ab68025513c3dbd2f7b92a94e0581f5d50f654e7 OP_EQUALVERIFY OP_CHECKSIG, 

压缩成:
>[0x00][ab68025513c3dbd2f7b92a94e0581f5d50f654e7]

总共21 字节。

p2sh 类型脚本， 比如:
>OP_HASH160 20 0x620a6eeaf538ec9eb89b6ae83f2ed8ef98566a03 OP_EQUAL

压缩成:
>[0x01][620a6eeaf538ec9eb89b6ae83f2ed8ef98566a03]

总共21字节。

p2pk 类型脚本， 比如:
>33 0x022df8750480ad5b26950b25c7ba79d3e37d75f640f8e5d9bcd5b150a0f85014da OP_CHECKSIG

压缩成：
>[0x2][2df8750480ad5b26950b25c7ba79d3e37d75f640f8e5d9bcd5b150a0f85014da]

总共33 字节， 未压缩的p2pk脚本， 首字节是0x04|(pubkey[64]&0x01), 接着是32字节的pubkey的x 坐标。

对于其它不能被压缩的脚本， 如segwit 的scriptPubKey，采用下面方法:
```c++
unsigned int nSize = script.size() + nSpecialScripts;
s << VARINT(nSize);
s << CFlatData(script);
```
***
本文由 `Copernicus团队` 喻建写作，转载无需授权



