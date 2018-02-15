---
title: '[译]Bitcoin源码之levelDB'
date: 2018-02-15 08:41:09
tags:
---
_Jeff Dean, Sanjay Ghemawat_

leveldb库提供了一个持久性的键值存储，键和值是任意字节数组。keys 根据用户指定的比较器功能在 key-value store 内排序。

## Opening A Database（创建并打开数据库）

leveldb 数据库具有与文件系统目录相对应的名称。所有数据库的内容都存储在这个目录下。如有必要创建数据库，下面的例子演示如何打开数据库：

```c++
#include <cassert>
#include "leveldb/db.h"

leveldb::DB* db;
leveldb::Options options;
options.create_if_missing = true;
leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
assert(status.ok());
...
```

如果你想在数据库已经存在的情况下抛出错误的话，可以在`leveldb :: DB :: Open`调用之前的行添加以下内容：

```c++
options.error_if_exists = true;
```
## Status（状态）
你可能已经注意到上面的`leveldb :: Status`类型。leveldb的函数大都会返回这种类型的值，但是可能会遇到错误。你可以检查结果是否是正确的，如果错误，打印相关的错误消息：

```c++
leveldb::Status s = ...;
if (!s.ok()) cerr << s.ToString() << endl;
```

## Closing A Database（关闭数据库）

当数据库的所有操作都执行完成之后，只需删除数据库对象。Example:

```c++
... open the db as described above ...
... do something with db ...
delete db;
```
## Reads And Writes（读写操作）

数据库提供了 Put，Delete 和 Get 方法来修改/查询数据库。例如，下面的代码将存储在key1下的值移动到key2。
```c++
std::string value;
leveldb::Status s = db->Get(leveldb::ReadOptions(), key1, &value);
if (s.ok()) s = db->Put(leveldb::WriteOptions(), key2, value);
if (s.ok()) s = db->Delete(leveldb::WriteOptions(), key1);
```

## Atomic Updates（原子更新）

请注意，如果进程在 Put key2 之后但在 delete key1 之前死亡，多个Key下可能会保存相同的值。这样的问题可以通过使用 WriteBatch 类来避免：
```c++
#include "leveldb/write_batch.h"
...
std::string value;
leveldb::Status s = db->Get(leveldb::ReadOptions(), key1, &value);
if (s.ok()) {
  leveldb::WriteBatch batch;
  batch.Delete(key1);
  batch.Put(key2, value);
  s = db->Write(leveldb::WriteOptions(), &batch);
}
```
WriteBatch 可以将数据批量写入数据库，并且能够保证这些批量的批次可以按照顺序使用。请注意，就像我们上述使用delete的方式，如果key1与key2相同，我们不会错误地将该值丢弃。

除了它原子性的好处外，`WriteBatch`也可以将许多单一的修改放入同一批次来批量更新。

## Synchronous Writes（同步写）

默认情况下，每次写入leveldb都是异步的：当进程写入操作系统后就会返回。从操作系统的内存到底层磁盘的传输是异步进行的。对于特定的写操作，可以打开同步sync标志使写操作一直到数据被传输到底层存储器后再返回。（在Posix系统上，这是通过在写操作返回之前调用`fsync（...）`或`fdatasync（...）`或`msync（...，MS_SYNC）`来实现的。）

```c++
leveldb::WriteOptions write_options;
write_options.sync = true;
db->Put(write_options, ...);
```

异步写入速度通常是同步写入速度的千倍以上。异步写入的缺点是当机器宕机时，可能导致最后几次更新丢失。如果是在写入过程中的宕机（而非重新启动），即使sync设置为false，更新操作也会认为已经将更新从内存中推送到了操作系统。

通常可以安全地使用异步写入。比如，当加载大量数据到数据库中时，可以通过在宕机后重新启动批量加载来处理丢失的更新。有一个可用的混合方案，将多次写入的第N次写入设置为同步的，并在宕机重启后的情况下，批量加载由前一次运行的最后一次同步写入之后重新开始。（同步写入时可以更新描述宕机后批量加载重新开始的标记。）

`WriteBatch`提供了异步写入的替代方法。可以将多个更新放置在同一个WriteBatch中，并和同步写入（即write_options.sync设置为true）一起进行。同步写入的额外开销将在批处理中的所有写入之间进行分摊。

## Concurrency（并发）

