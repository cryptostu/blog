# bitcoin multithread verify script

## 多线程脚本检查启动
```
bool AppInitMain(Config &config, boost::thread_group &threadGroup, CScheduler &scheduler) {
    ...
    // 脚本检查线程； 启动多个脚本检查线程
    if (nScriptCheckThreads) {
        for (int i = 0; i < nScriptCheckThreads - 1; i++) {
            threadGroup.create_thread(&ThreadScriptCheck);
        }
    }
    ...
}
static CCheckQueue<CScriptCheck> scriptcheckqueue(128);     

void ThreadScriptCheck() {
    RenameThread("bitcoin-scriptch");
    scriptcheckqueue.Thread();  
}
```
此处使用了boost的线程库，在绑定的线程函数`ThreadScriptCheck`中，调用一个全局状态的任务队列`scriptcheckqueue`；每个线程都去该队列中去任务，当队列中无任务可执行时，线程被条件变量阻塞。

## 任务队列
```
//模板类，执行的验证任务由T标识，T都必须提供一个重载的operator()方法，并且反回一个bool。
//默认为主线程push 批量任务到队列中，其他的工作线程去处理这些任务，当主线程push完任务后，也去处理这些任务，直到任务队列全部处理完毕。
template <typename T> class CCheckQueue {
private:
    // 互斥锁保护内部的状态
    boost::mutex mutex;
    // 在没有工作时，工作线程阻塞条件变量。
    boost::condition_variable condWorker;
    // 在没有工作时，master线程阻塞条件变量。
    boost::condition_variable condMaster;
    // 要处理元素的队列。
    std::vector<T> queue;
    // 空闲的工作线程数量(包含主线程)
    int nIdle;
    // 总的工作线程的数量，包含主线程
    int nTotal;
    // 临时评估结果
    bool fAllOk;
    // 还有多少验证任务没有完成。包括不再排队，但仍在工作线程自己的批次中的任务数量。
    unsigned int nTodo;
    // 是否需要退出。
    bool fQuit;
    // 每个批次最大的元素处理数量
    unsigned int nBatchSize;
    
    //内部函数，队列中的任务在此处将被全部处理。
    bool Loop(bool fMaster = false);
public:
    //! Create a new check queue
    CCheckQueue(unsigned int nBatchSizeIn)
            : nIdle(0), nTotal(0), fAllOk(true), nTodo(0), fQuit(false),
              nBatchSize(nBatchSizeIn) {}

    //! Worker thread；此处为工作线程进行调用，
    void Thread() { Loop(); }
    //在代码中主要是主线程进行调用，等待任务全部处理完毕，返回处理结果
    bool Wait() { return Loop(true); }
    //! 给类的内部队列批量添加任务；本次操作受锁保护；并更新所有的状态。
    void Add(std::vector<T> &vChecks) {
        boost::unique_lock<boost::mutex> lock(mutex);
        //将任务添加至 内部队列中
        for (T &check : vChecks) {
            queue.push_back(std::move(check));
        }
        // 更新未完成的任务数量
        nTodo += vChecks.size();
        //如果刚添加的任务数量为1，只唤醒一个工作线程去处理；否则，唤醒全部工作线程。
        if (vChecks.size() == 1) {
            condWorker.notify_one();
        } else if (vChecks.size() > 1) {
            condWorker.notify_all();
        }
    }
    // 任务处理队列是否处于休眠状态
    bool IsIdle() {
        boost::unique_lock<boost::mutex> lock(mutex);
        return (nTotal == nIdle && nTodo == 0 && fAllOk == true);
    }
    ~CCheckQueue() {}
}

//fMaster : 标识是否为主线程在调用
bool CCheckQueue::Loop(bool fMaster = false){
    // 根据参数，选择条件变量
    boost::condition_variable &cond = fMaster ? condMaster : condWorker;

    std::vector<T> vChecks;     //临时任务队列
    vChecks.reserve(nBatchSize);
    unsigned int nNow = 0;      
    bool fOk = true;
    do {
        {
            boost::unique_lock<boost::mutex> lock(mutex);       //自动管理资源锁；
            // first do the clean-up of the previous loop run (allowing us
            // to do it in the same critsect)  对上一次的运行状态做清理。
            if (nNow) {
                fAllOk &= fOk;
                nTodo -= nNow;
                if (nTodo == 0 && !fMaster)
                    // We processed the last element; inform the master it
                    // can exit and return the result  处理最后一个元素，通知master它可以退出，并且返回结果
                    condMaster.notify_one();
            } else {
                // nNow == 0,标识该线程第一次运行，即线程的数量又增加了一个。
                nTotal++;
            }
           
            // 该对象的任务队列为空时，进入下列条件；
            // 下面处理分为两种情况：1. 任务全部完成后，将主线程/或退出状态时，退出主线程或所有的线程。
            //  2. 任务全部完成后，将子线程
            while (queue.empty()) {
                // 当验证任务为0，且需要退出时，
                if ((fMaster || fQuit) && nTodo == 0) {
                    nTotal--;
                    bool fRet = fAllOk;     //fAllOk : 最新的临时评估结果。对该值做缓存，然后退出。
                    // reset the status for new work later
                    if (fMaster) fAllOk = true;
                    // return the current status        //返回当前的状态。
                    return fRet;
                }
                nIdle++;
                cond.wait(lock); // wait   此处配合条件变量使用。进行线程间的同步;
                nIdle--;
            }
        
            // 获取当前每个线程每个任务循环时处理的任务数量；
            nNow = std::max(
                1U, std::min(nBatchSize, (unsigned int)queue.size() /
                                             (nTotal + nIdle + 1)));
            //从类的任务队列中 向临时队列中添加任务。
            vChecks.resize(nNow);
            for (unsigned int i = 0; i < nNow; i++) {
                // 想让锁的时间尽可能的短，所以采用这种方法(move)来从 类的队列中拿到任务，而不是采用拷贝的方式。
                vChecks[i].swap(queue.back());
                queue.pop_back();       //将放到局部队列中的任务清除
            }
            fOk = fAllOk;
        }
        // execute work； 执行本线程刚分到的工作。
        for (T &check : vChecks) {
            if (fOk) fOk = check();
        }
        // 执行完后，清空临时任务的集合。继续下次循环。
        vChecks.clear();
    } while (true);
    
}
```

