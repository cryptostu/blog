---
title: '[译]bip-0199:hash时间锁定合约交易'
date: 2018-03-11 18:06:57
tags:
---

## 摘要

该BIP主要描述了广义的 off-chain 合约谈判的脚本。

## 总结

hash 时间锁定合约（HTLC）是一种脚本，允许指定方（“卖方”）通过公开 hash 的原始信息来花费资金。 在退款情况下，它还允许第二方（“买方”）在超时达到后花费这笔资金。

该脚本采用以下形式：

```
OP_IF
     [HASHOP] <digest> OP_EQUALVERIFY OP_DUP OP_HASH160 <seller pubkey hash>            
OP_ELSE
     <num> [TIMEOUTOP] OP_DROP OP_DUP OP_HASH160 <buyer pubkey hash>
OP_ENDIF
OP_EQUALVERIFY
OP_CHECKSIG
```

* [HASHOP] 代表 OP_SHA256 或 OP_HASH160.
* [TIMEOUTOP] 代表 OP_CHECKSEQUENCEVERIFY 或 OP_CHECKLOCKTIMEVERIFY.

### 相互作用

* Victor（“买方”）和 Peggy（“卖方”）交换公共 hash 并在超时阈值前达成共识。 Peggy（“卖方”）提供了一个 hash 摘要, 双方现在都可以为HTLC构建脚本和P2SH地址。
* Victor（“买方”）将资金发送到P2SH地址。
* 或者：
	* 	Peggy（“卖方”）花费这笔资金时，将交易中的原始信息提供给了 Victor（“买方”）, 或者
	*  Victor（“买方”）在超时时间到达后恢复资金

Victor（“买方”）希望降低超时时间，来减少在	Peggy（“卖方”）不透露原始信息的情况下资金投入的时间。 Peggy（“卖方”）希望增加超时时间，来减少超时时间到达之前，他无法花费这些资金的风险，或者更糟糕的是，Peggy（“卖方”）的交易花费的资金在Victor（“买方”）之前没有进入区块链，但却向Victor（“买方”）告知了它的原始信息。
## 动机

在许多 off-chain 协议中，揭露秘密被用作解决机制的一部分。 在另一些情况下，秘密本身很有价值。 由于能够从不合作的交易对手那里收回资金，HTLC 交易是一种在区块链上交换金钱秘密的安全和便宜的方法，并且秘密拥有者必须在发生这种退款之前收到资金。

### 闪电网络

在闪电网络中，HTLC脚本用于在支付渠道之间执行原子交换。

Alice 构造 K 并通过 hash 产生 L，她将 HTLC 支付发送给 Bob 以获得 L 的原始信息.Bob 将 HTLC 支付发送给 Carol 以获得相同的原始信息和金额。 只有当 Alice 公开原始信息， K 才可能进行相应的价值交换，并且由于每一步都泄露了秘密，所有各方都得到补偿。 如果在任何时候有些参与方不合作，这个过程可以通过退款条件中止。

### 零知识应急支付

存在各种实际的零知识验证系统，可用于保证 hash preimage 派生有价值的信息。 举个例子，零知识证明可以用来证明一个 hash preimage 作为一个加密的数独谜题解决方案的解密hash。 

HTLC交易可用于无风险地交换这些解密 hash 以获得金钱，并且它们不需要大量的、昂贵的验证交易。

## 实现

[https://github.com/bitcoin/bitcoin/pull/7601](https://github.com/bitcoin/bitcoin/pull/7601)

## 版权

该文档是双重许可的BSD 3条款和Creative Commons CC0 1.0 Universal。

***

本文由 `copernicus 团队 冉小龙` 翻译，转载无需授权。