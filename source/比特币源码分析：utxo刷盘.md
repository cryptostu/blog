---
title: 比特币源码分析：utxo刷盘
date: 2018-03-01 15:49:05
tags:
---

utxo的刷盘逻辑主要在` txdb.cpp`中实现，主要是 `CoinsViewDB::batchwrite`这个函数。下面我们来分析一下：

```
bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CoinEntry entry(&it->first);
            if (it->second.coin.IsSpent()) {
                batch.Erase(entry);
            } else {
                batch.Write(entry, it->second.coin);
            }
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }
    if (!hashBlock.IsNull()) {
        batch.Write(DB_BEST_BLOCK, hashBlock);
    }

    bool ret = db.WriteBatch(batch);
    LogPrint("coindb", "Committed %u changed transaction outputs (out of %u) "
                       "to coin database...\n",
             (unsigned int)changed, (unsigned int)count);
    return ret;
}
```
在前面我们介绍过 `CDBWrapper`主要是对 leveldb的一个简单封装，定义一个`CDBWrapper db;`我们拿着 db 就可以实现相应的操作。
![CDBWrapper.png](http://upload-images.jianshu.io/upload_images/6967649-c69be5f4d754be45.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


接下来迭代mapCoins，并填充其值，这里最主要的就是作为k-v数据库的leveldb中的key与value如何获得：
### key
CoinEntry是一个辅助工具类。
```
struct CoinEntry {
    COutPoint *outpoint;
    char key;
    CoinEntry(const COutPoint *ptr)
        : outpoint(const_cast<COutPoint *>(ptr)), key(DB_COIN) {}

    template <typename Stream> void Serialize(Stream &s) const {
        s << key;
        s << outpoint->hash;
        s << VARINT(outpoint->n);
    }
};
```
key指向的是outpoint，具体结构如下：
![key](http://upload-images.jianshu.io/upload_images/6967649-35fcd750376ea4cc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

我们将序列化后的值当作key，作为entry的参数，同时作为`db.write`的key。
>关于db.write和db.WriteBatch二者之间的联系，前面已经详细分析。
### value
value的值就是 coin 序列化后的值，而 coin 又包含了txout，如下：
```
class Coin {
    //! Unspent transaction output.
    CTxOut out;

    //! Whether containing transaction was a coinbase and height at which the
    //! transaction was included into a block.
    uint32_t nHeightAndIsCoinBase;
```
同样的，我们进行序列化并使用`CTxOutCompressor`对txout进行压缩，REF是一个宏定义，是非const转换，我们首先断言这个币是否被消费：
```
 template <typename Stream> void Serialize(Stream &s) const {
        assert(!IsSpent());
        ::Serialize(s, VARINT(nHeightAndIsCoinBase));
        ::Serialize(s, CTxOutCompressor(REF(out)));
    }
```
txout主要包含：
```
class CTxOut {
public:
    Amount nValue;
    CScript scriptPubKey;

```
对nValue和scriptPubKey采用了不同的压缩方式来进行序列化，如下：
```
class CTxOutCompressor {
private:
    CTxOut &txout;
public:
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
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
};
```

这时候我们就拿到了`db.write`的value值，这时候我们通过for循环，不断迭代，将值写入磁盘。

