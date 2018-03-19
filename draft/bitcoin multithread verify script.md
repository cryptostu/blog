# bitcoin multithread verify script

## 多线程脚本检查启动
```
bool AppInitMain(Config &config, boost::thread_group &threadGroup, CScheduler &scheduler) {
    ...
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
在 `AppInitMain` 中根据选项，创建多个线程。
此处使用了boost的线程库，在绑定的线程函数`ThreadScriptCheck`中，调用一个全局状态的任务队列`scriptcheckqueue`；每个线程都去该队列中去任务，当队列中无任务可执行时，线程被条件变量阻塞。

## 任务队列
```
template <typename T> class CCheckQueue {
private:
    boost::mutex mutex;
    boost::condition_variable condWorker;
    boost::condition_variable condMaster;
    std::vector<T> queue;
    int nIdle;
    int nTotal;
    bool fAllOk;
    unsigned int nTodo;
    bool fQuit;
    unsigned int nBatchSize;
    bool Loop(bool fMaster = false);
public:
    //! Create a new check queue
    CCheckQueue(unsigned int nBatchSizeIn)
            : nIdle(0), nTotal(0), fAllOk(true), nTodo(0), fQuit(false),
              nBatchSize(nBatchSizeIn) {}

    void Thread() { Loop(); }
    
    bool Wait() { return Loop(true); }
    
    void Add(std::vector<T> &vChecks) {
        boost::unique_lock<boost::mutex> lock(mutex);
    
        for (T &check : vChecks) {
            queue.push_back(std::move(check));
        }
    
        nTodo += vChecks.size();
        if (vChecks.size() == 1) {
            condWorker.notify_one();
        } else if (vChecks.size() > 1) {
            condWorker.notify_all();
        }
    }
    bool IsIdle() {
        boost::unique_lock<boost::mutex> lock(mutex);
        return (nTotal == nIdle && nTodo == 0 && fAllOk == true);
    }
    ~CCheckQueue() {}
}

