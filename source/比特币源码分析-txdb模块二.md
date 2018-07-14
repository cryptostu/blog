---
title: 比特币源码分析-txdb模块(二)
date: 2018-02-26 17:43:01
tags:
---
_本文主要从整体逻辑方面，抽象 txdb 模块的代码构建逻辑。_

首先 ` txdb ` 模块主要是用来实现 `block` 和 `utxo` 两个模块的落盘逻辑，所以我们将分为两个大的部分，来对其逻辑一一梳理。

## 原始数据块
首先，我们通过网络接收到原始块，进行块文件存储。

## 访问块数据文件
块文件通过以下方式访问：

* **CDiskTxPos**：一个 struct，`CDiskTxPos` 继承 `CDiskBlockPos`，`CDiskBlockPos`主要有两个参数 `nFile ` 和 ` nPos`, 指向一个块在磁盘上的位置的指针（一个文件号和偏移量）:

```c++
struct CDiskTxPos : public CDiskBlockPos {
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(*(CDiskBlockPos *)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos &blockIn, unsigned int nTxOffsetIn)
        : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {}

    CDiskTxPos() { SetNull(); }

    void SetNull() {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};
```
* **CBlockFileInfo** ：该函数用于执行如下任务：
	* 确定新块是否适合当前文件或需要创建新文件
	* 按块和撤消文件计算总的磁盘使用率
	* 遍历块文件并找到可修剪的文件

_数据库条目跟踪每个块文件已经有多少个字节使用，它有多少块，高度的范围是存在的以及日期的范围。_

## Block index

块索引保存所有已知块的元数据，包括块在磁盘上的存储位置。

对于存储在磁盘上的已知块，区块链是按照树状结构来描述其基于主链的众多分支结构（可能有的分支会很小），从根部的生成区块开始，每个区块可能有多个候选区块作为下一个区块。 blockindex 可能有多个 pprev 指向它，但是它们中至多有一个可以是当前活动分支的一部分。

实际上，LevelDB 的块索引是通过 txdb.h 中定义的  `CBlockTreeDB `包装类来访问的。 请注意，不同的节点会有略微不同的块树， 重要的是看他们是否认同所在的主链。

存储在数据库中的块在内存中表示为 `CBlockIndex` 对象。 这种类型的对象首先在收到 header 后被创建; 代码不会等待收到完整的块。 当通过网络接收到 header时，它们使用 stream 的方式被传输到一个 `CBlockHeaders` 矢量中，然后对其进行检查。 检出的每个header 都会导致创建一个新的`CBlockIndex`，并将其存储到数据库中。

**block index有两个重要的变量**

1. **nTx**：这个块的交易数量。nTx > 0 表示该块的状态至少为
 VALID_TRANSACTIONS。
2. **nChainTx**：包括此块在内的链中的交易数量，当且仅当此块及其所有父项的交易可用时，才会设置此值。

因此，`nChainTx> 0` 是一个 VALID_TRANSACTIONS 链的简写。 注意，这个信息不能通过块状态枚举来获得。 也就是说，VALID_TRANSACTIONS 只意味着它的父母是 TREE，而 VALID_CHAIN 意味着父母也是 CHAIN。 因此，从某种意义上来说，表达式（nChainTx！= 0）是可以被称为 “VALID_nChainTx = 3.5”的状态的缩写 - 因为它比VALID_TRANSACTIONS更多但是小于VALID_CHAIN。

注意：**nChainTx只存储在内存中; 数据库中没有对应的条目**。

```c++
class CBlockIndex {
public:
    void SetNull() {
       ...
    }
    CBlockIndex() { SetNull(); }
    CBlockIndex(const CBlockHeader &block) {
        ...
    }
    CDiskBlockPos GetBlockPos() const {
        ...
    }
    CDiskBlockPos GetUndoPos() const {
        ...
    }
    CBlockHeader GetBlockHeader() const {
        ...
    }
    uint256 GetBlockHash() const { return *phashBlock; }
    int64_t GetBlockTime() const { return (int64_t)nTime; }
    int64_t GetBlockTimeMax() const { return (int64_t)nTimeMax; }
    enum { nMedianTimeSpan = 11 };
    int64_t GetMedianTimePast() const {
        ...
    }
    std::string ToString() const {
       ...
    }
    //! Check whether this block index entry is valid up to the passed validity
    //! level.
    bool IsValid(enum BlockStatus nUpTo = BLOCK_VALID_TRANSACTIONS) const {
        ...
    }
    //! Raise the validity level of this block index entry.
    //! Returns true if the validity was changed.
    bool RaiseValidity(enum BlockStatus nUpTo) {
        ...
    }
    //! Build the skiplist pointer for this entry.
    void BuildSkip();
    //! Efficiently find an ancestor of this block.
    CBlockIndex *GetAncestor(int height);
    const CBlockIndex *GetAncestor(int height) const;
};
```

