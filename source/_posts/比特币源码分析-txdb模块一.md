---
title: '比特币源码分析:txdb模块(一)'
date: 2018-02-25 11:33:28
tags:
---
_本小节主要介绍 txdb 以及其所引用到的代码中一些常量所表示的含义_
在 `txdb.cpp`中，我们能够看到其定义了很多 char 类型的常量:

```c++
static const char DB_COIN = 'C';
static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';
```
它们所代表的具体意思如下所示：

## Block 模块（Key-value pairs）

在LevelDB中，使用的键/值对解释如下：

1. 'b' + 32 字节的 block hash -> 记录块索引，每个记录存储:
	* 块头（block header）
	* 高度（height）
	* 交易的数量
	* 这个块在多大程度上被验证
	* 块数据被存储在哪个文件中
	* undo data 被存储在哪个文件中
2. 'f' + 4 字节的文件编号 -> 记录文件信息。每个记录存储：
	* 存储在具有该编号的块文件中的块的数量
	* 具有该编号的块文件的大小（$ DATADIR / blocks / blkNNNNN.dat）
	* 具有该编号的撤销文件的大小（$ DATADIR / blocks / revNNNNN.dat）
	* 使用该编号存储在块文件中的块的最低和最高高度
	* 使用该编号存储在块文件中的块的最小和最大时间戳
3. 'l' - > 4个字节的文件号：使用的最后一个块文件号。
4. 'R' - > 1字节布尔值（如果为“1”）：是否处于重新索引过程中。
5. 'F'+ 1个字节的标志名长度+标志名字符串 - > 1个字节布尔型（'1'为真，'0'为假）：可以打开或关闭的各种标志。 目前定义的标志是  'txindex'：是否启用事务索引。
6. 't'+ 32字节的交易 hash - >记录交易索引。 这些是可选的，只有当'txindex'被启用时才存在。 每个记录存储：
	 * 交易存储在哪个块文件号码中
 	 * 哪个文件中的交易所属的块被抵消存储在
 	 * 从该块的开始到该交易本身被存储的位置的偏移量


## utxo 模块（Key-value pairs）

1. 'c'+ 32字节的交易hash - >记录该交易未花费交易输出。 这些记录仅对至少有一个未使用输出的事务处理。 每个记录存储：
	* 交易的版本。
	* 交易是否是一个coinbase或没有。
	* 哪个高度块包含交易。
	* 该交易的哪些输出未使用。
	* scriptPubKey和那些未使用输出的数量。

2. 'B' - > 32字节block hash：记录UTXO是在那个block下产生的。

##  `txdb.h`中的其他定义
在 `txdb.h`文件中，我们还能够看到如下定义，它们所表示的含义如下：
```c++
//在flush时，会额外补偿这么多的 memory peak
static constexpr int DB_PEAK_USAGE_FACTOR = 2;
//如果当前可用空间在这个范围之内的话，则无需定期刷新。
static constexpr int MAX_BLOCK_COINSDB_USAGE = 200 * DB_PEAK_USAGE_FACTOR;
//如果少于这个空间仍然可用，会定期刷新
static constexpr int MIN_BLOCK_COINSDB_USAGE = 50 * DB_PEAK_USAGE_FACTOR;
//DB Cache 的默认大小
static const int64_t nDefaultDbCache = 450;
//DB Cache的最大值
static const int64_t nMaxDbCache = sizeof(void *) > 4 ? 16384 : 1024;
//DB Cache 的最小值
static const int64_t nMinDbCache = 4;
//如果没有txIndex的话，内存最大分配给block tree DB的空间。
static const int64_t nMaxBlockDBCache = 2;
//如果有 txIndex 的话，内存最大分配给block tree DB的空间。
//与UTXO数据库不同，对于leveldb缓存创建的txindex方案
static const int64_t nMaxBlockDBAndTxIndexCache = 1024;
//内存最大分配给coins DB的缓存大小
static const int64_t nMaxCoinsDBCache = 8;
```

