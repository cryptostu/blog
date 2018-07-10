---
title: 'Bitcoin UTXO代码分析(二):CCoinsViewCache'
date: 2018-02-09 11:06:34
tags:
---
> 在上一篇[Bitcoin UTXO代码分析(一):UTXO的相关表示](https://mp.weixin.qq.com/s/UKFW05QFTz9mGMG7-tCXXw)中，简要说明了UTXO在Bitcoin是使用那些类表示的，这篇文章继续分析下UTXO的标记和花费。

`CCoinsViewCache`类有几个重要的方法，下面介绍下主要方法的使用
## Coin处理方法
#### 获取
其中获取Coin的方法是:

```c++
CCoinsMap::iterator CCoinsViewCache::FetchCoin(const COutPoint &outpoint) const {
    CCoinsMap::iterator it = cacheCoins.find(outpoint);
    if (it != cacheCoins.end())
        return it;
    Coin tmp;
    if (!base->GetCoin(outpoint, tmp))
        return cacheCoins.end();
    CCoinsMap::iterator ret = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::forward_as_tuple(std::move(tmp))).first;
    if (ret->second.coin.IsSpent()) {
        // The parent only has an empty entry for this outpoint; we can consider our
        // version as fresh.
        ret->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += ret->second.coin.DynamicMemoryUsage();
    return ret;
}

```

1. 使用传入的outpoint 对象作为key, 在coinViewCache 内部成员hashmap 中查找， 找到返回。
2. 找不到，调到coinViewCache对象初始化是传入的后端(parent view), 中查找， 找不到返回。 
3. 在parent view找到，使用此条目填充内部hashmap，如果找到的币已经被消费，标记此条目为fresh, 这样，在向后端(parent view)flush 时， 就跳过了此条目， 最后增加一个币的动态内存占用。

#### 添加
添加`Coin`的方法是:
```c++
void CCoinsViewCache::AddCoin(const COutPoint &outpoint, Coin&& coin, bool possible_overwrite) {
    assert(!coin.IsSpent());
    if (coin.out.scriptPubKey.IsUnspendable()) return;
    CCoinsMap::iterator it;
    bool inserted;
    std::tie(it, inserted) = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    bool fresh = false;
    if (!inserted) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
    }
    if (!possible_overwrite) {
        if (!it->second.coin.IsSpent()) {
            throw std::logic_error("Adding new coin that replaces non-pruned entry");
        }
        fresh = !(it->second.flags & CCoinsCacheEntry::DIRTY);
    }
    it->second.coin = std::move(coin);
    it->second.flags |= CCoinsCacheEntry::DIRTY | (fresh ? CCoinsCacheEntry::FRESH : 0);
    cachedCoinsUsage += it->second.coin.DynamicMemoryUsage();
}
```  
上述代码分析，断言：币没有被消费，币是可消费的，测试传入的outpoint有没有存在于cache, 如果没有，则减掉一个币的内存占用。 否则的话，更新coin对象，更新flags，增加一个币的内存占用。
#### 花费
花费Coin的方法是:
```c++
bool CCoinsViewCache::SpendCoin(const COutPoint &outpoint, Coin* moveout) {
    CCoinsMap::iterator it = FetchCoin(outpoint);
    if (it == cacheCoins.end()) return false;
    cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
    if (moveout) {
        *moveout = std::move(it->second.coin);
    }
    if (it->second.flags & CCoinsCacheEntry::FRESH) {
        cacheCoins.erase(it);
    } else {
        it->second.flags |= CCoinsCacheEntry::DIRTY;
        it->second.coin.Clear();
    }
    return true;
}
```
SpendCoin消费掉outpoint对应的币，根据传入moveout 指针是否为空，是否保存对应的coin对象，如果找到的条目状态是fresh 的，表明和后端视图一致，删除此条目， 否则此条目为dirty， 把币值设为-1。
#### 保存
保存的方法是:
```c++
bool CCoinsViewCache::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlockIn) {
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); it = mapCoins.erase(it)) {
        // Ignore non-dirty entries (optimization).
        if (!(it->second.flags & CCoinsCacheEntry::DIRTY)) {
            continue;
        }
        CCoinsMap::iterator itUs = cacheCoins.find(it->first);
        if (itUs == cacheCoins.end()) {
            // The parent cache does not have an entry, while the child does
            // We can ignore it if it's both FRESH and pruned in the child
            if (!(it->second.flags & CCoinsCacheEntry::FRESH && it->second.coin.IsSpent())) {
                // Otherwise we will need to create it in the parent
                // and move the data up and mark it as dirty
                CCoinsCacheEntry& entry = cacheCoins[it->first];
                entry.coin = std::move(it->second.coin);
                cachedCoinsUsage += entry.coin.DynamicMemoryUsage();
                entry.flags = CCoinsCacheEntry::DIRTY;
                // We can mark it FRESH in the parent if it was FRESH in the child
                // Otherwise it might have just been flushed from the parent's cache
                // and already exist in the grandparent
                if (it->second.flags & CCoinsCacheEntry::FRESH) {
                    entry.flags |= CCoinsCacheEntry::FRESH;
                }
            }
        } else {
            // Assert that the child cache entry was not marked FRESH if the
            // parent cache entry has unspent outputs. If this ever happens,
            // it means the FRESH flag was misapplied and there is a logic
            // error in the calling code.
            if ((it->second.flags & CCoinsCacheEntry::FRESH) && !itUs->second.coin.IsSpent()) {
                throw std::logic_error("FRESH flag misapplied to cache entry for base transaction with spendable outputs");
            }

            // Found the entry in the parent cache
            if ((itUs->second.flags & CCoinsCacheEntry::FRESH) && it->second.coin.IsSpent()) {
                // The grandparent does not have an entry, and the child is
                // modified and being pruned. This means we can just delete
                // it from the parent.
                cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                cacheCoins.erase(itUs);
            } else {
                // A normal modification.
                cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                itUs->second.coin = std::move(it->second.coin);
                cachedCoinsUsage += itUs->second.coin.DynamicMemoryUsage();
                itUs->second.flags |= CCoinsCacheEntry::DIRTY;
               
            }
        }
    }
    hashBlock = hashBlockIn;
    return true;
}
```
CCoinsViewCache 的BatchWrite方法，主要是服务于coinViewCache 对象作为另一个coinview 对象的后端视图时，通过CoinsMap 对象接受上层视图的状态，更新此对象的内部状态：在迭代上层视图时，跳过非dirty的条目（即没有被修改的条目),  如果找到一个条目存在于上层视图，在本对象内部找不到，跳过fresh 且被消费掉的条目，剩下的刷新到本视图；如果找到了，条目在上层视图是fresh且被消费了，在本视图中删除掉同key的条目，否则就更新条目中的币，标记此条目dirty。

## CCoinsViewCache的其他方法
另外还有一些衍生的方法用来方便使用的，比如：
```c++
bool CCoinsViewCache::HaveCoin(const COutPoint &outpoint) const {
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    return it != cacheCoins.end() && !it->second.coin.IsSpent();
}

void CCoinsViewCache::Uncache(const COutPoint &outpoint) {
    CCoinsMap::iterator it = cacheCoins.find(outpoint);
    if (it != cacheCoins.end() && it->second.flags == 0) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        cacheCoins.erase(it);
    }
}
const CTxOut &CCoinsViewCache::GetOutputFor(const CTxIn &input) const {
    const Coin &coin = AccessCoin(input.prevout);
    assert(!coin.IsSpent());
    return coin.GetTxOut();
}
......
```
以上方法是对一些简化逻辑的封装，更方便的被外部调用。

另外`CCoinsViewCache`还会记录当前的最高块的hash，主要用来在判断当前的UTXO是在那个高度的UTXO,它对应的方法有:
```c++
uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull()) {
        hashBlock = base->GetBestBlock();
    }
    return hashBlock;
}
void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

```
***
本文由 `Copernicus团队` 喻建写作，转载无需授权

