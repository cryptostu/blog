#Bitcoin Cash文件持久化详解

######作者：冉小龙、张勇
当我们打开Bitcoind项目的bitcoincash目录时，我们会看到如下的文件目录，这些文件究竟是什么，具体存储了哪些内容呢？下面我们将一一揭开其神秘面纱，本文只探讨了持久化过程中写文件操作，读文件操作我们后续还会有详细的解析。

####bitcoincash 文件夹：
![1.jpeg-88kB][1]

####blocks 文件夹：
![2.png-72.2kB][2]

####index 文件夹：
![3.png-138.2kB][3]

####chainstate 文件夹：
![4.png-131.6kB][4]

通过观察 bitcoincash 目录，我们可以发现， 比特币总共存储了以下内容：
| 文件名称          | 文件描述            | 存储形式      |
| ------------------|:--------------------|:-------------:|
| chainstate        | 存储utxo相关的数据  |LevleDB数据库  |
| blocks/index      |  存储blocks的元数据信息|  LevleDB数据库|
| blocks/blk***.dat |  存储blocks相关的数据信息，主要包括block header和txs| 磁盘文件 |
| blocks/rev***.dat |  存储block undo的数据主要包括每笔交易所花费out信息|  磁盘文件|

其中 block 的数据和 block 的 undo 数据是直接存储到disk上面的，block 的 index 数据和 utxo 的数据是写到 LevleDB 数据库中。

##LevelDB原理简述
为了方便理解 LevleDB 的目录存储结构，下面简述一下 LevleDB 的原理。
![5.png-137.4kB][5]
LevleDB 使用的是 LSMTree 的存储结构，其存储的逻辑大致如上图所示，具体步骤如下：

- 当往 LevleDB 中写入一条数据的时候，首先会将数据写入 log 文件，log 文件完成之后，再将数据写入内存（memtable）中。

- 当 memtable 中的数据写满之后，.log 文件会被锁定，同时生成 Immutable table 文件，该文件只支持读操作，不支持写和删除，这个时候，会重新生成 .log文 件和memtable 文件，新写入一条数据的时候，会重新写入空的 .log 文件和 memtable 中。

- LevleDB 后台调度会将 Immutable Memtable 的数据导出到磁盘，形成一个新的SSTable 文件。SSTable 就是由内存中的数据不断导出并进行 Compaction 操作后形成的，而且 SSTable的所有文件是一种层级结构，第一层为Level0，第二层为Level依次类推，层级逐渐增高，这也是为何称之为LevleDB的原因。

那么其中涉及的各个文件含义是什么呢？

####Current文件：
Current 文件是干什么的呢？这个文件的内容只有一个信息，就是记载当前的 manifest 文件名。因为在 LevleDB 的运行过程中，随着 Compaction 的进行，SSTable 文件会发生变化，会有新的文件产生，老的文件被废弃，Manifest 也会跟着反映这种变化，此时往往会新生成Manifest 文件来记载这种变化，而 Current 则用来指出哪个 Manifest 文件才是我们关心的那个 Manifest 文件。

####Manifest文件
Manifest 文件存储的是 xxx.ldb 文件的元数据信息，因为，我们只有 xxx.ldb 文件，我们并不知道它具体属于哪一个 level。这也是 Manifest 文件的作用，每次打开 DB 的时候，LevleDB 都会去创建这样一个文件并在其尾部追加后缀标识。该文件是以 append 的方式写入 disk 的。

####LOG文件
LevleDB 运行时的日志文件，方便用户查看。
####LOCK文件
它是使用文件实现的一个 DB 锁，告知用户，一个 LevleDB 的实例在一个进程范围内只允许被打开一次。
####xxx.ldb文件
这个文件是记录 LevleDB 的数据文件（区别与元数据文件），按照 KV 有序的形式写入数据库中。

level-0 的文件大小就是 memtable 文件做 compaction 之后的大小，level-1 10MB、level-2 100MB、level-3 1000MB 以此类推。
xxx.log 文件

我们上面说过，为了保证数据不丢失，在写数据之前会先写入 .log 文件，.log 文件存储的是一系列最近的更新，每个更新以 append 的方式追加到当前的 log 文件中，当 log 文件达到 4MB时会转化为一个有序的文件，并创建新的 log 文件来记录最近的更新。这个 log 文件中与上文中提到的 memtable 文件是互相映射的，当 memtable 文件被写入 level-0 后，对应的 log 文件会被删除，新的 log 文件会重新创建，对应新的 memtable，以此类推。

