---
title: 译 block header
date: 2018-03-12 21:07:06
tags:
---
_block headers 以80字节的格式进行序列化，然后作为比特币工作量验证算法的一部分进行哈希处理，使序列化头部格式成为共识规则的一部分。_

| bytes|      name   |  数据类型 |描述|
| ---- | ----------- |--------- |---|
|  4   |     version   | int32_t  | block version 指示要遵循哪一组块验证规则|
|  32   |   previous block header hash  | char[32] |SHA256（SHA256()）hash，以前面 block 的 header 的内部字节顺序排列。这确保在不改变该块的头部的情况下不能改变先前的块。|
|  32   |     merkle root hash   | char[32]   |SHA256（SHA256()）是按照内部字节排序的hash。 merkle root 源自该块中包含的所有交易的hash值，确保在不修改头部的情况下不会修改这些交易。|
|  4   |   time  | uint32_t |块时间是矿工开始散列头部时的Unix纪元时间（根据矿工）。 必须严格大于前11个block的平均时间。 根据其时钟，全节点将不会接受超过两个小时的headers。|
|  4   |     nBits    | uint32_t  | 此块的header hash 的目标阈值的编码版本必须小于或等于。|
|  4   |   nonce  | uint32_t |任意数量的矿工都可以修改头部 hash 来确保能够产生小于或等于目标阈值的hash。 如果所有32位值都经过测试，则可以更新时间或更改coinbase交易并更新merkle根。|

**哈希按内部字节顺序排列; 其他值都是小端顺序。**

_block header的消息示例如下：_

```
02000000 ........................... Block version: 2

b6ff0b1b1680a2862a30ca44d346d9e8
910d334beb48ca0c0000000000000000 ... Hash of previous block's header
9d10aa52ee949386ca9385695f04ede2
70dda20810decd12bc9b048aaab31471 ... Merkle root

24d95a54 ........................... Unix time: 1415239972
30c31b18 ........................... Target: 0x1bc330 * 256**(0x18-3)
fe9f0864 ........................... Nonce
```

## Block Versions