上述是队列的实现：主要的任务处理是在`Loop()`函数中;
该队列会进行两种调用，来处理队列中的任务:
    >* 向添加任务后：自动唤醒阻塞的工作线程去处理添加的任务；细节请看：`void Add(std::vector<T> &vChecks) `
    >* 主线程添加完任务后，调用`bool Wait()`，也去处理队列中的任务，队列中的全部任务处理完后，主线程退出。
    
## 采用RAII机制去操作任务队列
```
template <typename T> class CCheckQueueControl {
private:
    CCheckQueue<T> *pqueue;
    bool fDone;

public:
    CCheckQueueControl(CCheckQueue<T> *pqueueIn)
        : pqueue(pqueueIn), fDone(false) {
        // 用来构建该对象的任务队列只能是nil, 或者队列中无任务。
        // 因为创建的该对象在析构时会调用任务队列的wait()方法去处理完队列中所有的任务，然后退出：
        if (pqueue != nullptr) {
            bool isIdle = pqueue->IsIdle();     //获取该队列是否空闲
            assert(isIdle);
        }
    }
    
    //处理完队列中的所有任务后，该方法退出，并返回这些任务的处理结果
    bool Wait() {
        if (pqueue == nullptr) return true;
        bool fRet = pqueue->Wait();     //执行完所有的任务后，
        fDone = true;
        return fRet;
    }

    // 向 CCheckQueue 中添加任务；唤醒子线程去处理。
    void Add(std::vector<T> &vChecks) {
        if (pqueue != nullptr) pqueue->Add(vChecks);
    }
    // 对象析构时，调用wait()方法保证了该队列中的所有任务都被处理。
    ~CCheckQueueControl() {
        if (!fDone) Wait();
    }
};
```
该类主要是用来管理 `CCheckQueue`对象；采用RAII机制，保证每次析构该类的对象时，`CCheckQueue`中的任务队列被全部处理。