在启动时，`LoadBlockIndexGuts` 将整个数据库加载到内存中，这只需要几秒钟。

```c++
bool CBlockTreeDB::LoadBlockIndexGuts(
    std::function<CBlockIndex *(const uint256 &)> insertBlockIndex) {
    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));
    // Load mapBlockIndex
    while (pcursor->Valid()) {
       ...
    }
    return true;
}
```
## mapBlockIndex (map<block_hash, CBlockIndex*>)

mapBlockIndex 包含所有已知的块（“块”-->“块索引”）。上面我们提到，由于在收到 header 时就创建了块索引并将其存储在 LevelDB 中，因此在块映射中可能没有收到完整块的块索引，更不用说将其存储到磁盘了。

mapBlockIndex 是没有排序的。只要把它想象成你的块块哈希（ LevelDB）在内存中。

mapBlockIndex 是从 LoadBlockIndexGuts 中的数据库初始化的，LoadBlockIndexGuts 在启动的时运行。此后，无论何时通过网络接收到新块，都会更新。

mapBlockIndex 只会增长，它永远不会缩小。 （还要注意，块索引的 LevelDB 包装器不包含从数据库中删除块的功能 - 它的写入函数（`WriteBatchSync`）只写入数据库。相比之下，chainstate 包装器的写入功能（`BatchWrite`）可以写入和删除。 

块（ 'b' 键）被加载到全局  mapBlockIndex 变量中。 mapBlockIndex 是一个unordered_map，它为整个块树中的每个块保存 CBlockIndex。

```c++
bool CBlockTreeDB::WriteBatchSync(
    const std::vector<std::pair<int, const CBlockFileInfo *>> &fileInfo,
    int nLastFile, const std::vector<const CBlockIndex *> &blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo *>>::const_iterator
             it = fileInfo.begin();
         it != fileInfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex *>::const_iterator it =
             blockinfo.begin();
         it != blockinfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()),
                    CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}
```
## block 状态

其中一个关键特征就是它的 **“验证状态”** 。
验证状态不仅会验证当前块，还会去验证其祖先块。
该块的状态是下面的其中一种：

```c++
enum BlockStatus : uint32_t {
    //未使用
    BLOCK_VALID_UNKNOWN = 0,
    //解析时版本正常，哈希声明满足PoW，1 <= vtx count <= max，时间戳不在将来
    BLOCK_VALID_HEADER = 1,
    //找到所有父标题，难度匹配，时间戳> =中位数前一个检查点。 意味着所有的父母至少也是TREE。
    BLOCK_VALID_TREE = 2,
    //只有第一个tx是coinbase，2 <= coinbase输入脚本长度<= 100，
    //交易有效，没有重复的txids，sigops，大小，merkle根。
    //意味着所有父母至少是TREE，但不一定是TRANSACTIONS。
    //当所有父块都有TRANSACTIONS时，CBlockIndex :: nChainTx将会被设置。
    BLOCK_VALID_TRANSACTIONS = 3,
    // 输出不会超支输入，没有双重花费，coinbase输出正常，
    // 没有不成熟的硬币，BIP30。
    // 意味着所有的父母也至少在链中。
    BLOCK_VALID_CHAIN = 4,
    // 脚本和签名确定。 意味着所有的父母也至少是脚本。
    BLOCK_VALID_SCRIPTS = 5,
};
```

## CDBWrapper

_CDBWrapper是一个leveldb的包装函数，无论utxo还是block，均通过它写入leveldb，具体参照下图:_

