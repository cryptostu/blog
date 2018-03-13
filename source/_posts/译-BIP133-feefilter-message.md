---
title: '[译]BIP133 feefilter message'
date: 2018-03-13 20:44:22
tags:
---

## 摘要

增加一个新的消息类型--“feefilter”，用于告知 peer 不要向该节点发送低于指定费率的交易的“inv”。

## 动机

为了防止未开采的低费用的攻击和一些垃圾邮件的交易，Bitcoin Core 0.12中引入了有限 mempool 的概念。同时引入了拒绝过滤器，以防止同一交易因为费用不足而被拒绝时重复请求的问题。 这些方法有助于保持节点上的资源利用率不受限制。
	
但是，这些方法的有效性存在限制。 拒绝过滤器在每个块之后被重置，这意味着在较长时间段内被调用的交易将被重新申请，并且没有办法阻止第一次请求交易。 此外，对于接受到mempool的每个事务，inv数据至少被发送一次或发送给每个 peer，并且没有机制知道发送给指定的 peer 的 inv 不会导致 getdata 请求，因为它代表交易费用太少。

在收到feefilter消息之后，节点可以在发送 inv 之前知道给定事务费率低于给定peer当前所需的最小值，这时候节点能预先得知，不用继续去将该事务的inv消息给该 peer。

## 规范

1. feefilter消息被定义为：pchCommand ==“feefilter”的返回值是int64_t的一个消息。
2. 一旦收到“feefilter”消息，节点将被允许（但不是必需）执行这笔过滤交易的交易，这些交易的价格低于feefillter消息中提供的费率，解释为每 kilobyte 为 satoshis。
3. feefilter 与交易过滤器相结合，因此如果 SPV 客户端要加载 bloom 过滤器并发送 feefilter 消息，则交易只有在它们通过两个过滤器后才会被执行。
4. 如果它存在，则从 mempool 消息生成的 Inv 也受到费用过滤器的限制。
5. 通过检查协议版本 >= 70013 来启用功能发现

## 注意事项

网络交易的传播效率不应受到这种变化的不利影响。 通常，节点的mempool不接受的事务不会被此节点执行，并且使用此消息实现的功能仅用于过滤这些事务。 可能会有少量的极端情况，其中节点的 mempool 最小费用实际上小于其它节点意识到的过滤器值，并且现在将会重新禁止这些值之间的费率交易。

如果设置了“-whitelistforcerelay”选项，则不会将 Feefilter 消息发送给已列入白名单的同伴。在这种情况下，即使交易不被接受，交易也会被转发。

由于它广播了关于其mempool分钟费用的标识信息，因此对于节点的匿名问题存在隐私担忧。 为了帮助改善这种担忧，实现量化具有少量随机性的过滤器值广播，此外，消息在个别随机分布的时间被广播给其他不同的节点。

如果一个节点使用 prioritisetransaction 接受其实际费用率可能低于节点的 mempool min费用的交易，则可能需要考虑 feefilter 禁用以确保其暴露于所有可能的txid。

## 向后兼容

在这种变化之后，老客户仍然完全兼容并且可以互操作。此外，实施此BIP的客户可以选择不发送任何费用过滤消息。

## 实现

[https://github.com/bitcoin/bitcoin/pull/7542](https://github.com/bitcoin/bitcoin/pull/7542)

## 版权

该文件置于公共领域。
## 引用
bip-0133:[https://github.com/bitcoin/bips/blob/master/bip-0133.mediawiki](https://github.com/bitcoin/bips/blob/master/bip-0133.mediawiki)

***
本文由 copernicus 团队 冉小龙 翻译，转载无需授权。



