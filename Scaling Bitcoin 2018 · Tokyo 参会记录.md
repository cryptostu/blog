# Scaling Bitcoin 2018 · Tokyo 参会记录
####作者：温龙

![2.png-427.3kB][1]
##摘要
本次Scaling Bitcoin会议主要包含8个sessions：1）Current State of Bitcoin：介绍对UTXO集合观察以及对链上隐私的思考；2）On-Chain Scaling：Core成员提出了修改挖矿奖励的想法、利用Bitcoin脚本构建Universal Turing Machine的项目、以及对Bitcoin进行扩容的Forward Block方案（值得注意的是这个方案也有对挖矿奖励规则进行修改的提议，提案来自Blockstream的cofounder）；3）Scaling Security：新构造的multi-signature用来缩减链的体积（Dan Boneh参与的工作）、利用Fraud Proof改进SPV轻节点安全性（V神参与的工作）、利用Accumulator构建新的结构替换Merkle Tree（Dan Boneh参与的工作）；4）Scriptless Scripts and Multi-party Channels：讨论闪电网络方面的进展以及改进闪电网络中的隐私性，值得注意的是Lightening Labs中对cutting-edge的密码构造2P-ECDSA的应用，可以用来简化脚本；5）Work-in-Progress Session：一些尚未成熟的工作的介绍，其中值得注意的是Zion钱包的工作，利用手机中的TPM、SecureEnclave等组件提升钱包的安全性；6）Architecture：OmniLedger协议通过分片提升tps（包含了对跨片交易处理的协议）、GHOSTDAG协议基于DAG重塑区块链底层数据结构；7）Lightning Network：探讨闪电网络中的rebalancing的问题、开发可重现结果Lightning Benchmark工具及如何激励payment channel watchtowers；8）Interoperability：探讨atomic swaps的进展、链下进行UTXO传递的statechain、利用payment channel构建Bitcoin Bridge的Niji项目、以及cryptocurrency-backed的通证发行方案。

目前文档中整理了1、2、3、5、6五个session的详细报告内容，关于闪电网络的4、7以及Interoperability的8没有整理，对这几部分了解不多。然而闪电网络中利用的2P-ECDSA或许值得关注。由于新东西很多，整理的session中相应报告也只是有了大体的了解，诸多细节尚不清晰，对于重点工作后续需仔细看下。

值得重点关注的几个内容：1）在On-Chain Scaling 中有两个Bitcoin Core的人都提到了修改Bitcoin的挖矿奖励的问题这部分或许跟进下；2）Scaling Security不部分，新构建的multi-signature scheme、基于Accumulator理念构建的旨在替换Merkle Tree的结果值得重点关注下【算是本次会议的一个亮点】；3）Architecture部分的OmniLedger值得关注下，是了解到的第一个认真处理跨片交易并给出解决方案的Sharding方案【初步感觉multi-signature scheme可以用来进一步改进这个方案，改进其中的BFT协议部分，尚未深入思考】；4）WIP部分：基于手机的安全硬件模块提升钱包的安全型的想法或许值得关注
##An Analysis of Dust in UTXO based Cryptocurrencies
Cristina Pérez-Solà, Sergi Delgado Segura, Guillermo Navarro-Arribas and Jordi Herrera (Universitat Autònoma de Barcelona)
论文参见：https://eprint.iacr.org/2018/513.pdf

主要工作是统计采用UTXO模型的各个币种中UTXO集合的相关信息，想要回答的问题是：1）有多少unspent output是值得花费的？2）全节点中有多少空间被用于存储not-worth-spending output？

Bitcoin Core开发者对dust output的定义是an output that costs more in fees to spend than the value of the output，通常没有经济动机去花费UTXO集合中的dust output。为了计算花费一个output所需的代价，既需要知道output的大小也需要知道相应的input的大小。由于无法提前知道input的大小，Bitcoin Core就用148个字节来作为input的粗略估计（根据最常见的P2PKH交易类型做出的估计）。

这篇工作中指出，dust output的这种定义在刻画不值得花费的output方面并不精确：1）根据交易类型的不同，input的大小会有不同；2）dust output的定义中同时考虑了output和input的大小，这项工作中，作者谈到“we claim that since the transaction containing the output is already in the blockchain, its size should not be taken into account when analyzing the dust problem (since it has already been paid).”基于这两点原因，给出了unprofitable output的定义，该定义与dust output基本相同，差别在于unprofitable output定义中仅考虑花费相应output所需的input的大小。根据交易类型的不同，有可能准确推断出input的大小也有可能无法准确估计，因此给出了两种度量unprofitable_low和unprofitable_est，前者考虑可能的最小的input大小，或者则根据链上数据的给出尽可能准确的input的大小。然后用这三种指标度量了Bitcoin、Bitcoin Cash和Litecoin的UTXO集合，在下图中以50k block为间距绘制了unproﬁtable outputs随着时间推移在不同的费率条件下在UTXO集合中的占比。一个很自然的观察是，unproﬁtable outputs总是在增加。根据论文统计分析，在Bitcoin中P2PKH交易贡献了所有的unproﬁtable outputs中的81%。另外Bitcoin Cash和Bitcoin在450k之前相同，所以可直接参考Bitcoin的数据。Litecoin图标的诡异之处是因为“67% of Litecoin’s UTXOs belong to the ﬁrst ﬁve months of the coin and a similar amount carries just one satoshi”。
![3.png-76kB][2]
总结下，在费率为100sat/byte时，35~45%的Bitcoin和Bitcoin Cash以及67%的Litecoin的UTXO为unproﬁtable outputs，也占据了全节点中UTXO集合同等比例的存储体积，然而这些output仅代表了各自币种中非常小的货币供应：0.01% for BTC & BCH，negligible for LTC。

几个结论：1）UTXO集合中存在较大比例的dust；2）当前的UTXO实现可以无限增长；3）UTXO集合越大，越不适合资源受限节点运行全节点；4）粉尘攻击可以用来将UTXO集合撑大。

应对策略？1）TXO commitment by Peter Todd；2）output consolidation when fees are low；3）good coin selection algorithm is important, specially for exchanges。
##How Much Privacy is Enough? Threats, Scaling, and Trade-offs in Blockchain Privacy Protocols
Bitcoin地址仅仅提供了化（Pseudonymity）而非匿名（Anonymity），通过交易分析，可以逆向地址的化名特性，相关工作参见下图中列出的工作。
![4.png-304.1kB][3]
讨论数字货币的隐私问题，首先需要考虑如何评估隐私？这跟1992年时候讨论Internet的隐私问题非常类似。不能仅根据经验主义进行评估，而需要进行一个思想实验，这就需要先了解一些真是的威胁。Google、Facebook、Mastercard、Target、Venmo等公司极大侵犯了个人隐私。
![5.png-118kB][4]
数字货币领域的Fungibility问题与Internet上的隐私问题类似，因为历史干净的Coin可能更受欢迎，交易所可能会根据历史交易针对用户采取特定措施（相对于区块上的历史交易图谱，交易所了解更多信息）。相关的应对措施？“In a world of AL/ML and targeted ads, plausible deniability is not a plausible defense.”当前的区块链隐私保护技术在上图有粗线条的展示。