![dbWrapper](http://upload-images.jianshu.io/upload_images/6967649-a8e57c92f4ffbf50.jpeg?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

`CDBWrapper` 主要有如下参数:

* path -->系统中存储leveldb数据的位置
* nCacheSize -->配置各种leveldb缓存设置
* fMemory --> 如果为true，则使用leveldb的内存环境
* fWipe --> 如果为true，则删除所有现有数据
* obfuscate --> 如果为true，则通过简单的XOR存储数据。 如果为false，则与零字节数组进行异或运算

```c++
class CDBWrapper {
public:
    CDBWrapper(const boost::filesystem::path &path, size_t nCacheSize,
               bool fMemory = false, bool fWipe = false,
               bool obfuscate = false);
    ~CDBWrapper();
};
```

## UTXO

访问 UTXO 数据库比块索引复杂得多。 这是因为它的性能对比特币系统的整体性能至关重要。 块索引对于性能来说并不是很关键，因为只有几十万个块，在好的硬件上运行的节点可以在几秒钟内检索并滚动（而且不需要经常这样做）。在UTXO数据库中有数百万个coins，并且必须对每个进入mempool或包含在块中的每个输入的输入进行检查和修改。

在 `init.cpp`文件的 1941-1946，我们会发现，utxo数据库在这里被初始化。

```c++
pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex || fReindexChainState);
pcoinscatcher = new CCoinsViewErrorCatcher(pcoinsdbview);
pcoinsTip = new CCoinsViewCache(pcoinscatcher);
```

上述代码首先初始化一个`CoinsViewDB`，它有从LevelDB中加载 coin 的方法。
接下来，初始化pCoinsTip，它是代表活动链状态的高速缓存，并由数据库视图支持。

_`pCoinsTip`保存对应于活动链的提示的 UTXO 集合, 检索/刷新到数据库视图。_

**`coins.cpp` 中的 `FetchCoins` 函数演示了代码如何使用缓存与数据库：**

```c++
1 CCoinsMap::iterator it = cacheCoins.find(outpoint);
2   if (it != cacheCoins.end()) {
3       return it; }
4    Coin tmp;
5    if (!base->GetCoin(outpoint, tmp)) {
6       return cacheCoins.end(); }
7    CCoinsMap::iterator ret = cacheCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::forward_as_tuple(std::move(tmp))).first;
```

1.  首先，代码在缓存中搜索给定交易ID的硬币 （第1行）
2.  如果找到，它返回“提取”的硬币 （2-3行）
3.  如果不是，则搜索数据库 （第5行）
4.  如果在数据库中找到，它会更新缓存（第7行）

##CCoinsViewDBCursor

`CCoinsViewDBCursor`继承自`CCoinsViewCursor`，专门用来迭代`CCoinsViewDB`:

```c++
class CCoinsViewDBCursor : public CCoinsViewCursor {
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(COutPoint &key) const;
    bool GetValue(Coin &coin) const;
    unsigned int GetValueSize() const;

    bool Valid() const;
    void Next();
private:
    CCoinsViewDBCursor(CDBIterator *pcursorIn, const uint256 &hashBlockIn)
        : CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;
    friend class CCoinsViewDB;
};
```

## CCoinsViewDB

`CCoinsViewDB` 继承自 `CCoinsView`，CCoinsView 由 coin 数据库备份（chainstate /），主要与 leveldb 进行交互。它会根据 ` chainstate` 在 LevelDB 设置的 UTXO, 检索 coins 并且 flush 到 LevelDB 的变化: 

```c++
class CCoinsViewDB : public CCoinsView {
protected:
    CDBWrapper db;
public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;
    //! Attempt to update from an older database format.
    //! Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const override;
};
```

## CoinEntry

`CoinEntry`是一个基础结构，服务于`CCoinsViewDB`:

```c++
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
    template <typename Stream> void Unserialize(Stream &s) {
        s >> key;
        s >> outpoint->hash;
        s >> VARINT(outpoint->n);
    }
};
```

## 引用

* 源码：[bitcoin-abc](https://github.com/Bitcoin-ABC/bitcoin-abc)
* 版本号：v0.16.0

***
本文由 `Copernicus 团队 冉小龙` 编写，转载无需授权。