综上所述，我们可以看出，LevleDB 是存储模型中一个典型的数据与元数据分离存储的数据库。
##chainstate 文件夹
chainstate 是一个LevleDB的数据库，主要存储一些 utxo 和 tx 的元数据信息。存储 chainstate 的数据主要是用来去验证新进来的 blocks 和 tx 是否是合法的。如果没有这个操作，就意味着对于每一个被花费的 out 你都需要去进行全表扫描来验证。
![6.png-49.7kB][6]
如上图所示，utxo的数据主要存储于chainstate这个文件目录，由于要存储到LevleDB中，所以肯定是按照 key、value 的格式将数据准备好。
######Coin
![7.png-104.2kB][7]
![8.png-75.6kB][8]
如上所示：key总共包含三部分内容，1 字节的大写 `C` , 32 字节的 hash，4 字节的序列号。

value 是 coin 被序列化之后的值，具体如下：
![9.png-71.4kB][9]
coin 又包含了 txout 结构，具体如下：
![10.png-27kB][10]
对 nValue 和 scriptPubKey 采用了不同的压缩方式来进行序列化，如下：
![11.png-184.3kB][11]
####best block
![12.png-46.1kB][12]
比特币还往 chainstate 中记录了另一部分信息，首先去判断当前 block 的 hash 是否为 null，不为 null 的话，以 1 字节的大写 `B` 为 key，32 字节的 block hash 为value，写入 coin 数据库中。

总结：utxo 写入 disk 的数据库为：chainstate，写入数据分为两部分，第一部分：key是outpoin, 由<txid>+<tx out index>组成，其中txid是32字节，tx out index 是用var int的编码方式序列化value 为 coin 序列化之后的大小。第二部分：写入的 key 为 1 字节的DB_BEST_BLOCK 标识，value 为 32 字节的 block hash。
![13.png-81.8kB][13]
在 [bitcoin core 0.17][14] 的时候， chainstate 目录做了改动，多写了一部分数据进去，图示如下：
![14.png-108.2kB][15]
####Note:
在0.17的结构中，第一部分并不会存在很长时间，它只会在触发BatchWrite第一步写入，在整个coinsmap写完之后将这部分删除。
####index 文件夹: 
![15.png-55.3kB][16]
index 文件夹下记录的主要是 blocks 的 index 信息，block index 是block的元数据信息，其中包含和block header信息，高度，以及chain的信息；按照 utxo 存储的思路，我们再去寻找 blocks 中 index 的 key 和 value。
######reindex
![16.png-61.6kB][17]
index 中写的第一部分数据：key 是 1 字节的 DB_REINDEX_FLAG，value 是 1 字节的布尔值。用来标识是否需要进行 reindex 操作。
######txindex
![17.png-111kB][18]
index 中写的第二部分数据：key 是 1 字节的 DB_TXINDEX 加 32 字节的 hash，value 是序列化之后的 CDiskTxPos，它只有一个成员是，int 类型的 nTxOffset。这些是可选的，只有当'txindex' 被启用时才存在。 每个记录存储：

- 交易存储在哪个块文件号码中。
- 哪个文件中的交易所属的块被抵消存储在。
- 从该块的开始到该交易本身被存储的位置的偏移量。
######blockfileinfo
![18.png-193.1kB][19]
index 中写的第三部分数据：这部分数据是比较重要的。
######fileinfo
首先写入 fileinfo 数据，key 是 1 字节的 DB_BLOCK_FILES 加上 4 字节的文件编号，value 是 CBlockFileInfo 序列化后的数据。
######lastFile
其次写入 lastFile 信息，key 是 1 字节的 DB_LAST_BLOCK，value 是 4 字节的 nLastFile。
######blockindex
最后写入 blockindex 的信息，key 是 1 字节的 DB_BLOCK_INDEX 加上 32 字节的 blockhash value是CDiskBlockIndex序列化之后的数据。
######flag
![19.png-48.4kB][20]
index 中写的第四部分数据：key 是 1 字节的 DB_FLAG 加上 flag 的名字，value 是 1 字节的布尔值（1 为 true，0 为 false），可以打开或关闭各种类型的标志，目前定义的比如：TxIndex（是否启动交易索引）。
![20.png-189.8kB][21]
####block 文件夹
block 文件夹下主要存在两种文件，一种是 blk???.dat，用于存储 block，另一种是 rev???.dat，用于存储 undo block。 主要存储格式如下:
blk****.dat

存储 block 序列化的数据。
![21.png-224.5kB][22]
存储格式如下（按照先后顺序）：
![22.png-20.8kB][23]
######MessageStart
MessageMagic 在启动程序时定义，并且在不同网络中定义不同，MessageMagic 分为 netMagic 和 diskMagic :