Blockchain privacy is not intuitive。不仅存在被动的观察者，也存在主动的攻击者，通过与目标用户进行交易来获取信息。上图中的技术，大概分3类：
![6.png-135.7kB][5]
![7.png-81.5kB][6]
接下来的报告关注decoy system是否具有足够的隐私，当前的decoy system不够安全。通过taint tree分析可以跟踪客户、识别匿名的店主，而通过粉尘攻击则可以在decoy system中观察到某人的资金流动。如果想要用decoy system达到隐私保护的目的，Ian给出了几点建议：1）用足够大的decoy size（i.e. 5,000,000 instead of 5)；2）Decoy sets substantially overlap across all recent transactions；3）Decoys are sampled really carefully。但是仍然需要进行更详尽的分析以搞明白隐私保护在什么情况下失效并界定decoy system的能力边界。随后Ian给出了scalable decoy schemes需要满足的条件：
![8.png-170.2kB][7]
在最后，Ian给出scalability与privacy关系的思考（见下图），其中个人最有感触一点：privacy problems don’t magically go away with small tweaks。
![9.png-112.5kB][8]
额外参考An Empirical Analysis of Traceability in the Monero Blockchain【https://arxiv.org/pdf/1704.04299/】讨论Monero的隐私问题
##Playing With Fire: Adjusting Bitcoin's Block Subsidy
####Anthony Towns (Xapo)
很有争议的题目也是很有争议的报告，确实是在玩火（Anthony Towns的个人标签里有Bitcoin Core），考虑到当年区块扩容的场景。报告的中心主题，通过更改Bitcoin的挖矿奖励方式（仍然保持货币总量不变），有可能在未来降低Bitcoin网络挖矿的电力消耗。当前的挖矿奖励是每4年折半一次，Anthony给出的一个具体的方案（甚至给出了该提议的实现方案）根据全网算力的变化来计算挖矿奖励，而不是机械的每4年减半一次，例如：算力每翻倍一次，挖矿奖励就减少20%。

为了说明自己并不是真的在担心挖矿的电力消耗，Anthony首先明确了观点“saying Bitcoin uses too much energy is saying it should be less secure”。但是为什么会有人担心Bitcoin挖矿消耗了太多电力，Anthony给出了以下几个原因，看起来非常合理：
![10.png-84.4kB][9]
但是这些争论都是定性的讨论，是否能够定量讨论相关问题，也是接下来报告的重点。Anthony在Hashrate（TH/s），Electricity（kWh/year）和Value（USD）中选用TH/USD指标来进行讨论
![11.png-55.5kB][10]
基于TH/USD指标进行定量讨论，根据上述图标，有以下观察：
![12.png-113.5kB][11]
基于以上观察，Anthony尝试做出一些预测，但是Bitcoin中挖矿会收到各方面因素的影响：币价、矿机效率和电费。如果对这三个因素的如下假设
![13.png-46.5kB][12]![14.png-20kB][13]![15.png-18.8kB][14]
基于上述假设Anthony对奖励（USD）、挖矿难度、电力消耗做以下预测：
![16.png-28.1kB][15]![17.png-24.9kB][16]
![18.png-98.7kB][17]
根据上述预测，Bitcoin的电力消耗并没有传说中的那么恐怖“Bitcoin Mining on Track to Consume All of the World's Energy by 2020”，但是电力消耗也足够多，促使人（Anthony）重新思考如何降低电力消耗。因此有了提议：
![19.png-86.9kB][18]
报告中给出了详细的技术细节来实现这一提案，此处略去不讨论。上述方案的部署，会带来怎样的效应，Anthony继续给出了如下预测图表：
![20.png-38kB][19]![21.png-41.9kB][20]
![22.png-26.4kB][21]![23.png-71.6kB][22]
随后Anthony继续提到，这一提案还有别的用处：1）Smothing the halvening schedule；2）Smoothing fee income when a fee market eventuates；3）As a cost mechanism for allowing temporary increases in the block weight limit。或许有别的方式能达到同样的效果，报告中未详细提及。报告最后，Anthony也说到，这不会是个win-win-win的提案：
![24.png-127.3kB][23]
##Forward Blocks: On-chain/Settlement Capacity Increases Without the Hard-fork
Slide首页标注报告人Mark：No organizational afﬁliation，但是Google得到的信息Mark似乎是Blockstream的cofounder。值得注意的是，在这个报告中也提及了修改挖矿奖励。Forward Blocks协议目标是用soft-fork的方式修改PoW机制，并且用soft-fork的方式实现privacy-enhancing alternative ledgers，但是后来发现该协议还带来额外的好处：1）Improved censorship resistance through sharding；2）Direct on-chain scaling up to 3584x (for bitcoin speciﬁcally)。同时还能其他益处：1）linearized block subsidy；2）将来支持confidential transaction和sidechain。

为了便于讨论，Mark首先给出了soft fork和forwards compatible soft fork的定义，其主要差别在于未升级的节点在后一种fork的情况下仍然能够接收并处理所有的交易。协议的第一个组件是Dual PoW through soft fork。这并不意味着对一个块执行两次PoW机制。Forward Blocks协议中包含separate chains with separate PoW，不同的链上的PoW机制所采用的Hash选择在协议中并没有明确给出，报告中Mark也特意讨论了这一可能的争论点，并说自己不做推荐。
![25.png-278.1kB][24]
Onchain scale的一个自然的想法是提高区块上限。之前已经讨论过的安全提高区块上限的措施是通过forced hard fork：“move transactions into a committed extension block with higher aggregate limits—and/or any other consensus changes—and then force the old blocks to be empty”。这种升级方式的问题在于未升级的节点再也看不到交易数据，因此只能被迫升级。并由此因此了Forward blocks协议的出发点：
![26.png-261.6kB][25]

Forward block协议激活后会有两条链： forward block chain和compatibility block chain（未升级的Bitcoin链）。Forward block chain有更大的区块和更长的区块间隔时间（inter-block interval）。Compatibility block chain通过利用time warp bug缩短区块间隔时间（10分钟的间隔内可以出现多个区块）来进行扩展。也即compatibility block chain是首先在Forward block chain中被处理，compatibility block chain按照forward block chain的区块中指定的交易顺序依次处理forward block中的交易。由于forward block会大于compatibility block，所以可能需要多个compatibility block才能处理完一个forward block，这也是为什么需要利用time warp bug来在一个区块间隔时间内构造多个区块的原因。

但是这种设计策略会带来各种问题，包括但不限于：1）由于forward block较大，可能需要多个compatibility blocks来处理单个forward block，所以会有工作进度的问题（对某个forward block处理的进度）；2）由于forward block chain中的coinbase交易对于未升级的节点来说是不可见的，所以forward block chain区块奖励只能通过分摊compatibility block chain中的coinbase的奖励来达成，也即compatibility block chain中的区块奖励需要在两条链中进行分摊；3）由于两条链中进行挖矿的是不同的矿工（逻辑上），需要某种同步机制来确保compatibility block chain能够看到和处理forward block chain处理过的消息、forward block chain中的矿工也需要看到compatibility block chain中的coinbase来将其放到UTXO集合中。

Loosely Coupled Chain State 用来处理两条链之间的同步问题。两条链互相commit到对方的block header中，当一个区块头部经过100次确认之后，就认为该区块头被锁定（locked-in）：所有被锁定的区块头必须引用一个合法的区块；compatibility chain锁定一个forward block header时，该forward block中的交易被添加到compatibility chain的交易处理队并将改块的coinbase的output添加到compatibility chain的coinbase的payout queue中（以让forward block的miner分摊区块compatibility chain中的挖矿奖励）；当forward chain锁定compatibility block header时，compatibility block中的coinbase交易进入UTXO集合。也即两条链都在跟踪另一条链的信息。

