---
title: 'Bitcoin UTXO代码分析(一):UTXO的相关表示'
date: 2018-02-08 08:43:44
tags:
---
在Bitcoin代码中，使用*Coin*类来表示单个交易对象中某个输出的币:
```c++
class Coin
{
public:
    //! unspent transaction output
    CTxOut out;

    //! whether containing transaction was a coinbase
    unsigned int fCoinBase : 1;
    
    //! at which height this containing transaction was included in the active block chain
    uint32_t nHeight : 31;
    .....
    .....
    
}
```
数据元素除了`CTxOut`中的币值(nValue)、花费条件(scriptPubKey)之外, 还附带了一些元信息：是否是coinbase, 所在交易在哪个高度被打包进入Blockchain。
再使用`CCoinsView`抽象类表达整个blockchain上的币的集合:
```c++
class CCoinsView
{
public:
    /** Retrieve the Coin (unspent transaction output) for a given outpoint.
     *  Returns true only when an unspent coin was found, which is returned in coin.
     *  When false is returned, coin's value is unspecified.
     */
    virtual bool GetCoin(const COutPoint &outpoint, Coin &coin) const;

    //! Just check whether a given outpoint is unspent.
    virtual bool HaveCoin(const COutPoint &outpoint) const;

    //! Retrieve the block hash whose state this CCoinsView currently represents
    virtual uint256 GetBestBlock() const;

    //! Retrieve the range of blocks that may have been only partially written.
    //! If the database is in a consistent state, the result is the empty vector.
    //! Otherwise, a two-element vector is returned consisting of the new and
    //! the old block hash, in that order.
    virtual std::vector<uint256> GetHeadBlocks() const;

    //! Do a bulk modification (multiple Coin changes + BestBlock change).
    //! The passed mapCoins can be modified.
    virtual bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock);

    //! Get a cursor to iterate over the whole state
    virtual CCoinsViewCursor *Cursor() const;

    //! As we use CCoinsViews polymorphically, have a virtual destructor
    virtual ~CCoinsView() {}

    //! Estimate database size (0 if not implemented)
    virtual size_t EstimateSize() const { return 0; }
};

```
Cinsview 类作为接口类，有很多具体实现子类:

                                                       CCoinsView
                                                     /          \
                                                    /            \
                                        CCoinsViewDB            CCoinsViewBacked
                                                                 /          |     \               
                                                                /           |      \
                                                               /            |       \
                                         CCoinsViewErrorCatcher   CCoinsViewMemPool   CCoinsViewCache

`CoinsViewDB`类主要服务于从Bitcoin数据目录下的*chainstate*子目录下保存和读取存盘的UTXO集合:
```c++
class CCoinsViewDB final : public CCoinsView
{
protected:
    CDBWrapper db;
public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;

    //! Attempt to update from an older database format. Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const override;
};
```
此类只有一个全局实例，在`validation.cpp`中定义:
```c++
std::unique_ptr<CCoinsViewDB> pcoinsdbview;
```
在init.cpp中进程启动时， 会对改对象进行初始化:
```c++
  pcoinsdbview.reset(new CCoinsViewDB(nCoinDBCache, false, fReset || fReindexChainState));
```
类`CCoinsViewBacked`本身没什么实际用处， 主要是作为多个Coinview层级之间的转接层， 它的数据成员 CCoinView *base 指向的就是后端即parent view , 如果某个coinsviewBacked的子类没有覆盖接口类CCoinsView 中的方法， 就会调用base指向的后端相应的方法。
```c++
class CCoinsViewBacked : public CCoinsView
{
protected:
    CCoinsView *base;

public:
    CCoinsViewBacked(CCoinsView *viewIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    void SetBackend(CCoinsView &viewIn);
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;
    size_t EstimateSize() const override;
};

CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }
bool CCoinsViewBacked::GetCoin(const COutPoint &outpoint, Coin &coin) const { return base->GetCoin(outpoint, coin); }
bool CCoinsViewBacked::HaveCoin(const COutPoint &outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
std::vector<uint256> CCoinsViewBacked::GetHeadBlocks() const { return base->GetHeadBlocks(); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) { return base->BatchWrite(mapCoins, hashBlock); }
CCoinsViewCursor *CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }
```
`CCoinsViewErrorCatcher` ,  `CCoinsViewMemPool` , ` CCoinsViewCache`  三个定制实现在初始化时需要指定parent view,所以要继承于CCoinsViewBacked类。
coinsviewErrorCatcher 主要用途是包装对数据库读取做错误处理，后端是全局的磁盘实现pcoinsdbview。 

```c++
class CCoinsViewErrorCatcher final : public CCoinsViewBacked
{
public:
    explicit CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override {
        try {
            return CCoinsViewBacked::GetCoin(outpoint, coin);
        } catch(const std::runtime_error& e) {
            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            abort();
        }
    }
};
```
启动时的初始化代码:
```
//init.cpp  AppInitMain()
pcoinscatcher.reset(new CCoinsViewErrorCatcher(pcoinsdbview.get()));
```
`CCoinsViewCache` 类是一个内存缓存的实现，内部使用hashmap 存储了某个outpoint 到` Coin `对象的映射，有一个全局实例`pcoinsTip` , 指向atctiveChain 的utxo，后端是磁盘实现`CCoinsViewDB`对象pcoinsdbview。

```c++
class CCoinsViewCache : public CCoinsViewBacked
{
protected:
    /**
     * Make mutable so that we can "fill the cache" even from Get-methods
     * declared as "const".  
     */
    mutable uint256 hashBlock;
    mutable CCoinsMap cacheCoins;

    /* Cached dynamic memory usage for the inner Coin objects. */
    mutable size_t cachedCoinsUsage;
    ...
    ...
}
``` 
启动时的初始化代码:
```
//init.cpp  AppInitMain()
pcoinsTip.reset(new CCoinsViewCache(pcoinscatcher.get()));
```
它的内部hashmap使用了定制的hash 方法siphash, 没有使用默认的std::hash方法(不是加密学安全的hash)， 估计是防止hash的key冲突,:
```c++
typedef std::unordered_map<COutPoint, CCoinsCacheEntry, SaltedOutpointHasher> CCoinsMap;

class SaltedOutpointHasher
{
private:
    /** Salt */
    const uint64_t k0, k1;

public:
    SaltedOutpointHasher();

    size_t operator()(const COutPoint& id) const {
        return SipHashUint256Extra(k0, k1, id.hash, id.n);
    }
};
```
这篇文章介绍了表示UTXO的相关表示的数据结构，下一篇文章将会UTXO的标记以及保存。

***
本文由 `Copernicus团队` 喻建写作，转载无需授权