* Version 1：在创世区块中被引入(January 2009)
* Version 2：在Bitcoin Core 0.7.0（2012年9月）中通过软分叉被引入。如BIP34所述，有效的 version2 block 需要 [block height parameter in the coinbase](https://bitcoin.org/en/developer-reference#term-coinbase-block-height "The current block's height encoded into the first bytes of the coinbase field")。 在BIP34中还描述了拒绝某些块的规则; 根据这些规则，Bitcoin Core 0.7.0及更高版本在 block height 为224,412 处开始拒绝在 coinbase 中没有 version2 的 block height，并在块高度为 227,930 的三周后开始拒绝新生成的 version1 的块。
* version3：在Bitcoin Core 0.10.0（2015年2月）中通过软分叉被引入。当 fork 达到全面执行（2015年7月）时，它需要严格按照 BIP66 中所描述的对新块中的所有ECDSA签名进行DER编码。 自从Bitcoin Core 0.8.0（2012年2月）以来，不使用严格DER编码的交易是非标准的。
* version4：在 BIP65 中指定并在 Bitcoin Core 0.11.2（2015年11月）中引入的区块，通过软分叉开始启动（2015年12月）。这些块现在支持该BIP中描述的新`OP_CHECKLOCKTIMEVERIFY`操作码。

用于 version2、3和4 升级的机制通常称为IsSuperMajority()，该功能添加到Bitcoin Core 中以管理这些软分支更改。有关此方法的完整说明，请参阅[BIP34](https://github.com/bitcoin/bips/blob/master/bip-0034.mediawiki)。

### Merkle Trees

merkle root 是使用此块中所有交易的TXID构建的，但首先需要按照共识规则的要求排列TXID：

* coinbase 交易的 TXID 总是排在第一位。
* 此块中的任何输入都可以使用同时出现在此块中的输出（假设花费是有效的）。但是，对应于输出的TXID必须放置在与输入对应的TXID之前的某个点。 这可以确保整个区块链的交易在输入之前都有与之对应的输出。

如果一个块只有一个 coinbase 交易，coinbase TXID将会被用作merkle root 的hash。

如果一个块只有一个coinbase交易和一个其他交易，那么这两个交易的TXID按顺序排列，连接成64个原始字节，然后SHA256（SHA256()）hash在一起最终形成 merkle root。

如果一个块有三个或更多的交易，则会形成中间的Merkle树行。 TXID按顺序排列并配对，从coinbase交易的TXID开始。 每个对连接在一起作为64个原始字节和SHA256（SHA256()）hash形成第二行hash。 如果有一个奇数（非偶数）的TXID，则将最后一个TXID与其自身的副本连接并进行hash。 如果第二行中有两个以上的hash，则重复该过程以创建第三行（并且，如有必要，可以进一步重复以创建附加行）。 一旦获得一行只有两个hash值，这些hash值被连接并哈希来产生merkle root。

上面逻辑有点绕，我们通过一张图来直观感受一下：
![merlke.png](https://upload-images.jianshu.io/upload_images/6967649-a51882dd398c737c.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

串联时，TXID和中间hash总是以内部字节顺序排列，并且当它放入block header时，生成的merkle root也按照内部字节顺序排列。

### Target nBits

target threshold 是一个256位无符号整数，header hash 必须等于或低于该header 才能成为块链的有效部分。 然而，header 字段 nBits仅提供32位空间，因此目标号码使用一些称为“紧凑”的精确格式，其工作方式类似于科学记数法的基本256版本：

![nBits.png](https://upload-images.jianshu.io/upload_images/6967649-146e72c7ee0a483d.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

作为一个 base-256 的数字，nBits可以像字节一样快速地解析，这与解析10进制科学记数法中的小数相同：

![serailize.png](https://upload-images.jianshu.io/upload_images/6967649-263832e23db5497a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

尽管 target threshold 应该是一个无符号整数，但原始 nBits 实现继承了已签名数据类的属性，如果设置了有效数的高位，则target threshold为负值。 这是无用的 - header hash被视为无符号数，因此它永远不会等于或低于负目标阈值。 Bitcoin 以两种方式处理这个问题：

1. 在解析nBits时，Bitcoin 将负目标阈值转换为零目标，该header hash可以等于（理论上至少）。
2. 当为nBits创建一个值时，Bitcoin 会检查它是否会产生nBits，这会被解释为负值; 如果是这样，它将有效位数除以256，并将指数增加1以产生具有不同编码的相同数字。

难度1，允许的最小难度，在mainnet和当前testnet上由nBits值0x1d00ffff表示。 Regtest模式使用不同的难度值1-0x207fffff，uint32_max以下可以编码的最高值; 这允许在regtest模式下几乎立即构建块。

### Serialized Blocks

根据当前的共识规则，除非序列化大小小于或等于1 MB，否则块无效。下面介绍的所有字段均按序列化的大小计算

| bytes|      name   |  数据类型 |描述|
| ---- | ----------- |--------- |---|
|  80   |     block header   | block_header  | block header 部分中描述的格式。|
| Varies| txn_count|compactSize uint|此区块中的交易总数，包括coinbase交易。|
| Varies| txns|raw transaction|此块中的每笔交易都是原始交易格式。交易必须与他们的TXID出现在merkle树的第一行中相同的顺序出现在数据流中。|

在一个区块中的第一笔交易必须是一个coinbase交易，该交易应收集并消费本区块交易支付的任何交易费用。

block height低于6,930,000的所有 block 都有权获得新创建的比特币价值的区块补贴，这也应该用于coinbase交易。 （区块补贴从50比特币开始，每210,000块减半 - 大约每四年一次，截至2017年11月，为12.5比特币。）

交易费和区块补贴一起被称为区块奖励。如果它试图花费比块奖励可用的更多价值，则coinbase交易是无效的。

*** 
本文由copernicus 团队 冉小龙 翻译，转载无需授权！