Mainnet：
![23.png-101.4kB][24]
TestNet:
![25.png-101.8kB][25]
RegTestNet:
![26.png-102.6kB][26]
MessageMagic 是一个 4 byte 的数组，在写入数据的时候调用 FLATDATA 这个宏定义，具体如下：
![27.png-43.2kB][27]
FLATDATA 会将vector或者map这种数据结构中的元素按照数组的原始序列dump到disk上。
![28.png-53.1kB][28]
write() 函数的第一个参数代表要写入的数据的起始位置，第二个参数代表要写入数据的大小，pbegin 指向 vector 的起始位置，pend指向末尾元素 +1 的位置，所以在这里先写入了 4 byte 的 messageStart。
![29.png-48.4kB][29]
######BlockSize
BlockSize主要描述 Block 被序列化后的长度，为 4 byte。
![30.png-29.7kB][30]
######Block 序列化
block 序列化主要序列化两部分，一部分是 BlockHeader 结构，一部分为 transaction 的一个共享指针 vtx：
![31.png-69.8kB][31]
第一部分是 BlockHeader:
![32.png-126kB][32]
第二部分是 vtx：
![34.png-34.6kB][33]
CTransaction 主要序列化以下内容：
![35.png-67kB][34]
##总结
blk****.dat 文件首先写入 4 byte 的 messageMagic，其次写入 4 byte 的 block size，最后写入 block 被序列化之后的数据。
![36.png-54.1kB][35]
rev****.dat
存储 undoblock 序列化的数据。
![37.png-23.9kB][36]
MessageStart 和  UndoBlockSize 与 Block 中的相同。
######BlockUndo 序列化
BlockUndo 序列化只有 vtxundo 一个对象，vtxundo 是 CTxUndo 的一个 vector ，对其进行序列化操作如下：
![38.png-58kB][37]
CTxUndo 的序列化操作如下，其中 prevout 是一个 Coin 的 vector：
![39.png-155.2kB][38]
Coin的序列化操作如下：
![40.png-93.6kB][39]
Coin包含两部分内容，代码如下：
![41.png-99.5kB][40]
其中对 TxOut 的序列化如下，对 nValue 和 scriptPubKey 采用了不同的压缩方式来进行序列化：
![42.png-190.2kB][41]
######BlockUndoCheckSum
具体代码如下：
![43.png-99kB][42]
将 hashBlock 和 blockundo 的数据写入 CHashWriter 的接口中，获取 CHashWriter 的 hash ，并将 32 字节的 hash 值写入 undofile 文件中。
######blk****.dat 和 rev****.dat 的区别：
blk***.dat 和 rev***.dat 所存储的数据是不一样的，block 存储的是 block header 和 txs 序列化后的数据，undo block 存储的是 txout 被序列化后的数据。
######关于文件大小的一些问题：
1.blk.dat 的默认初始化大小是16M，最大为 128M， rev.dat 的默认初始化大小为 1M。
![44.png-143.9kB][43]
2.在导入 block 时，会去检查磁盘空间，必须大于 50M，否则就会 Disk space is low 
![45.png-50.7kB][44]
3.关于在 prune 时， 磁盘要求必须大于 550M：
![46.png-180.3kB][45]
bitcoin 要求必须保留 288 个 block， 按每个 block 1M 大小进行计算， 需要 288M， 还需要额外的 15% 的空间去存储 UNDO 的数据， 再加上以 20% 的孤块率， 大约需要 397M 的空间， 这是最低限度， 但我们还需要加上同步块的数据 blk.dat， 需要128M， 再加上约为 15% 的 undo data， 约为147M。 所以整个需要 147M + 397M=544M， 所以设置限度为 550M。


  [1]: http://static.zybuluo.com/Fangzheng1992/e0xqwq3ulgeo1lvkq5rvk7g7/1.jpeg
  [2]: http://static.zybuluo.com/Fangzheng1992/r9sjnmcjxb0eithmaakqjz7i/2.png
  [3]: http://static.zybuluo.com/Fangzheng1992/udu1gdndni38ythaaznm1q9l/3.png
  [4]: http://static.zybuluo.com/Fangzheng1992/iud0embllgh23k8u9vrk45nt/4.png
  [5]: http://static.zybuluo.com/Fangzheng1992/fgybcn74pjfrdjq2ta4sf2zp/5.png
  [6]: http://static.zybuluo.com/Fangzheng1992/90oaog6xnqrnfr0lgesgi75v/6.png
  [7]: http://static.zybuluo.com/Fangzheng1992/31qz32ug4gify2i7ujnd67yq/7.png
  [8]: http://static.zybuluo.com/Fangzheng1992/f31lwzc9jtg9f2v3zazqxjjg/8.png
  [9]: http://static.zybuluo.com/Fangzheng1992/xpjdyzb7g3uaejumvccuylwy/9.png
  [10]: http://static.zybuluo.com/Fangzheng1992/5qtm68y0dk8v7ieiydk0lg0u/10.png
  [11]: http://static.zybuluo.com/Fangzheng1992/clngjqb152khi3evf1s9y890/11.png
  [12]: http://static.zybuluo.com/Fangzheng1992/zs4y3iocx287rt2rhy0uqdvd/12.png
  [13]: http://static.zybuluo.com/Fangzheng1992/2u2rv4k97bb5llkev97a9lig/13.png
  [14]: https://github.com/bitcoin/bitcoin/commit/013a56aa1af985894b3eaf7c325647b0b74e4456
  [15]: http://static.zybuluo.com/Fangzheng1992/5jjzpxjcj7rx14zyoqf2yi0u/14.png
  [16]: http://static.zybuluo.com/Fangzheng1992/1vve4cjdrobn51lhcoa8jzqx/15.png
  [17]: http://static.zybuluo.com/Fangzheng1992/049x5pxb9fwoqzfctvus3ak8/16.png
  [18]: http://static.zybuluo.com/Fangzheng1992/ph69pij9bkr6s4v6ssh4pho0/17.png
  [19]: http://static.zybuluo.com/Fangzheng1992/6awbneyul3r54576ui7d2a3h/18.png
  [20]: http://static.zybuluo.com/Fangzheng1992/nw4hf1v1zr9xrxdyscf4qnvw/19.png
  [21]: http://static.zybuluo.com/Fangzheng1992/ncygvwd9sr4jq0dzr1hyi5hu/20.png
  [22]: http://static.zybuluo.com/Fangzheng1992/ulg9aw8y6cbtvt20umb12hqd/21.png
  [23]: http://static.zybuluo.com/Fangzheng1992/wkoawxjwtipmsrzfs5cna02d/22.png
  [24]: http://static.zybuluo.com/Fangzheng1992/ptychb0fnrhgw6hfrof8b93y/23.png
  [25]: http://static.zybuluo.com/Fangzheng1992/6ti4ict1ufvb0wck3u1tysq9/25.png
  [26]: http://static.zybuluo.com/Fangzheng1992/ywo0lofbqo15a36sx4iz1a50/26.png
  [27]: http://static.zybuluo.com/Fangzheng1992/gu2c7940rmb9a0ojqduas7m4/27.png
  [28]: http://static.zybuluo.com/Fangzheng1992/lt2xsptk8vdi009lz6on4x7o/28.png
  [29]: http://static.zybuluo.com/Fangzheng1992/vcfw7bgdv6xtrvju7ftbs5dd/29.png
  [30]: http://static.zybuluo.com/Fangzheng1992/5bmvr1r0g1xdvokpbw9f1oyx/30.png
  [31]: http://static.zybuluo.com/Fangzheng1992/ssxbfb6bxky7r7ypqalijh8l/31.png
  [32]: http://static.zybuluo.com/Fangzheng1992/5xubtk651teuoc1u00ncq8jz/32.png
  [33]: http://static.zybuluo.com/Fangzheng1992/hqe6p4ze4rto5uayb75fb1iq/34.png
  [34]: http://static.zybuluo.com/Fangzheng1992/01yoemvxpz8qwz51rrhhas9i/35.png
  [35]: http://static.zybuluo.com/Fangzheng1992/n3natb4ltq7hn8x20qidk2if/36.png
  [36]: http://static.zybuluo.com/Fangzheng1992/0cpqxicvq6a89xsra76jstk5/37.png
  [37]: http://static.zybuluo.com/Fangzheng1992/o4vi6lu2rx6twt3rx44w7lju/38.png
  [38]: http://static.zybuluo.com/Fangzheng1992/2cl12zhmon1gspnj30e8xdyj/39.png
  [39]: http://static.zybuluo.com/Fangzheng1992/4q25jcups81sadqahrtnzo4h/40.png
  [40]: http://static.zybuluo.com/Fangzheng1992/g63h74i1qny7qg3ykbstveij/41.png
  [41]: http://static.zybuluo.com/Fangzheng1992/2jiiw319x7smpypen8t9j5rk/42.png
  [42]: http://static.zybuluo.com/Fangzheng1992/1j9hyl9ism92iu2se5tayxcv/43.png
  [43]: http://static.zybuluo.com/Fangzheng1992/3fdfjva7ptggquwmtdit1tu0/44.png
  [44]: http://static.zybuluo.com/Fangzheng1992/pdo1bx6mln17hpadu6uiar66/45.png
  [45]: http://static.zybuluo.com/Fangzheng1992/xfsp6kjsx5ljpaizij7m2512/46.png