Forward block chain的初始参数为15分钟的区块间隔和6MWe的max block weight，两个参数的选择与原来的Bitcoin的交易处理维持在同一个水平（10分钟区块间隔和4MWe的max block weight）。Time warp bug被用来在600s的时间间隔内在compatibility block chain上创建足够多的区块以及时处理forward block中打包的交易。

Mark在报告中继续讲到“A Smooth Subsidy Schedule”的主题，具体说来是eliminate the “halvening” with a continuous subsidy curve，不过是针对forward block进行的区块奖励调整，但是要保证区块货币总量不超过2100w个上限并且在任意时间调整后的累计奖励不能超过Bitcoin原计划中累计奖励太多。（个人认为：通过调整forward block chain的奖励，可以影响compatibility block 

chain中的区块奖励，通过合适的参数配置，应该可以达到前一个报告中Anthony提案的类似效果）“Most notably, we can use this opportunity to smooth out the step function used to calculate subsidy, making subsidy a continuous function”调整后的区块奖励的变化更为平滑，见图：
![27.png-54.8kB][26]

接下来Mark继续讨论multiple forward block chains，基本idea是，如果可以有一个forward block chain，也可以有多个multiple forward block chains。基本动机是，可以将一个forward block chain“分片”（与以太坊、ZILLIQA中的Sharding概念有区别）为多条链，每条链的UTXO集合无交集。通过将forward block chain分片为M条链，要求交易只能花费一个分片中的coin，并且每条链用不同的PoW机制（separately-salted work function）。注意每个shard chain中还是以15分钟的间隔生成区块，这样大约每15min/M就可以产生一个shard block，能够达到“M-fold increase in censorship resistance for a given aggregate weight across all shards, if activity is evenly distributed amongst the shards.”可以用前述的coinbase方法在shards之间进行value transfer。

一番折腾之后，理论上会有多少TPS的提升？Mark在下图的slide中给出了理论的上界（利用time warp bug缩短compatibility block chain的区块间隔在保证compatibility时的理论上限），结果比较有意思，基本可以支持1tx/pp/day的目标，即支持地球上每个人每天在Bitcoin上发送一笔交易。
![28.png-277.3kB][27]
在报告的最后，还提及coinbase payout queue还有别的用处（Generalized ledger transfer mechanism）。还不太理解这部分，需要研读相关论文。
![29.png-268.2kB][28]
##Self-Reproducing Coins as Universal Turing Machine 
挺有意思的一个报告，展示了do more with less的方法论。相关工作的基本结论是：“Turing-completeness of a blockchain system can be achieved through unwinding the recursive calls between multiple transactions and blocks instead of using a single one”。通过在UTXO模型下构建一个简单的通用图灵机（universal Turing machine）的展示证明了这个结论。

报告由几个问题驱动：1）是否可能在没有jump（或者等价的）操作符的条件下实现图灵完备？2）是否可能在UTXO模型下达成这个目标？3）如果可能，有什么实际应用？
![30.png-124kB][29]
有很多传言说，如果在区块链上构建图灵完备的机器，jump、while或者递归操作必须在脚本层面得到支持。但是报告通过构建如下的Bitcoin+来说明，这些传言并不准确。在下图的Bitcoin+构造中，在脚本语言层面，除了基本的算术运算、条件判断、密码算法等基本操作之外，仅添加了数组声明与访问、上下文数据获取等操作。

在上述操作的基础之上，展示了这些操作足够达成图灵完备目标。“Turing completeness can be demonstrated by simulating a simple Turing complete system”。另外有工作证明复杂度学科中的Rule 110元胞自动机是图灵完备的。所以通过展示利用上述操作可以模拟Rule 110元胞自动机的执行，就可以证明在UTXO模型下如果脚本语言支持以上语言特性，则该脚本语言是图灵完备的。
![31.png-99.1kB][30]
![32.png-155.9kB][31]
![33.png-111.5kB][32]
在关于上述构造的实际应用方面，报告给出了如下几个方面：1）Crowdfunding；2）Demurrage currency；3）Oracles（w. authenticated state）；4）Decentralized exchanges。并给出了ERGO项目中Token实例：
![34.png-160.4kB][33]
最后报告给出了几个结论：1）图灵完备的区块链系统可以通过在交易之间展开递归调用达成；2）提供了Ergo区块链脚本系统的图灵完备证明；3）该构造方式非常明确并且功能已经完整实现；4）Self-reproducing coins allow one to make practical constructions，例如很多的验证操作可以从硬编码转到脚本语言层，更进一步则可以实现任意复杂的逻辑。

问题：self-reproducing coins是在哪里体现的？
##Compact Multi-Signatures for Smaller Blockchains
这项工作关注multi-signature的密码算法构造，在此之前， Maxwell等人基于Schnorr签名构造了multi-schnorr-signature算法MuSig，允许一组签名者对同一个消息进行签名，与标准的Schnorr签名算法相比，MuSig在支持多人签名的同时，保持了密钥大小和签名大小不变，也即允许key aggregation（将不同的签名者的公钥聚合到一起生成aggregate public key），也即验签过程与标准的Schnorr签名验签并无区别。MuSig算法是在public-key model（参与签名的各方无需证明自己持有自己所声称公钥的对应私钥）下第一个支持key aggregation的multi-signature算法。然而在MuSig提出之后，有人指出其证明过程中的缺陷，Maxwell等人随后更新了MuSig算法重新证明了MuSig的安全性，代价是参与签名的各方从之前的2轮交互变为3轮交互。
![35.png-125.7kB][34]
![36.png-84.4kB][35]
multi-signature签名算法可以用来缩减Bitcoin的区块链大小，这包括两个方面：1）花费multisig output时，压缩多个签名值和公钥；2）聚合一个交易中不同的inputs中 的签名（参见上图）。Maxwell等人针对Bitcoin的历史区块分析了如果应用multi-signature来压缩区块，在缩减区块方面的效果，参见下图：
![37.png-91.5kB][36]
Boneh等人在这次报告中的工作一方面，即是用 BLS签名算法替代Schnorr签名算法那构造了新的BLS-based multi-signature scheme （MSP）与之前的工作相比，新构造中聚合操作可以公开执行（即是签名各方已经下线也可以执行），而基于Schnorr签名的聚合操作仅能在各方签名时进行（要求签名各方在线）。