## `dbwrapper.h`中的定义
在 `dbwrapper.h` 文件的 `class CDBWrapper` 下，定义了在操作`leveldb`时的一些选项，其具体含义如下所示：

```c++
//该数据库使用自定义环境（在默认环境情况下,可以是nullptr）
leveldb::Env *penv;
//数据库使用选项
leveldb::Options options;
//从数据库读取时使用的选项
leveldb::ReadOptions readoptions;
//迭代数据库的值时使用的选项
leveldb::ReadOptions iteroptions;
//写入数据库时使用的选项
leveldb::WriteOptions writeoptions;
//同步写入数据库时使用的选项
leveldb::WriteOptions syncoptions;
//数据库本身
leveldb::DB *pdb;
```

## 其它常量和枚举的定义
在 `chain.h` 的 CBlockFileInfo 下，有如下常量：
```c++
class CBlockFileInfo {
public:
    //文件中存储的块的数量
    unsigned int nBlocks;
    //块文件使用的字节数
    unsigned int nSize;
    //撤消文件需要使用的字节数
    unsigned int nUndoSize;
    //文件中块的最低高度
    unsigned int nHeightFirst;
    //文件中块的最高高度
    unsigned int nHeightLast;
    //文件中最早的块
    uint64_t nTimeFirst;
    //文件中最新的块的时间
    uint64_t nTimeLast;
```

在 `chain.h` 的 BlockStatus 文件下，列举了一些状态，用来标识 block的状态：

```c++
enum BlockStatus : uint32_t {
    //未使用
    BLOCK_VALID_UNKNOWN = 0,
    //解析正确、版本正确并且 hash 满足声明 PoW，1 <= vtx count <= max，时间戳正确。
    BLOCK_VALID_HEADER = 1,
    //找到所有父标题，难度匹配，时间戳> =中位数前一个检查点。意味着所有的父母至少也是TREE。
    BLOCK_VALID_TREE = 2,
    // 只有第一个 tx 是 coinbase，2 <= coinbase输入脚本长度<= 100，
    //交易有效，没有重复的txids，sigops，大小，merkle根。
    //所有父母至少是TREE，但不一定是TRANSACTIONS。
    //当所有父块都有TRANSACTIONS时，CBlockIndex :: nChainTx将被设置。
    BLOCK_VALID_TRANSACTIONS = 3,
    //输出不会超支输入，没有双重花费，coinbase输出正常，
    //没有不成熟的硬币，BIP30。
    //所有的父母也至少包含在链中。
    BLOCK_VALID_CHAIN = 4,
    //脚本和签名确定。意味着所有的父母也至少是脚本。
    BLOCK_VALID_SCRIPTS = 5,
    // 所有的有效位。
    BLOCK_VALID_MASK = BLOCK_VALID_HEADER | BLOCK_VALID_TREE |
                       BLOCK_VALID_TRANSACTIONS | BLOCK_VALID_CHAIN |
                       BLOCK_VALID_SCRIPTS,
    //blk * .dat中的完整块
    BLOCK_HAVE_DATA = 8,
    //撤销rev * .dat中的可用数据
    BLOCK_HAVE_UNDO = 16,
    BLOCK_HAVE_MASK = BLOCK_HAVE_DATA | BLOCK_HAVE_UNDO,
    //上次达到有效性的阶段失败
    BLOCK_FAILED_VALID = 32,
    //从失败块下降
    BLOCK_FAILED_CHILD = 64,
    BLOCK_FAILED_MASK = BLOCK_FAILED_VALID | BLOCK_FAILED_CHILD,
};
```

## 引用
源码：bitcoin-abc:[https://github.com/Bitcoin-ABC/bitcoin-abc](https://github.com/Bitcoin-ABC/bitcoin-abc)  
版本号：v0.16.0

****
本文由`Copernicus 团对 冉小龙`分析编写，转载无需授权。


