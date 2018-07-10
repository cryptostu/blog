---
title: 对BUIP039（extension point）的解释
date: 2018-02-07 09:20:00
tags:
---

>之前翻译过BUIP039[【译】BUIP039 通过 extension point 的升级方式](https://mp.weixin.qq.com/s/kuRABdcj95OACVRaBnMWzw)相关的内容，作者Amaury SECHET（Bitcoin ABC首席开发者）对于extension point在论坛中进行了解释。

## 前言
上面的提案过于抽象，是因为不想将该想法与具体的实现技术联系起来。为了使事情更清晰，我解释一下对于指定OP_NOPs操作码的工作机制。
比特币使用脚本机制去验证交易的合法性，该脚本是非图灵完备的智能合约。该脚本由大量的操作码组成，并且每个操作码做对应的事情，例如：检查签名的有效性。
该脚本语言有一些OP_NOPX的操作码，这里的X标识一个数字。可以重新设计这些操作码，用来给比特币增加一些新功能，例如：OP_NOP2被重新设置为OP_CHECKLOCKTIMEVERIFY；当遇到这个操作码时，旧节点不做任何事情，但是新节点将检查锁定时间。

## 软分叉、硬分叉、extension point 
今天，操作码可以通过软分叉来重新设计，在新的操作码的操作上增加一些限制。例如：新的操作码不可以修改栈，不可以产生结果，并且对于验证一个交易只能有条件的失败。
通过软分叉升级有点问题：旧节点被欺骗为他们有效的验证了区块，但实际上并不是这样。另外，软分叉增加了设计约束，并产生了技术债。另一方面，硬分叉强迫每个人立即升级，并且旧的节点不可以再进行验证，因为它们无法理解它们收到的交易和区块。两者都不是理想的升级方式。
我建议定义所有的OP_NOPs作为拓展点。每个拓展点是预先商量好的，它将被用来在在协议中添加一些新功能。一旦某个OP_NOPs被定义为拓展点，协议通过该拓展点来分配一个特定的功能，从而进行拓展。
支持该功能的新节点，可以使用指定的OP_NOP来接收和验证交易。另一方面，旧的节点看到它们所不知道的拓展点的使用，就会知道它们不理解的功能在网络中被触发。此时，它们有多种执行方案：

*  它们可以将拓展点作为软分叉来对待，然后跳过使用了该拓展点的脚本签名检查。
*  它们可以选择等待AD区块，看使用该拓展点的块是否被网络中的多数节点所接受；如果是，则跳过这些特殊交易的签名检查。
*  它们可以选择不跟随当前使用拓展点的链，并等待操作码的升级，这种方式将拓展点作为硬分叉来对待。
如果拓展点被绑定上AD参数，通过为AD选择合适的值，上述3中方式都可以实现。

## 总结
简而言之：当前节点对于链的合法性有两种状态：__有效__或者__无效__。使用拓展点，将获取3中：__有效__，__无效__，__一些我不理解的事情发生__。作为软分叉，旧节点将以减少安全的方式在网路中运行，然而，与软分叉相反的是，这些节点直到自己正运行在安全环境低下的场景，并且没有被欺骗。除此之外，如果节点愿意，它们可以拒绝接收采用新功能的区块。
   
## 引用
[【译】BUIP039 通过 extension point 的升级方式](https://mp.weixin.qq.com/s/kuRABdcj95OACVRaBnMWzw)
[BUIP039: Upgrade via extension point -Discussion in 'Bitcoin Unlimited' started by deadalnix](https://bitco.in/forum/threads/buip039-upgrade-via-extension-point.1641/?nsukey=vaxHBeKc5u36XnHqvvWOYRLBT1JLAeJizXghPtCIUvPot6hnaAVYSd%2B96rCv6sFdoC86frI2%2F4Qu0p7WMwl645mj4oGrlKOZOzOdFGNX3LU2xnj4Fa3zxS3VM6QjEqh2dXEB8UR%2BeD7lVbUS3Yc5sym%2BXDCa%2Bw233Caix1pJvgc%2FvHqIneKdug2clMZ1RdmLxhbPRfyhfOhabJ3MXWygOQ%3D%3D)
***
本文由 `Copernicus团队 姚永芯`翻译整理，转载无需授权。