另外，这项工作还描述了aggregate multi-signature scheme （AMSP）用来将不同交易的多个签名值压缩成一个聚合签名。Aggregate multi-signature scheme lets each of the n parties sign a different message, but all these signatures can be aggregated into a single short signature. aggregate multi-signature可以看成是更广义的multi-signature，不仅可以在一个交易内部进行签名压缩，而且可以在交易之间进行压缩（将一个区块内的所有交易的签名压缩），参加下面3个图片示例。
![38.png-55.5kB][37]
![39.png-69.6kB][38]
![40.png-96.7kB][39]
此外，这项工作还构造了第一个accountable-subgroup multi-signature scheme（ASM）。An ASM enables any subset S of the n parties to jointly sign a message m, so that a valid signature implicates the subset S that generated the signature; hence S is accountable for signing m. The veriﬁer in an ASM is given as input the (aggregate) ASM public key representing all n parties, the set S ⊆ {1, . . . , n}, the multi-signature generated by the set S, and the message m. 值得指出的，任意安全的签名体制都能够构造出ASM算法，例如比特币中的多签机制，多个签名的级联其实就是accountable  multi-signature，只是公钥大小与组内成员个数n呈线性关系，而签名大小与参与签名的人员个数S呈线性关系。Boneh等人在这个工作中给出ASM的构造，签名大小在S的描述之外仅与安全参数k有关系，与n的大小无关，而公钥大小也为常量。就密码算法构造角度来讲，ASM是这项工作中最具创新性的工作。ASM机制允许在多签交易中设定任意的阈值，而且在解锁脚本中也无需重复列出所有的公钥。同样的ASM也可以在多个交易之间做压缩。
![41.png-109.6kB][40]
![42.png-115.3kB][41]
各签名算法之间的区别在下图中总结。还没有时间深入理解，但是聚合签名算法，BCH支持大区块的路线图中，也许可以用来降低计算量，在多个维度进行签名（或者公钥的聚合）：1）在multisig交易中；2）在交易的多个input之间；3）在一个块内的所有交易之间。这个方向值得深入跟踪下。
![43.png-365.1kB][42]
##Improving SPV Client Validation and Security with Fraud Proofs
汇报的是V神参与的研究成果，关注如何利用Fraud Proofs增强SPV节点的安全性，问题来源于当前的SPV节点会接受非法的区块，因为SPV的安全性基于诚实的大多数这一假设。报告也从该问题出发：how can we make non-fully validating (SPV) nodes reject invalid blocks, so that they don’t have to trust miners? 链接中的论文摘要中阐述了这一工作的目标与意义：“By allowing such clients to receive fraud proofs generated by fully validating nodes that show that a block violates the protocol rules, and combining this with probabilistic sampling techniques to verify that all of the data in a block actually is available to be downloaded, we can eliminate the honest-majority assumption, and instead make much weaker assumptions about a minimum number of honest nodes that rebroadcast data. Fraud and data availability proofs are key to enabling on-chain scaling of blockchains (e.g., via sharding or bigger blocks) while maintaining a strong assurance that on-chain data is available and valid.”。
![44.png-128.6kB][43]
之前已经有部分关于fraud proof的工作，Bitcoin的白皮书中简略提到了“alerts”，全节点利用该消息通知SPV节点某个区块是非法的。Maxwell和Todd在“compact fraud proofs”方向上做了部分研究工作，早期的提案对不同规则的违反需要不同的fraud proof，汇报的内容在这个基础上进行改进。G. Maxwell has discussed on IRC using erasure coding for data availability with a scheme using a “designated source” with PoW rate-limiting (and no way to deal with incorrectly generated codes?)。

提出的方案中用到的第一个工具是Sparse Merkle Tree。通过将blockchain看作是state transition system，并利用Sparse Merkle Tree来表示state，就可用Merkle Root来表示当前state指纹。每执行一笔交易，MerkleRoot就被修改，即有表示：transitionRoot(stateRoot, tx, witness) = stateRoot’ or error。则一笔交易的witness就是该tx读写过的所有状态的merkle proof。
![45.png-129kB][44]
通过将交易执行之后的post-state root存储到区块中，就获得了tx的execution trace，注意，并不需要把每个tx的excution trace都存放到区块中，可以每隔几个交易保存一个execution trace，但这里有trade-off。
![46.png-113.4kB][45]
按照这种结构组织数据之后，fraud proof会包含图片中的几个部分，根据存储的MerkleRoot信息，就可以构造针对某个tx的fraud proof，也即从stateroot出发经过该tx之后得不到下一个合法的stateroot，即可判定该tx非法。如前所述如果是每隔几个tx在区块中保存一个execution trace，则fraud proof的大小会增大，即前述的trade-off。

报告接下来处理data availability problem，所利用的基本工具为erasure coding。erasure coding可以将t个片段数据扩展成2t个片段的数据，然后可以从任意的t个片段恢复出原先的数据。
![47.png-100.8kB][46]
论文中的研究工作通过利用multidimensional erasure code方法来应对miner可能生成错误的erasure code的情况，同时不给SPV节点增加过多负担。报告最后比较了采用该方法后，SPV节点安全性的假设，见下图。
![48.png-125.7kB][47]
##A Scalable Drop-in Replacement for Merkle Trees
个人比较喜欢的报告，但似乎作者还没有公开相应的论文，Benedikt Bünz也是Bulletproof协议的设计者，报告以另一个blockchain会议的预告开始，密码Dan Boneh主场办会，值得期待。
![49.png-85kB][48]
报告主要讨论UTXO集合不断增长的问题，并提到了Miller、Todd、Dryja等人提出的UTXO-Commitment的应对方案，通过将UTXO结合commit到区块中，并结合Merkle Tree的特性，可在log(n)的复杂度内，完成inclusion proof、exclusion proof（if sorted）以及update（这个不是很懂为什么会是log（n），感觉应该是O(n)的复杂度，存疑），参见下面PPT截图。
![50.png-60.4kB][49]
![51.png-73kB][50]
利用utxo-commitment可以构建stateless full node/mining，例如花费某个UTXO时，是需要提交相应的merkle inclusion proof证明其存在于最新区块上的utxo-commitment所关连的merkle tree中（也即未被花费）。
![52.png-111.1kB][51]
然而基于Merkle Tree构建这种机制，有如下问题：
![53.png-80.3kB][52]
从这个点出发，Bünz提出利用acumulator[CL02,…]来处理应用Merkle Tree时碰到的上述问题。并从RSA accumulator开始谈起，利用RSA accumulator，元素的增加删除都非常快速，一个指数模运算就足够了：
![54.png-58.4kB][53]
基于同样的构造甚至可以做accumulator proof，包括inclusion、exclusion proof，基于[LiLiXue07]中的构造，甚至可以做efficient stateless update。
![55.png-64kB][54]
基于RSA的accumulator有以下几个问题：1）N=pq，该由谁来选择p和q，也即trusted setup的问题；2）基于RSA Accumulator做Del时需要知道限门；3）Ron Rivest Assumption说到“You can find Ns in the wild”。现状：
![56.png-98.1kB][55]
针对第一个问题，Bünz提出用一个新的数学结构（我第一次听说）class group[BW88, L12]来规避：
![57.png-71.3kB][56]
关于aggregate inclusion proof：
![58.png-40.4kB][57]
关于stateless deletion，利用inclusion proof可以在不知道限门信息（p和q）的条件下，进行删除，并可以做批量删除，都是非常好的特性。
![59.png-67kB][58]
基于RSA的问题在于速度可能会是个问题，没有class group的速度信息：
![60.png-28.7kB][59]
针对速度问题，Bünz提到Wesolowski Proof [Wesolowski’18]可以用来做Proof-of-Exponentiation，在128比特的安全强度下，PoE比直接计算指数快5000倍的，这一技术可以用来做Fast Block Validation。
![61.png-72.3kB][60]
![62.png-85.1kB][61]
Bünz给出了在MacBook上的测试速度（基于RSA，测试比较粗糙并不精确），仍然对于基于classgroup的速度还没有相关数据。
![63.png-102.1kB][62]
同样的技术可以用来做别的事情Vector Commitments
![64.png-66kB][63]
具体在Short IOPs （STARKs etc.）中会有应用，600kb -> 200kb
![65.png-53.3kB][64]
![66.png-54.2kB][65]
这个报告的内容，在paper出来之后，需要深入看下。
##HTC | EXODUS
来自HTC的Justin Lin介绍了他们在开发中的Zion Wallet，特性在于基于手机的TrustZone组件构建钱包的安全体系，通过分隔钱包敏感操作与日常操作，来确保钱包（尤其是密钥的安全）。各类钱包App都需要考虑的一个问题，如何备份用户的私钥。就目前获得的信息来看，基于助记词的密钥备份方式应用广泛（例如基于BIP39：Mnemonic code for generating deterministic keys等方式将私钥转换为容易记忆的单词，记录在纸张上并将纸张安全存储）。Zion Wallet中采用Shamir的门限算法（例如：3-of-5）将私钥分割，并将分割后的私钥分片发给用户信任的人（或者存储在不同的地方，如果没有可信任的朋友），在需要恢复私钥时，拥有相应私钥分片的人提供各自拥有的私钥分片，由TrustZone进行门限算法的计算并恢复出私钥（Social Key Recovery）。类似的方式支付宝曾经用过，问题是如果几个被选定的私钥分片拥有者可联合起来背叛用户。
![67.png-270.3kB][66]
关于TrustZone的原理介绍可参考博客《一篇了解TrustZone》【https://blog.csdn.net/guyongqiangx/article/details/78020257】。简单来说，TrustZone在概念上将SoC的硬件和软件资源划分为安全(Secure World)和非安全(Normal World)两个世界，所有需要保密的操作在安全世界执行（如指纹识别、密码处理、数据加解密、安全认证等），其余操作在非安全世界执行（如用户操作系统、各种应用程序等），安全世界和非安全世界通过一个名为Monitor Mode的模式进行转换。具体的处理器架构上，TrustZone将每个物理核虚拟为两个核，一个Non-Secure Core，用来执行非安全世界的代码；一个Secure Core，运行安全世界的代码。
![68.png-184.3kB][67]
![69.png-254.1kB][68]
##Efficient Transaction Relay
Gleb的报告关注Bitcoin网络中交易中继的所带来的带宽消耗问题。Bitcoin的网络有冗余的，默认情况下每个节点都跟8个节点连接。交易由冗余的网络进行中继，会带来带宽方面的开销。因此引入了交易中继协议（Transaction Relay Protocol）以避免传播对等节点不需要的交易。完整的交易大概250字节，而交易的声明是32个字节，虽然有所改进，但是交易的声明仍然是冗余的，大约85%的声明是重复的。
与理想情况相比，现在的网络中交易的广播效率非常不理想，Gleb等人试图通过重新设计中继协议来进一步降低带宽使用，这中间需要考虑的因素有带宽、时延（传播时延）以及面对恶意攻击者时的鲁棒性。
![70.png-165.2kB][69]