## 
```
//将该区块链接到当前激活链上，并更新UTXO集合。
// block(in):将要链接到激活链上的区块(带有完整数据)； pindex(in):该链接块对应的索引；
static bool ConnectBlock(const Config &config, const CBlock &block, CValidationState &state, CBlockIndex *pindex,
    CCoinsViewCache &view, const CChainParams &chainparams, bool fJustCheck = false) {
    ...
    
    CCheckQueueControl<CScriptCheck> control(fScriptChecks ? &scriptcheckqueue : nullptr);
        ...
    for (size_t i = 0; i < block.vtx.size(); i++) {
        ...
        if (!tx.IsCoinBase()) {
            Amount fee = view.GetValueIn(tx) - tx.GetValueOut();
            nFees += fee.GetSatoshis();

            // Don't cache results if we're actually connecting blocks (still
            // consult the cache, though).
            bool fCacheResults = fJustCheck;

            std::vector<CScriptCheck> vChecks;
            // 检查交易交易,并将的交易每个输入构造成对应的可检查对象`CScriptCheck`，放入临时集合中，然后添加进任务队列中。
            if (!CheckInputs(tx, state, view, fScriptChecks, flags,
                             fCacheResults, fCacheResults,
                             PrecomputedTransactionData(tx), &vChecks)) {
                return error("ConnectBlock(): CheckInputs on %s failed with %s",
                             tx.GetId().ToString(), FormatStateMessage(state));
            }

            control.Add(vChecks);       //向验证线程中添加任务；添加完后，此时其他的任务线程就开始执行。
        }
        ...
    }
    
    ...
}
```
在该方法中：使用了全局对象`scriptcheckqueue`去构造了一个临时的管理对象，并通过该管理对象来操作全局任务队列：添加任务，执行任务；当该临时的管理对象析构时，会调用wait()方法，加入任务处理，处理完所有任务后，该对象析构完成。

## CScriptCheck(根据每个交易输入构造的检查任务)
```
class CScriptCheck {
private:
    CScript scriptPubKey;       //锁定脚本(即该验证交易的某个引用输出对应的锁定脚本)
    Amount amount;              //上述锁定脚本对应 的金额(即花费的UTXO的金额)
    const CTransaction *ptxTo;  //正在花费的交易，即要检查的交易
    unsigned int nIn;           //要检查该交易的第几个输入；
    uint32_t nFlags;            //检查标识
    bool cacheStore;
    ScriptError error;          //验证出错的原因
    PrecomputedTransactionData txdata;  

public:
    CScriptCheck()
        : amount(0), ptxTo(0), nIn(0), nFlags(0), cacheStore(false),
          error(SCRIPT_ERR_UNKNOWN_ERROR), txdata() {}

    CScriptCheck(const CScript &scriptPubKeyIn, const Amount amountIn,
                 const CTransaction &txToIn, unsigned int nInIn,
                 uint32_t nFlagsIn, bool cacheIn,
                 const PrecomputedTransactionData &txdataIn)
        : scriptPubKey(scriptPubKeyIn), amount(amountIn), ptxTo(&txToIn),
          nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn),
          error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) {}

    bool operator()();    //此处重载了()运算符，执行脚本检查操作；详情见下集：脚本验证

    // 采用这种方式对新对象进行赋值，避免拷贝赋值，节省时间。
    void swap(CScriptCheck &check) {
        scriptPubKey.swap(check.scriptPubKey);
        std::swap(ptxTo, check.ptxTo);
        std::swap(amount, check.amount);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};
```
***
本文由 `Copernicus团队 姚永芯`写作，转载无需授权。
