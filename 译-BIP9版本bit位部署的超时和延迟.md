---
title: '[译]BIP9版本bit位部署的超时和延迟'
date: 2018-02-20 18:33:08
tags:
---
## 综述
本提案目标是：改变区块版本中 `version`字段的含义，允许同时部署多个向后兼容的更改(或称软分叉)。该功能的实现依赖于将 `version`字段解释为bit vector,每个bit位可以用来跟踪一个独立的更改。在每个目标周期统计部署该bit位的区块的个数，一旦部署达成共识或超时(失败)，接下来有一个暂停期，之后该bit 位可以被以后新的规则变化重新使用。
## 动机
[BIP34](https://github.com/bitcoin/bips/blob/master/bip-0034.mediawiki)引入了一种不需要预先定义时间戳或区块高度来进行软分叉的机制，而是依赖于通过统计矿工的支持率：即在块头中通过高版本号进行标示。这种方式依赖于将版本字段作为一个整型进行比较，所以它每次支持部署单个的更改，要求协调各种提案，并且不允许永久拒绝该提案：只要一个软分叉没有完成部署，就不会安排未来的另一个软分叉。
另外，当一个新的共识规则达到95%的阈值之后，[BIP34](https://github.com/bitcoin/bips/blob/master/bip-0034.mediawiki)将会做整型比较(当nversion >= 2)，从有效的版本集合中移除2<sup>31</sup>+2(所有的负数，因为版本号被解释为一个有符号的整型，以及0和1)。这表明了这种方法的另一个缺点：每次升级都会永久限制允许使用的版本字段的集。在[BIP66](https://github.com/bitcoin/bips/blob/master/bip-0066.mediawiki)[BIP65](https://github.com/bitcoin/bips/blob/master/bip-0065.mediawiki)也使用了该方法，又从有效集合中移除了版本2和3。如进一步所示，这是没有必要的。
## 规范
1. 每个软分叉的部署被它当前所采用的链参数指定(详细描述如下：)
>   1. `name`字段标识这个软分叉的简要描述，可以作为标识符来合理使用。对于在单个BIP中描述的部署，建议使用`bipN`作为它的名称，N标识对应的BIP号。
>   2. `bit`字段决定块的版本字段中，哪个bit位用将被用于通知某个软分叉将被锁定或激活。从{0,1,2,...28}中选择。
>   3. `starttime`字段定义了该bit位开始 起作用的最小时间(这里采用的是块的中位数时间MTP)
>   4. `timeout`字段定义了该bit为部署失败的时间。如果块的MTP时间 >= timeout并且该软分叉至今未锁定，那么这个部署将从这个块开始被认为失败。

## 选择指南
建议使用下面的指南作为一个软分叉的参数
>   1. `name`应该被设计为 在两个同时部署的软分叉之间不存在名字冲突。
>   2. `bit`应该被设计为在两个同时部署的软分叉之间不存在bit冲突。
>   3. `starttime`应该被设计为包含这个软分叉的软件发布后的大约一个月开始。这样就可以允许发布延迟，同时可以阻止意外运行预发布软件造成功能的触发。
>   4. `timeout`应该被设计为`starttime`之后的一年以后。

一个新的部署可以使用与原来部署相同的bit位，只要这个部署的`startTime`在原来部署的超时时间或激活时间之后，但是没有必要不鼓励这样做，并且如果非这样做，推荐有段休整期用来检测软件的BUG。
## 状态
对于每个软分叉，关联了一系列部署状态，如下所述：
>   1. `DEFINED`:是每个软分叉的第一个状态，每个部署的初始块都被定义为该状态。
>   2. `STARTED`: 接收的区块进入了部署阶段。(>=startTime)
>   3. `LOCKED_IN`: 在STARTED状态周期之后的每个周期，只要版本号中的部署达到阈值，标识下个周期进入LOCKED_IN。
>   4. `ACTIVE`:锁定周期之后的状态。
>   5. `FAILD`: 在block MTP > timeout ,但是没有到达LOCKED_IN状态，则进入FAILD状态。

## bit 标识
块头的版本字段被解释为一个32位的小端整型，并且在这个整数中bits作为1<< N的值，N是bit所在的位置索引。
在STARTED状态的区块的版本字段该bit位被设置为1，并且块的版本字段的高3位必须是001，所以版本字段的实际的范围是：[0x20000000 ... 0x3FFFFFFF].
由于BIP34，BIP65，BIP66的限制，仅有0x7FFFFFFB些版本号的值可以使用。这限制最多可以有30个独立的部署。由于最高3比特位的限制，我们最多可以从本提案使用29个bit位，并且支持未来两种不同的升级(010,011).当一个块的版本号的高位不含有001，为了部署，就将所有的bits位都视为0.
矿工应该在LOCKED_IN阶段继续设置块版本字段的bit位，尽管这对共识规则没有影响。
## 新的共识规则
当处于ACTIVE 状态时，每个块被强制执行软分叉所包含的新的共识规则。

## 状态转变
![状态转换图][1]
每个部署的初始块的状态为：`DEFINED`
```
State GetStateForBlock(block) {
if (block.height == 0) {
    return DEFINED;
}
```
    
在每个目标周期所有的块有相同的状态。这意味着如果 block1/2016 == block2/2016,则他们在每个部署中一定有相同的状态。
```
if ((block.height % 2016) != 0) {
    return GetStateForBlock(block.parent);
}
        
```

否则，接下来的状态依赖于先前的状态。
 `switch (GetStateForBlock(GetAncestorAtHeight(block, block.height - 2016))) {`
 保持这个初始状态，直到区块的MTP时间 >= timeout/starttime. `GetMedianTimePast`获取一个块的MTP时间(包含这个块和它的10个祖先)。
```
case DEFINED:
    if (GetMedianTimePast(block.parent) >= timeout) {
        return FAILED;
    }
    if (GetMedianTimePast(block.parent) >= starttime) {
        return STARTED;
    }
    return DEFINED;

```
STARTED 状态周期之后，如果块的MTP时间>= timeout,则返回FAILED状态。如果块的时间没有通过timeout，则统计设置bit的区块个数，如果在一个目标周期内，设置在版本字段的部署达到阈值，将状态转换为LOCKED_IN.主链中阈值>= 1916(95% of 2016),测试链中阈值>=1512(75% of 2016).注意：优先转换到FAILED状态，否则可能接下来会有歧义。同一个bit位上，可能有两个不重叠的部署，其中一个转换到了锁定状态而另一个同时转换到了STARTED状态，这意味着该bit位的设置同时有两种需求。
注意：该区块的状态只依赖于它的祖先，而不依赖自身。
```
case STARTED: 
    if (GetMedianTimePast(block.parent) >= timeout) {
        return FAILED;
    }
    int count = 0;
    walk = block;
    for (i = 0; i < 2016; i++) {
        walk = walk.parent;
        if (walk.nVersion & 0xE0000000 == 0x20000000 && (walk.nVersion >> bit) & 1 == 1) {
            count++;
        }
    }
    if (count >= threshold) {
        return LOCKED_IN;
    }
    return STARTED;

```
一个`LOCKED_IN`周期之后，自动转换到`ACTIVE`状态。
```
case LOCKED_IN:
    return ACTIVE;

```
`ACTIVE`和`FAILED`都是终止状态，一旦达到后，该部署将持续保持这种状态。
```
case ACTIVE:
    return ACTIVE;
case FAILED:
    return FAILED;

```
实现：应该与所有的分叉保持相同的状态，当重组发生时，可能需要重新计算块的状态。
对于给定的块和部署的组合，完全由它的祖先在当前目标周期之前确定。可以实现一种以它的父区块为索引，高效，安全的缓存每2016个块的状态。

### 警告机制
为了支持升级警告，额外的`unknown upgrade`被跟踪。使用隐式的掩码即：`mask = (block.nVersion & ~expectedVersion ) != 0`,当一个不认识的bit位在版本字段中被设置，mask将为非0. 当未知的升级被检测到处于LOCKED_IN状态时，软件应该大声的警告即将到来的软分叉。在未知版本号处于ACTIVE状态时，应该更大声的警告。

### getblocktemplate 的变化
块模板请求对象被拓展包含一个新的条目
template request |
---|

 Key | Required | Type | Description |
 ----|---|---|---|
|rules | No | Array of Strings|list of supported softfork deployments, by name 

模板对象也被如下拓展
template |
---|

Key	|Required	|Type	|Description|
---|----|----|----|
rules|	Yes|	Array of Strings|	list of softfork deployments, by name, that are active state|
vbavailable|	Yes|	Object|	set of pending, supported softfork deployments; each uses the softfork name as the key, and the softfork bit as its value
vbrequired|	No|	Number|	bit mask of softfork deployment version bits the server requires enabled in submissions

模板的`version`字段被保留，通常用来暗示服务引用的部署。如果versionbits被使用，`version`必须在[0x20000000...0x3FFFFFFF]之间。矿工可以不使用指定的key，而在block的`version`字段中清空或设置bit位，只要矿工在模板的`vbavailable`列出并且(当设计清空时)不包含在`vbrequired`.
软分叉部署的名字列在`rules`字段，或在`vbavailable`中以`!`为前缀添加在keys中。不包含前缀时，GBT客户端可能认为该规则不会影响模板的使用。经典的例子是：以前有效的交易不会再认为有效，例如：BIPs 16，65，66，68，112和113.如果一个客户端不理解没有前缀的规则，它可能会使用未经修改的模板进行挖矿。另一方面，当使用前缀时，它标识块结构或创币交易有微妙的变化；例子是：BIP 34(它修改了coinbase的结构)和141(它修改了交易hash，并在创币交易中添加了一个承诺结构)。如果一个客户端不理解带`!`前缀的规则，一定不能用它来处理模板，并且一定不要尝试用它来进行挖矿，即使不对模板进行修改。

## 对未来变化的支持
上述描述机制的非常通用，并且对于未来的软分叉也是可能的。下面有一些想法可以考虑。
__Modified thresholds(修改阈值)__：1916这个阈值(基于BIP34 95%)不必永久保持，但是修改它时，对于警告系统的影响应该考虑。尤其是，不兼容的锁定阈值对于警告系统会有长期的影响，因为警告系统不能再依赖一个可永久检测的状态。
__Conflicting soft forks(软分叉冲突)__：在这方面，由于两个相互互斥的软分叉可能被提议。传统的做法是不制造一个同时实施两个软分叉的软件，但是这是一个保证至少有一方未实现的赌注。更好的方法将"软分叉X不能被锁定"编码，作为冲突软分叉的共识规则，允许软件同时支持双方，但是由于上述的编码，所以从来不会触发冲突。
__Multi-stage soft forks(多级软分叉)__：现在的软分叉通常被视为Boolean值：在块中从未激活到激活状态。或许在一些方面需要多级的软分叉，并且逐个启用额外的验证规则。通过解释一个bits集合作为一个整数，而不是仅仅作为一个独立的bit位，上述的机制可以适用这点。采用该机制将与警告机制兼容，因为对于递增的整数`(nVersion & ~nExpectedVersion) `将一直为非0
## 原理
超时失败机制允许重用一个bit位，即使软分叉从未被激活，很明显，新bit位引用了一个新的BIP。考虑到合理的开发和延迟部署情况，它被故意设计的非常粗糙。没有足够的失败建议导致提案设计有点不足。
软分叉结束的休闲期允许有一些客户端的BUG检查，并且给时间进行警告和软件升级。

## 引用
原文链接：https://github.com/bitcoin/bips/blob/master/bip-0009.mediawiki
[1]: https://github.com/bitcoin/bips/blob/master/bip-0009/states.png

***
本文由 `Copernicus团队 姚永芯`翻译整理，转载无需授权。