![71.png-178.2kB][70]

![72.png-183.5kB][71]

![73.png-195.4kB][72]
新协议包含两部分：low-fanout flooding （to relay txs only to a fraction of all network nodes）以及 transaction reconciliation（to bridge gaps）。Set reconciliation的目标是使拥有不同tx集合的两个节点A、B以最少的通信量计算并集。新协议允许利用short tx IDs来进行交易声明，并将带宽占用降低至原先的1/44。值得注意的是，BCH社区提出的Graphene协议也用set reconciliation技术减少带宽利用。
![74.png-150.8kB][73]
##WIP | b_verify 
b_verify: a protocol that makes equivocation as hard as double spending on Bitcoin by providing the abstraction of multiple independent logs of statements in which each log is controlled by a cryptographic keypair.

Equivocation: is the deliberate presentation of inconsistent data by a participant within a system.
![75.png-212.1kB][74]
Perfect world意味着至少有一个诚实Commitment Server（CS），并检查其他CS提交的数据。网络中的每一方V有公私钥，并相互独立。每一方V用b_verify客户端将各自的观测存储到日志数据中，然后CS将日志数据组织成ADS（应用Merkle Patricia Tree，MPT）,数据的改变（树根）通过Catena存储到Bitcoin区块中（OP_RETURN的output）作为见证。依赖MPT的认证特性与Bitcoin网络，可以保证日志和见证数据不被随意修改。

未解决的问题也即能够破坏b_verify协议的隐患，也是一开头perfect world假设存在的原因：
1）diabolical commitment server的问题如何解决？
2）malicious Bitcoin peer can hide new witness transactions from a light client

应用场景：1）公开可验证的注册服务；2）供应链中的数据管理

问题：跟open timestamp有什么区别？从提供的功能上貌似是一样的？
https://opentimestamps.org/ （主要区别是用CS？，演讲人不确定as WIP）
问题：使用BCH岂不是会更好一些？更低手续费？OP_RETURN的大小？
![76.png-172.5kB][75]
![77.png-151kB][76]
![78.png-130.8kB][77]
##Introduction of Internet-Draft: General Security Considerations for Crypto Asset Custodians
已经有太多安全事故，我们应该从过去的事故中学到经验教训。
![79.png-152.6kB][78]
Goals：
1）	build a base document for crypto assets custodian’s security best practices
2）	share lessons from past incidents without violating a confidentiality obligation (e.g. criminal investigation)
![80.png-176.6kB][79]

![81.png-264.8kB][80]

![82.png-136.6kB][81]
##OmniLedger: A Secure, Scale-Out, Decentralized Ledger via Sharding
报告以OmniLedger与Bitcoin之间的TPS、确认时间等维度的对比开始，最重要的区别在于OmniLedger在有更多资源时（网络中更多的参与节点），其TPS随着资源线性增长（也即Sharding）。
![83.png-108.8kB][82]
OmniLedger是新设计的区块链结构，汲取了近几年提出的新理念，包括Bitcoin-NG、Luu等人提出的Sharding理念、CollectiveSigning等，结合基于RandHound方法进行节点的随机选择分派。
![84.png-84.7kB][83]
做Sharding时如果节点可以自由选择加入哪个分片，则恶意节点可以集结到同一个分片从而操控该分片。所以OmniLedger采用了随机分配的方式，过随机分配，在每个分片内都含有较多节点的情况下，可以保证高安全性，如下图所示，即使在有25%的恶意节点的条件下，每个分片内分配1000个节点，可以将失败概率控制在10^-6之下。
![85.png-65.7kB][84]
Bitcoin-NG的基本思想是在保持Bitcoin的10分钟PoW出块的前提下，在两个区块的间隔中通过生成microblock（利用签名做不可篡改保证，前一个PoW区块产生的节点在这个区间内有权生成microblock并用自己的私钥签名）继续打包tx，由此提高Bitcoin网络的TPS。然而Bitcoin-NG的构造中有重大缺陷，但是这个idea结合Sharding、PBFT等方法启发了多个后续工作，包括ByzCoin、ELASTICO和ZILLIQA等，OmniLedger是这个方向上的又一新的成果，发表在安全顶会IEEE S&P 2018，也是我个人一直比较喜欢的一个工作。在Sharding方向，cross-sharding通信一直是个难度，OmniLedger第一个（我所了解到的信息）明确针对这个问题给出解决方案（优雅程度另说）。