bool CCheckQueue::Loop(bool fMaster = false){
    boost::condition_variable &cond = fMaster ? condMaster : condWorker;

    std::vector<T> vChecks; 
    vChecks.reserve(nBatchSize);
    unsigned int nNow = 0;      
    bool fOk = true;
    do {
        {
            boost::unique_lock<boost::mutex> lock(mutex);       
            // first do the clean-up of the previous loop run (allowing us
            // to do it in the same critsect)  
            if (nNow) {
                fAllOk &= fOk;
                nTodo -= nNow;
                if (nTodo == 0 && !fMaster)
                    // We processed the last element; inform the master it
                    // can exit and return the result  
                    condMaster.notify_one();
            } else {
                nTotal++;
            }
           
            while (queue.empty()) {
                if ((fMaster || fQuit) && nTodo == 0) {
                    nTotal--;
                    bool fRet = fAllOk;     
                    // reset the status for new work later
                    if (fMaster) fAllOk = true;
                    return fRet;
                }
                nIdle++;
                cond.wait(lock); 
                nIdle--;
            }
            nNow = std::max(
                1U, std::min(nBatchSize, (unsigned int)queue.size() /
                                             (nTotal + nIdle + 1)));
            vChecks.resize(nNow);
            for (unsigned int i = 0; i < nNow; i++) {
                vChecks[i].swap(queue.back());
                queue.pop_back();       //将放到局部队列中的任务清除
            }
            fOk = fAllOk;
        }
        // execute work； 执行本线程刚分到的工作。
        for (T &check : vChecks) {
            if (fOk) fOk = check();
        }
        vChecks.clear();
    } while (true);
    
}
```
`boost::mutex mutex;`: 互斥锁保护内部的状态

`boost::condition_variable condWorker;`: 在没有工作时，工作线程阻塞条件变量。

`boost::condition_variable condMaster;`: 在没有工作时，master线程阻塞条件变量。

`std::vector<T> queue;`: 要处理元素的队列。

`int nIdle;`: 空闲的工作线程数量(包含主线程)

`int nTotal;`: 总的工作线程的数量，包含主线程

`bool fAllOk;`: 临时评估结果

`unsigned int nTodo;`: 还有多少验证任务没有完成。包括不再排队，但仍在工作线程自己的批次中的任务数量。

`bool fQuit;`: 是否需要退出。

`unsigned int nBatchSize;`: 每个批次最大的元素处理数量
    
模板类，执行的验证任务由T标识，T都必须提供一个重载的operator()方法，并且反回一个bool。
默认为主线程push 批量任务到队列中，其他的工作线程去处理这些任务，当主线程push完任务后，也去处理这些任务，直到任务队列全部处理完毕。
上述是队列的实现：主要的任务处理是在`Loop()`函数中;
该队列会进行两种调用，来处理队列中的任务:
    >* 添加任务后：自动唤醒阻塞的工作线程去处理添加的任务；细节请看：`void Add(std::vector<T> &vChecks) `
    >* 主线程添加完任务后，调用`bool Wait()`，也去处理队列中的任务，队列中的全部任务处理完后，主线程退出。
    //! 给类的内部队列批量添加任务；本次操作受锁保护；并更新所有的状态。
    
`void Add()`:给类的内部队列批量添加任务；本次操作受锁保护；并更新所有的状态。
    >* 如果刚添加的任务数量为1，只唤醒一个工作线程去处理；否则，唤醒全部工作线程。
    
    
## 采用RAII机制去操作任务队列
```
template <typename T> class CCheckQueueControl {
private:
    CCheckQueue<T> *pqueue;
    bool fDone;

public:
    CCheckQueueControl(CCheckQueue<T> *pqueueIn)
        : pqueue(pqueueIn), fDone(false) {
        if (pqueue != nullptr) {
            bool isIdle = pqueue->IsIdle();    
            assert(isIdle);
        }
    }
    
    bool Wait() {
        if (pqueue == nullptr) return true;
        bool fRet = pqueue->Wait();    
        fDone = true;
        return fRet;
    }

    void Add(std::vector<T> &vChecks) {
        if (pqueue != nullptr) pqueue->Add(vChecks);
    }
    
    ~CCheckQueueControl() {
        if (!fDone) Wait();
    }
};
```
该类主要是用来管理 `CCheckQueue`对象；采用RAII机制，保证每次析构该类的对象时，`CCheckQueue`中的任务队列被全部处理。
用来构建该对象的任务队列只能是nil, 或者队列中无任务。
因为创建的该对象在析构时会调用任务队列的wait()方法去处理完队列中所有的任务，然后退出。
`bool Wait()`处理完队列中的所有任务后，该方法退出，并返回这些任务的处理结果。
`void Add()`向 CCheckQueue 中添加任务；唤醒子线程去处理。
`~CCheckQueueControl()`对象析构时，调用wait()方法保证了该队列中的所有任务都被处理。


## 
```
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
          
            if (!CheckInputs(tx, state, view, fScriptChecks, flags,
                             fCacheResults, fCacheResults,
                             PrecomputedTransactionData(tx), &vChecks)) {
                return error("ConnectBlock(): CheckInputs on %s failed with %s",
                             tx.GetId().ToString(), FormatStateMessage(state));
            }

            control.Add(vChecks);   
        }
        ...
    }
    
    ...
}
```
`ConnectBlock`将该区块链接到当前激活链上，并更新UTXO集合。
在该方法中：使用了全局对象`scriptcheckqueue`去构造了一个临时的管理对象，并通过该管理对象来操作全局任务队列：添加任务，执行任务；当该临时的管理对象析构时，会调用wait()方法，加入任务处理，处理完所有任务后，该对象析构完成。

## CScriptCheck(根据每个交易输入构造的检查任务)
```
class CScriptCheck {
private:
    CScript scriptPubKey;   
    Amount amount;      
    const CTransaction *ptxTo;
    unsigned int nIn;         
    uint32_t nFlags;          
    bool cacheStore;
    ScriptError error;       
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

    bool operator()();   
    
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
`CScript scriptPubKey;` 锁定脚本(即该验证交易的某个引用输出对应的锁定脚本)
`Amount amount;` 上述锁定脚本对应的金额(即花费的UTXO的金额)
`const CTransaction *ptxTo;`  正在花费的交易，即要检查的交易
`unsigned int nIn;`           要检查该交易的第几个输入；
`uint32_t nFlags;`            检查标识
`ScriptError error;`          验证出错的原因
`bool operator()();` 此处重载了()运算符，执行脚本检查操作；详情见下集：脚本验证
***
本文由 `Copernicus团队 姚永芯`写作，转载无需授权。