数据库一次只能由一个进程打开。leveldb的实现是从操作系统层面获取锁来防止误操作。在一个进程中，相同的`leveldb :: DB`对象可以安全地被多个并发线程共享。即，在同一个数据库中，无需任何外部同步（leveldb会自动执行所需的同步），不同的线程就可以写入、取出 interior 或调用 Get方法。但是其他对象（如迭代器和`WriteBatch`）可能需要外部同步。如果两个线程共享这样的对象，它们必须使用自己的协议锁来保护自己的访问。
## Iteration（迭代器）

以下示例演示如何在数据库中打印所有键值对。

```c++
leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
for (it->SeekToFirst(); it->Valid(); it->Next()) {
  cout << it->key().ToString() << ": "  << it->value().ToString() << endl;
}
assert(it->status().ok());  // Check for any errors found during the scan
delete it;
```

以下变体显示了如何仅处理 range[start，limit）中的Key：

```c++
for (it->Seek(start); it->Valid() && it->key().ToString() < limit; it->Next()) {
  ...
}
```

也可以按相反的顺序处理条目。（注意：反向迭代可能比正向迭代慢一些。）

```c++
for (it->SeekToLast(); it->Valid(); it->Prev()) {
  ...
}
```


## Snapshots（快照）

snapshot 在 key-value 存储的整个状态中提供一致的只读视图。`ReadOptions :: snapshot`如果是 non-NULL，表示读操作应该在特定版本的DB状态下运行。如果`ReadOptions :: snapshot`为NULL，则读操作将在当前状态的隐式 snapshot 上运行。

Snapshots 由 `DB::GetSnapshot()` 方法创建:

```c++
leveldb::ReadOptions options;
options.snapshot = db->GetSnapshot();
... apply some updates to db ...
leveldb::Iterator* iter = db->NewIterator(options);
... read using iter to view the state when the snapshot was created ...
delete iter;
db->ReleaseSnapshot(options.snapshot);
```

请注意，当不再需要snapshot时，应该使用`DB :: ReleaseSnapshot`接口来释放快照。这可以减少为维持读取 snapshot 而维护的状态的开销。

## Slice（切片）

上面的`it-> key（）`和`it-> value（）`调用的返回值是`leveldb :: Slice`类型的实例。Slice是一个简单的结构，它包含一个长度和一个指向外部字节数组的指针。因为我们不需要复制潜在的大键和值，所以返回一个Slice是返回`std :: string`的更便宜的方法。另外，leveldb方法不会返回以空字符结尾的C风格字符串，因为leveldb键和值允许包含“\ 0”字节。

C ++字符串和以空字符结尾的C风格的字符串可以很容易地转换为Slice：

```c++
leveldb::Slice s1 = "hello";

std::string str("world");
leveldb::Slice s2 = str;
```
切片可以很容易地转换回C ++字符串：

```c++
std::string str = s1.ToString();
assert(str == std::string("hello"));
```
使用切片时要小心，因为调用者要确保在切片使用时切片点保持有效的外部字节数组。例如，以下是错误示例：

```c++
leveldb::Slice slice;
if (...) {
  std::string str = ...;
  slice = str;
}
Use(slice);
```

当if语句超出范围时，str 将被销毁，slice 的后备存储将消失。

## Comparators（比较器）

前面的例子使用了按照key的默认排序功能，按字典顺序排列字节。但是，您可以在打开数据库时提供自定义比较器。例如，假设每个数据库key由两个数字组成，我们应该用第一个数字排序，第二个数字打破关系。首先，定义表达这些规则的“leveldb :: Comparator”的适当子类：
```c++
class TwoPartComparator : public leveldb::Comparator {
 public:
  // Three-way comparison function:
  //   if a < b: negative result
  //   if a > b: positive result
  //   else: zero result
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const {
    int a1, a2, b1, b2;
    ParseKey(a, &a1, &a2);
    ParseKey(b, &b1, &b2);
    if (a1 < b1) return -1;
    if (a1 > b1) return +1;
    if (a2 < b2) return -1;
    if (a2 > b2) return +1;
    return 0;
  }

  // Ignore the following methods for now:
  const char* Name() const { return "TwoPartComparator"; }
  void FindShortestSeparator(std::string*, const leveldb::Slice&) const {}
  void FindShortSuccessor(std::string*) const {}
};
```

现在用这个自定义比较器创建一个数据库：
```c++
TwoPartComparator cmp;
leveldb::DB* db;
leveldb::Options options;
options.create_if_missing = true;
options.comparator = &cmp;
leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
...
```

### Backwards compatibility（向后兼容性）

比较器的 Name 方法的结果会在创建时附加到数据库，并在后续每个打开的数据库上进行检查。如果名字改变，`leveldb :: DB :: Open`调用将失败。因此，当且仅当新的Key格式和比较函数与现有的数据库不兼容时才需要更改名称，当然丢弃所有已有数据库的内容是可以的。

然而，你可以通过一些预先规划来逐步演变你的Key格式。例如，您可以在每个Key的末尾存储版本号（一个字节应该足以满足大多数用途）。当你希望切换到一个新的Key格式时（例如，向由TwoPartComparator处理的Key添加一个可选的第三部分）:

* （a）保持相同的比较器名称
* （b）增加新Key的版本号
* （c）更改比较器功能，以便使用Key中的版本号来决定如何解释它们。
## Performance（性能）

性能可以通过改变`include/leveldb/options.h`中定义的类型的默认值来调整。

### Block size（块大小）

leveldb将相邻的Key组合在一起成为相同的块，并且这样的块是传送到磁盘和从磁盘传送的单位。默认的块大小大约是4096个未压缩的字节。主要对数据库内容进行批量扫描的应用程序可能希望增加此大小。如果性能测量结果显示有改善，则应用程序执行很多小值的点读取操作可能希望切换到较小的块大小。使用小于一千字节的块，或者大于几兆字节，没有太大的好处。另外请注意，压缩将在更大的块大小时更有效。

### Compression（压缩）
在写入永久性存储之前，每个块都被单独压缩。由于默认压缩方法非常快，因此压缩默认为打开状态，并且会自动禁用不可压缩数据。在极少数情况下，应用程序可能希望完全禁用压缩，但只有在基准测试显示性能得到提高时才应该这样做：

```c++
leveldb::Options options;
options.compression = leveldb::kNoCompression;
... leveldb::DB::Open(options, name, ...) ....
```

### Cache（缓存）

数据库的内容存储在文件系统中的一组文件中，每个文件存储一系列压缩块。如果options.cache不为NULL，则用于缓存经常使用的未压缩块内容。

```c++
#include "leveldb/cache.h"

leveldb::Options options;
options.cache = leveldb::NewLRUCache(100 * 1048576);  // 100MB cache
leveldb::DB* db;
leveldb::DB::Open(options, name, &db);
... use the db ...
delete db
delete options.cache;
```

请注意，缓存保存未压缩的数据，因此应根据应用程序级别的数据大小进行调整，而不能从压缩中减少。（压缩块的高速缓存留给操作系统缓冲区高速缓存，或由客户端提供的任何自定义Env实现。）

在执行批量读取时，应用程序可能希望禁用高速缓存，以便批量读取所处理的数据不会取代大部分高速缓存的内容。每个迭代器选项可以用来实现这一点：

```c++
leveldb::ReadOptions options;
options.fill_cache = false;
leveldb::Iterator* it = db->NewIterator(options);
for (it->SeekToFirst(); it->Valid(); it->Next()) {
  ...
}
```
### Key Layout（键的布局方式）

请注意，磁盘传输和缓存的单位是一个块。相邻的Key（根据数据库排序顺序）通常会放在同一个块中。因此，应用程序可以通过将彼此靠近的Key放置在一起，并将不经常使用的Key放置在Key空间的单独区域中来改善其性能。

例如，假设我们正在leveldb之上实现一个简单的文件系统。我们可能希望存储的条目类型是：

    filename -> permission-bits, length, list of file_block_ids
    file_block_id -> data

我们可能希望用一个字母（如'/'）和用不同的字母（比如'0'）的`file_block_id`作为文件名字母的前缀，这样只扫描元数据就不会强制我们获取和缓存庞大的文件内容。

### Filters（过滤器）

由于leveldb数据在磁盘上的组织方式，一个`Get()`调用可能涉及从磁盘读取多个数据。可选的 FilterPolicy 机制可用于大幅减少磁盘读取次数。

```c++
leveldb::Options options;
options.filter_policy = NewBloomFilterPolicy(10);
leveldb::DB* db;
leveldb::DB::Open(options, "/tmp/testdb", &db);
... use the database ...
delete db;
delete options.filter_policy;
```

上面的代码将基于布隆过滤器的过滤策略与数据库相关联。基于布隆过滤器的过滤依赖于每个Key在内存中保留一些数据位（在这种情况下，每个Key10位，因为这是我们传递给`NewBloomFilterPolicy`的参数）。这个过滤器可以将`Get()`调用所需的不必要的磁盘读取次数减少大约100倍。增加每个按Key的位数将导致更大的减少，但会增加内存使用量。我们建议那些工作集中不适合内存的应用程序，并且执行大量的随机读操作来设置过滤策略。

如果您使用自定义比较器，则应确保您使用的过滤策略与您的比较器兼容。例如，考虑一个比较器，比较Key时会忽略尾随空格。`NewBloomFilterPolicy`不能与这样的比较器一起使用。相反，应用程序应该提供一个自定义过滤器策略，也会忽略尾随空格。例如：

```c++
class CustomFilterPolicy : public leveldb::FilterPolicy {
 private:
  FilterPolicy* builtin_policy_;

 public:
  CustomFilterPolicy() : builtin_policy_(NewBloomFilterPolicy(10)) {}
  ~CustomFilterPolicy() { delete builtin_policy_; }

  const char* Name() const { return "IgnoreTrailingSpacesFilter"; }

  void CreateFilter(const Slice* keys, int n, std::string* dst) const {
    // Use builtin bloom filter code after removing trailing spaces
    std::vector<Slice> trimmed(n);
    for (int i = 0; i < n; i++) {
      trimmed[i] = RemoveTrailingSpaces(keys[i]);
    }
    return builtin_policy_->CreateFilter(&trimmed[i], n, dst);
  }
};
```

高级应用程序可能会提供一个不使用布隆过滤器的过滤器策略，但使用其他一些机制来汇总一组Key。有关详细信息，请参阅`leveldb/filter_policy.h`。

## Checksums（校验和）

leveldb将校验和与其存储在文件系统中的所有数据关联起来。提供了两个单独的控件，用于验证这些校验和的激进程度：

`ReadOptions :: verify_checksums`可以设置为true，以强制校验和验证代表特定读取从文件系统读取的所有数据。默认情况下，不进行此类验证。

在打开数据库之前，可以将`Options :: paranoid_checks`设置为true，以便在数据库检测到内部损坏时立即引发错误。根据数据库的哪个部分被损坏，数据库打开时或者稍后由另一个数据库操作引发错误。默认情况下，偏执检查是关闭的，这样即使数据库的一部分持久性存储已被损坏，数据库也可以被使用。

如果一个数据库被损坏（也许在打开偏执检查时不能打开），`leveldb :: RepairDB`函数可能被用来恢复尽可能多的数据。

## Approximate Sizes（预估大小）

`GetApproximateSizes`方法可用于获取一个或多个Key范围使用的文件系统空间的近似字节数。

```c++
leveldb::Range ranges[2];
ranges[0] = leveldb::Range("a", "c");
ranges[1] = leveldb::Range("x", "z");
uint64_t sizes[2];
leveldb::Status s = db->GetApproximateSizes(ranges, 2, sizes);
```
前面的调用将把`sizes [0]`设置为Key范围`[a..c]`和`sizes [1]`使用的文件系统空间的近似字节数， Key范围`[x..z]`。

## Environment（环境）

所有由leveldb实现发布的文件操作（和其他操作系统调用）都通过`leveldb :: Env`对象进行路由。复杂的客户可能希望提供自己的Env实施以获得更好的控制。例如，应用程序可能会在文件IO路径中引入虚假延迟，以限制leveldb对系统中其他活动的影响。
```c++
class SlowEnv : public leveldb::Env {
  ... implementation of the Env interface ...
};

SlowEnv env;
leveldb::Options options;
options.env = &env;
Status s = leveldb::DB::Open(options, ...);
```
## Porting（移植）

leveldb可以通过提供`leveldb / port / port.h`输出的类型/方法/函数的平台特定实现移植到一个新的平台上。有关更多详细信息，请参阅`leveldb / port / port_example.h`。

另外，新平台可能需要一个新的默认`leveldb :: Env`实现。例如，参见`leveldb / util / env_posix.h`。

## Other Information

有关leveldb实现的细节可以在以下文档中找到：

1. [Implementation notes](https://github.com/bitcoin/bitcoin/blob/master/src/leveldb/doc/impl.md)
2. [Format of an immutable Table file](https://github.com/bitcoin/bitcoin/blob/master/src/leveldb/doc/table_format.md)
3. [Format of a log file](https://github.com/bitcoin/bitcoin/blob/master/src/leveldb/doc/log_format.md)

## 引用
* https://github.com/bitcoin/bitcoin/blob/master/src/leveldb/doc/index.md

***
本文由`Copernicus团队 冉小龙`翻译，转载无需授权。