OmniLedger的核心思想参见下图，如前述，OmniLedger的区块链由两种区块构成KeyBlock（PoW区块）和Microblock（打包交易），这是来自Bitcoin-NG的思想，类似的也用KeyBlock来做Leader的选举，而Microblock由Leader打包并广播到网络。
![86.png-84.8kB][85]
从这些idea出发，Kokoris-Kogias接下来从跟一个粗糙的原型系统SimpleLedger出发，通过不断改进SimpleLedger方案，最终呈现OmniLedger的整体方案。SimpleLedger方案参见下图。根据Trusted Randomness Beacon生成的随机数对节点进行随机分片，在每个分区中利用共识规则对处理的交易达成一致。
![87.png-120.8kB][86]
SimpleLedger的上述构造中，存在几个问题包括安全性和性能两个方面。安全性方面：1）trusted randomness beacon需要可信第三方；2）epoch切换时不能处理交易；3）不能处理跨片区的交易。在性能方面：1）系统鲁棒性不够强；2）存储和bootstraping的代价较高；3）tps和时延之间的trade-off。
![88.png-65.3kB][87]
Kokoris-Kogias给出了OmniLedger协议的路线图，基本上是针对SimpleLedger中存在的安全和性能方面的6个问题逐一进行处理，就得到了OmniLedger协议。如前所述，用到的一些工具为distributed randomness（RandHound）（VRF）、Atomix协议、Robust BFT共识等。
![89.png-72.5kB][88]
针对trusted randomness beacon，采用distrusted randomness方案去掉去可信第三方的依赖（VRF）。给出epoch切换时的交易处理暂停的问题，给出了对应的解决方案。
![90.png-59.2kB][89]
针对跨分片的交易，构造了atomic cross-shard tx处理协议。跨分片交易的处理的挑战在于，对该tx的commit atomically或者abort eventually，针对这一挑战设计了atomix协议，该协议的基本思想是如下图所示的两阶段承诺协议
![91.png-43.2kB][90]
Atomic协议是client-managed protocol，client端向一tx（跨分片）的input所在的分区发送tx信息，如果从所有分区都收到了ack确认（该input合法），则client向output所在的分区提交commit。如果没有收到所有的ack确认，则abort并回收该tx中的资产。
![92.png-207.6kB][91]
针对latency与tps之间的trade-off，OmniLedger给出了Trust-but-Verify的交易验证模型。这个trade-off来自于更大的区块可以承载多的交易（更好的tps）但是更大区块的传播（网络和验证）有更大的latency。针对这一问题，OmniLedger的对策是两级验证机制：1）更小的分区、安全性更弱（让客户端选择）来加快optimistically validation（比如1个确认就认为足够，而不需要6个）；2）更大的分区（安全性更好）来对区块做进一步审查。这一过程类似于信用卡的使用时的两级安全验证：小额交易不用输入密码、大额交易才输入PIN码。
![93.png-105.2kB][92]
接下来Kokoris-Kogias给出了OmniLedger协议的测试环境和测试数据，可以看到随着节点的增多，shards增多的时候，相应的tps也在不断提升。而trust-but-verify模型下，tps也比regular模型要高。
![94.png-136.5kB][93]
![95.png-96.3kB][94]
![96.png-57kB][95]
![97.png-160.7kB][96]
OmniLedger协议感觉上是比较靠谱的Sharding方案，真正处理了跨片问题，也有开源的代码和测试数据，感觉值得深入了解。
##The GHOSTDAG Protocol
这个报告关注基于DAG（Directed Acyclic Graph）结构的区块链。

报告首先回顾了Bitcoin的共识协议，并介绍PHANTOM协议改动的地方
![98.png-149kB][97]
![99.png-209.4kB][98]
PHANTOM协议带来的好处是：security no longer breaks at higher throughput.但是latency会增加，也没有解决存储、验证时间等scaling相关的问题。
![100.png-195.7kB][99]
为方便讨论，先给出一些术语。Past区块是指被区块x直接或间接指向的区块，Future区块是指被区块x影响的后续区块，而Anticone的区块则是剩下的那些区块，没有被x直接或间接引用，也不直接或间接应用区块x。

在一个DAG结构中，如果有两个区块B1和B2差不多同时被诚实的节点打包并广播到全网，则会出现如下图所示的DAG结构。如果预知B1和B2是由诚实节点构造的区块，则下图结构可以划分成两个部分：诚实节点打包的区块和攻击者打包的区块。
![101.png-200.4kB][100]
由此引出了k-cluster的概念，或者叫做k-chain的概念：一组区块的集合C是k-cluster，如果这个集合中任意区块B的Anticone集合与该集合的交集中包含的区块个数不大于k，则这个区块的集合是k-cluster的。示例参见下图所示的1-cluster和2-cluster。
![102.png-82.2kB][101]

![103.png-76.3kB][102]

![104.png-82.4kB][103]
根据上述k-cluster的概念，不难想象中0-cluster其实就是Bitcoin中的链。也即k-cluster DAG是比chain更为广义的概念。
![105.png-119.4kB][104]
从k-cluster的概念出发，Zohar给出了PHANTOM协议概貌： pick a max weight k-cluster in the DAG, then sort it topologically in some canonical way。
![106.png-191kB][105]
上述过程的难点在在于，在DAG中寻找maximal k-cluster是NP-Hard的。Zohar等人在这项工作中给出的解决方案是：用贪婪算法来获得k-cluster，由此有了名字：GHOST-DAG protocol。
![107.png-143.4kB][106]
GHOST-DAG协议的核心思想也非常简单：1）each block inherits the “heaviest” k-cluster from one of its predecessors；2）adds blocks greedily (as long as still a k-cluster)。
![108.png-125.3kB][107]
根据上述两条原则，在上图中，带？的区块在选择父辈区块时，两个可能的父辈区块的k-cluster包含的区块个数都是5，此时做一个随机选择就可以，然后在保持k-cluster性质的基础之上，将尽可能多的区块包含到当前的k-cluster集合中。
![109.png-94.9kB][108]

![110.png-101.1kB][109]
下图所示的带？的区块，在选择自己的父辈区块时，由于两个可能的父辈区块的权重不一样，所以此时没有任何疑问，选择大的那个。
![111.png-105kB][110]

![112.png-77.9kB][111]

![113.png-86.6kB][112]
构建出来的DAG为什么能够阻止double-spend？Zohar随后给出直观解释。假设下图中区块X和Y中有冲突的交易，但是这种情况算不上成功的double-spend攻击，因为区块Y实际上是“well behaved”的区块，区块Y指向了区块发布时的tips并且被广播到全网。此时X（tx1）和Y（tx2）区块中的冲突交易可以被快速的解决掉。由于两个区块都在k-cluster中，并且协议会对区块进行拓扑排序（假设X排在Y之前）则tx1只需要等待几个区块确认即可，而tx2由于与tx1冲突，不会被视为合法的交易（最后这半句是猜测的：k-cluster区块中可以存在相互冲突的交易，但是具体哪个交易合法，需要根据排序来确定，区块在前的交易被认为是合法的交易，与之冲突的交易不被协议采纳，但是协议不会因此就排斥相应的区块，而只是从中择取无冲突交易）。此时并没有成功完成double-spend攻击，交易的冲突被快速解决了。
![114.png-152kB][113]
真正需要避免的是下图所示的double-spend的尝试，攻击者在Y区块中构造tx2来双花区块X中的tx1，但是并不广播区块Y，而是等到tx1中资产的接收人已经确认了收到资产之后，再释放自己的区块（可能有多个，下图红色曲线中的区块），但是在这种场景之下，GHOST-DAG协议也保证了这种double-spend不会成功，因为这要求攻击者cluster中的区块数多余诚实的cluster中的区块数，在诚实算力的大前提下，这是不可能的。
![115.png-182.6kB][114]
Ethereum中由于15s的区块间隔，在同一个高度出现两个区块的概率较高，所以采用了DAG的思想，与这个工作的区别在于，在Ethereum中仅将叔块包括进来并给一定的挖矿奖励（不浪费算力），但是叔块中的交易并没有被处理，并且以太坊的引用文献里提到了这个报告内容的两位研究人员早期的一个关于DAG的工作。


  [1]: http://static.zybuluo.com/Fangzheng1992/0zn5perol69qtk8tlx3ggnig/2.png
  [2]: http://static.zybuluo.com/Fangzheng1992/hjc42ljpzdt6wsts0c6rzuk6/3.png
  [3]: http://static.zybuluo.com/Fangzheng1992/8ape3dvkbtf5fkkimw5061gy/4.png
  [4]: http://static.zybuluo.com/Fangzheng1992/6qz25g1b77bzkx355kjirco9/5.png
  [5]: http://static.zybuluo.com/Fangzheng1992/lb7sy3nwfbyg1rejllsxcijq/6.png
  [6]: http://static.zybuluo.com/Fangzheng1992/igq7c8mixdmb3dbnrwgd049t/7.png
  [7]: http://static.zybuluo.com/Fangzheng1992/rzbfr3qyicsvoq9mvkm7zv4w/8.png
  [8]: http://static.zybuluo.com/Fangzheng1992/jh3rnj2m53z9tdu7pf3zcu8d/9.png
  [9]: http://static.zybuluo.com/Fangzheng1992/4seke7foyjhmz08kx583bvce/10.png
  [10]: http://static.zybuluo.com/Fangzheng1992/beoijp4dwp5j0wzvvxgnjghd/11.png
  [11]: http://static.zybuluo.com/Fangzheng1992/unzvvgvsupixx01avx0us6l6/12.png
  [12]: http://static.zybuluo.com/Fangzheng1992/6vuw9flax834ym86pojf0p2d/13.png
  [13]: http://static.zybuluo.com/Fangzheng1992/jk3julfiqx36jwxqjvjgxgh8/14.png
  [14]: http://static.zybuluo.com/Fangzheng1992/t0zt7l0udcp1eghozacdjkeg/15.png
  [15]: http://static.zybuluo.com/Fangzheng1992/6m95drxg9lejntxu3gun5p76/16.png
  [16]: http://static.zybuluo.com/Fangzheng1992/dczf3tplcjuotiz11bn5veof/17.png
  [17]: http://static.zybuluo.com/Fangzheng1992/w4329n7fxbd5r1mqes0pfnqp/18.png
  [18]: http://static.zybuluo.com/Fangzheng1992/g4pervii4w59b3jgssh10wqi/19.png
  [19]: http://static.zybuluo.com/Fangzheng1992/il5t2lqukxpwrczabku1oo78/20.png
  [20]: http://static.zybuluo.com/Fangzheng1992/nlu55ckx7oigxseyvqs6g7hj/21.png
  [21]: http://static.zybuluo.com/Fangzheng1992/hxe7uzam0jk9bm0mfli202xq/22.png
  [22]: http://static.zybuluo.com/Fangzheng1992/ramx7bxft08ffsiuj564pef4/23.png
  [23]: http://static.zybuluo.com/Fangzheng1992/wyixwvjuytufsog2sa7uk9bk/24.png
  [24]: http://static.zybuluo.com/Fangzheng1992/8kh2sbaea6bb5y9cfzqmhr2t/25.png
  [25]: http://static.zybuluo.com/Fangzheng1992/5y05lj8shme5tbbdzvk8jpjk/26.png
  [26]: http://static.zybuluo.com/Fangzheng1992/ycoi68yh0x74mf6acyq59vq0/27.png
  [27]: http://static.zybuluo.com/Fangzheng1992/7liui1z9q95qix2n2lvmho2d/28.png
  [28]: http://static.zybuluo.com/Fangzheng1992/6tctswmhbntkbihqiulk1uz9/29.png
  [29]: http://static.zybuluo.com/Fangzheng1992/w46rwxl990mzqni9dcbpkq3u/30.png
  [30]: http://static.zybuluo.com/Fangzheng1992/dfjlkutpvx1ut5g4x5a00d6k/31.png
  [31]: http://static.zybuluo.com/Fangzheng1992/7zk8c17ufkod0gqai8bx90u4/32.png
  [32]: http://static.zybuluo.com/Fangzheng1992/eoc2vuvrzaa6rnnwirqho4gl/33.png
  [33]: http://static.zybuluo.com/Fangzheng1992/7j1ag8kzvolbjzgmh6ugmxsv/34.png
  [34]: http://static.zybuluo.com/Fangzheng1992/ili867922h2l43lw64rjfask/35.png
  [35]: http://static.zybuluo.com/Fangzheng1992/vrnxsczdmy4449ktaw232nmz/36.png
  [36]: http://static.zybuluo.com/Fangzheng1992/po82zddtz02vomrugka47a80/37.png
  [37]: http://static.zybuluo.com/Fangzheng1992/9tam9fwmhmsi0icqjkrd26ir/38.png
  [38]: http://static.zybuluo.com/Fangzheng1992/f78bcvgab5bk09g3hipkv560/39.png
  [39]: http://static.zybuluo.com/Fangzheng1992/qxvsowjslr985ii6p8huyh57/40.png
  [40]: http://static.zybuluo.com/Fangzheng1992/seeizfsaeve9d0rm084k3qck/41.png
  [41]: http://static.zybuluo.com/Fangzheng1992/v7rcqo8hv7tkstar9bfwwhdq/42.png
  [42]: http://static.zybuluo.com/Fangzheng1992/0tz58n4xtl13tq7hd32xagcm/43.png
  [43]: http://static.zybuluo.com/Fangzheng1992/7apgl4k21b0fed1b0ibpy26d/44.png
  [44]: http://static.zybuluo.com/Fangzheng1992/1b5vefksuz26162glbvy9hig/45.png
  [45]: http://static.zybuluo.com/Fangzheng1992/fqwefpud1jtwymii8m3rctcz/46.png
  [46]: http://static.zybuluo.com/Fangzheng1992/f72uvy525y0y9pdodumil3zd/47.png
  [47]: http://static.zybuluo.com/Fangzheng1992/nuwshgl3trrfczan32cburgm/48.png
  [48]: http://static.zybuluo.com/Fangzheng1992/x5f1s9bgkxg7qgoymgpc2wzp/49.png
  [49]: http://static.zybuluo.com/Fangzheng1992/u8nswygmwlzg85g0jgeykmxb/50.png
  [50]: http://static.zybuluo.com/Fangzheng1992/lalf2bb95lfl3h5siyt5mxyg/51.png
  [51]: http://static.zybuluo.com/Fangzheng1992/wrpx53fj48jkk6p3m7z75wdw/52.png
  [52]: http://static.zybuluo.com/Fangzheng1992/qg0ckknlvbrag7p234xvst55/53.png
  [53]: http://static.zybuluo.com/Fangzheng1992/7xeinr5w3nycremgj45b1q9r/54.png
  [54]: http://static.zybuluo.com/Fangzheng1992/7fcm1g90e2kmao6c20s76ow1/55.png
  [55]: http://static.zybuluo.com/Fangzheng1992/3lb9p3y8uneyfh3n9xtuzqjf/56.png
  [56]: http://static.zybuluo.com/Fangzheng1992/k50au3d2tm7kvzorv7xhv3it/57.png
  [57]: http://static.zybuluo.com/Fangzheng1992/hikpi7cb18tcc33akcbpevl5/58.png
  [58]: http://static.zybuluo.com/Fangzheng1992/y0ope7tx7s32gexm0yjwr2z0/59.png
  [59]: http://static.zybuluo.com/Fangzheng1992/5gtx0mu1xxwoe7y7rxi23gex/60.png
  [60]: http://static.zybuluo.com/Fangzheng1992/lbkcy5gfcy6l3ib3p3jl8hoe/61.png
  [61]: http://static.zybuluo.com/Fangzheng1992/1c4q8acdtsbfy2b694yv9n9x/62.png
  [62]: http://static.zybuluo.com/Fangzheng1992/6rjnwtr7gb01r6m4chrsarar/63.png
  [63]: http://static.zybuluo.com/Fangzheng1992/c3axfu3tax27u9fq59jpdun4/64.png
  [64]: http://static.zybuluo.com/Fangzheng1992/esh59fgdeowfiffehjwu19dz/65.png
  [65]: http://static.zybuluo.com/Fangzheng1992/lnec1h16paguuj4cx6xw4udn/66.png
  [66]: http://static.zybuluo.com/Fangzheng1992/7uirbg5ttcudxwziw35ldbv6/67.png
  [67]: http://static.zybuluo.com/Fangzheng1992/pax7c1unhfxtvasn9x7j3x6o/68.png
  [68]: http://static.zybuluo.com/Fangzheng1992/wz71gj8joi6qyv9zukmdy2m7/69.png
  [69]: http://static.zybuluo.com/Fangzheng1992/qso325oxgeeuxklw2ne730zv/70.png
  [70]: http://static.zybuluo.com/Fangzheng1992/jt0ho0exozhutp1kd1411c63/71.png
  [71]: http://static.zybuluo.com/Fangzheng1992/gh1edztgdvis9vg63meg5zws/72.png
  [72]: http://static.zybuluo.com/Fangzheng1992/x28oyz0e18yce8b50jjnt3n9/73.png
  [73]: http://static.zybuluo.com/Fangzheng1992/p8riohj3sxbw9keb6vq2316y/74.png
  [74]: http://static.zybuluo.com/Fangzheng1992/1u3iui1jcphcuvm5glkj0785/75.png
  [75]: http://static.zybuluo.com/Fangzheng1992/mqy6ud4rp824dkupg90yophq/76.png
  [76]: http://static.zybuluo.com/Fangzheng1992/chejk9izzbgjg9jz6s14deyr/77.png
  [77]: http://static.zybuluo.com/Fangzheng1992/zrjoggbdfvfl387kclm63sil/78.png
  [78]: http://static.zybuluo.com/Fangzheng1992/c15d1o9ym5zygqjaychkvjuu/79.png
  [79]: http://static.zybuluo.com/Fangzheng1992/evuk9uynwy698zngudg21s4b/80.png
  [80]: http://static.zybuluo.com/Fangzheng1992/8li4kh2zfd19ko7w9suiza12/81.png
  [81]: http://static.zybuluo.com/Fangzheng1992/70bg3fxszw3hhiy3gepiabyo/82.png
  [82]: http://static.zybuluo.com/Fangzheng1992/nlcspewv55132ocyv06ed6ci/83.png
  [83]: http://static.zybuluo.com/Fangzheng1992/zq9fkp91j28ylu4419oke71t/84.png
  [84]: http://static.zybuluo.com/Fangzheng1992/57znhvcuf3g7mm7ek3owl2xw/85.png
  [85]: http://static.zybuluo.com/Fangzheng1992/thihh8m575mgpaw6249dxk2d/86.png
  [86]: http://static.zybuluo.com/Fangzheng1992/h6ed4qaf58iw45f1abjpz43l/87.png
  [87]: http://static.zybuluo.com/Fangzheng1992/kz0tx8va6tb6x0yk5rl19haf/88.png
  [88]: http://static.zybuluo.com/Fangzheng1992/v7gejns4hnfp86eewak0qs46/89.png
  [89]: http://static.zybuluo.com/Fangzheng1992/odis9nnweftxd7v7n9jpv935/90.png
  [90]: http://static.zybuluo.com/Fangzheng1992/czj0yh2qmzm3oa2cz85xfgxw/91.png
  [91]: http://static.zybuluo.com/Fangzheng1992/36rwp9r59la38otqccu991v8/92.png
  [92]: http://static.zybuluo.com/Fangzheng1992/rnsjgosphmzwjbvwf97t2uw3/93.png
  [93]: http://static.zybuluo.com/Fangzheng1992/n80vfrtsitukasgjosycn9qh/94.png
  [94]: http://static.zybuluo.com/Fangzheng1992/thhxk2wvx1kz2x2h4l82tl1z/95.png
  [95]: http://static.zybuluo.com/Fangzheng1992/2l1an6yro2n28e9bmzayhxo2/96.png
  [96]: http://static.zybuluo.com/Fangzheng1992/rrfw4zsk7qzydbet11cod4dm/97.png
  [97]: http://static.zybuluo.com/Fangzheng1992/3mj29dlylltc3l84i6x41gip/98.png
  [98]: http://static.zybuluo.com/Fangzheng1992/xnb77aelgf475ypwsmorpi7s/99.png
  [99]: http://static.zybuluo.com/Fangzheng1992/wigrxpzhcc0g48e9dnqzzwup/100.png
  [100]: http://static.zybuluo.com/Fangzheng1992/68ncc0yp43iral0e2l1g6hth/101.png
  [101]: http://static.zybuluo.com/Fangzheng1992/p84327mnxxjjf9avqd3kp9y7/102.png
  [102]: http://static.zybuluo.com/Fangzheng1992/r9kgfoh4xchfjblmx2sfbvsm/103.png
  [103]: http://static.zybuluo.com/Fangzheng1992/3enw4qp67ppblszm0x1x79lt/104.png
  [104]: http://static.zybuluo.com/Fangzheng1992/8sryzcruf9rkovrz8oi9hf29/105.png
  [105]: http://static.zybuluo.com/Fangzheng1992/bjiyog1mki7xh5n5dhbmaqu3/106.png
  [106]: http://static.zybuluo.com/Fangzheng1992/4kcicadkkwvvucnl0easxaqr/107.png
  [107]: http://static.zybuluo.com/Fangzheng1992/b67w0fx2ufvh3zajd2l9z13e/108.png
  [108]: http://static.zybuluo.com/Fangzheng1992/w6virn13xjz4apyfydv8qxvq/109.png
  [109]: http://static.zybuluo.com/Fangzheng1992/alzw8mvrlbdir2gv49fvm80q/110.png
  [110]: http://static.zybuluo.com/Fangzheng1992/fct8i7u952rjlr7889o3ofbi/111.png
  [111]: http://static.zybuluo.com/Fangzheng1992/a0phx0m68d34qxt822bxwrcu/112.png
  [112]: http://static.zybuluo.com/Fangzheng1992/o73qf0x6m4oeuc2bv87bizxn/113.png
  [113]: http://static.zybuluo.com/Fangzheng1992/0ife8p8e4xu75zwn8norqxvh/114.png
  [114]: http://static.zybuluo.com/Fangzheng1992/fnj72c8tz8131h3uxoof7k9n/115.png