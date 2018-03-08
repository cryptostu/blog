---
title: '比特币源码分析-boost::signal的使用'
date: 2018-03-08 10:13:19
tags:
---

bitcoin 代码中大量使用 `boost::signal`, boost::signal 实现了信号与槽的事件通知机制，或者说是一种消息的发布与订阅机制， `signal` 类型是一个可调用类型，`slot` 就是callback 对象，或者说事件的订阅者，signal 实例是一个可调用对象，调用signal 对象，就相当于发布了相应的事件,  signal  的`connect`， `disconnect` 方法分别相当于对事件的订阅，取消。

```c++
#include <boost/signals2.hpp>
#include <iostream>

void print_args(float x, float y)
{
	  std::cout << "The arguments are " << x << " and " << y << std::endl;
}

void print_sum(float x, float y)
{
	  std::cout << "The sum is " << x + y << std::endl;
}

void print_product(float x, float y)
{
	  std::cout << "The product is " << x * y << std::endl;
}

void print_difference(float x, float y)
{
	  std::cout << "The difference is " << x - y << std::endl;
}

void print_quotient(float x, float y)
{
	  std::cout << "The quotient is " << x / y << std::endl;
}

int main() {
	boost::signals2::signal<void(float, float)> sig;

	sig.connect(print_args);
	sig.connect(print_sum);
	sig.connect(print_product);
	sig.connect(print_difference);
	sig.connect(print_quotient);

	sig(5., 3.);
	return 0;

```
上面这个例子, 有五个函数订阅sig 事件，sig(5. , 3.) 的调用触发事件，参数5，3，相当于事件携带的消息paylaod, 传给了五个事件订阅者。

bitcoin 中定义了类型`CMainSignals` 来统一管理各个功能模块的事件通知，`CMainSignal` 是一个资源管理类型， 主要工作代理给由`unique_ptr` 管理内存的成员 `m_internals` , 它的类型是`MainSignalsInstance`，内部定义十个boost signal 变量， 分别表达十种要通知的事件。


```c++
class CMainSignals {
private:
    std::unique_ptr<MainSignalsInstance> m_internals;

    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
    friend void ::CallFunctionInValidationInterfaceQueue(std::function<void ()> func);

    void MempoolEntryRemoved(CTransactionRef tx, MemPoolRemovalReason reason);

public:
    /** Register a CScheduler to give callbacks which should run in the background (may only be called once) */
    void RegisterBackgroundSignalScheduler(CScheduler& scheduler);
    /** Unregister a CScheduler to give callbacks which should run in the background - these callbacks will now be dropped! */
    void UnregisterBackgroundSignalScheduler();
    /** Call any remaining callbacks on the calling thread */
    void FlushBackgroundCallbacks();

    size_t CallbacksPending();

    /** Register with mempool to call TransactionRemovedFromMempool callbacks */
    void RegisterWithMempoolSignals(CTxMemPool& pool);
    /** Unregister with mempool */
    void UnregisterWithMempoolSignals(CTxMemPool& pool);

    void UpdatedBlockTip(const CBlockIndex *, const CBlockIndex *, bool fInitialDownload);
    void TransactionAddedToMempool(const CTransactionRef &);
    void BlockConnected(const std::shared_ptr<const CBlock> &, const CBlockIndex *pindex, const std::shared_ptr<const std::vector<CTransactionRef>> &);
    void BlockDisconnected(const std::shared_ptr<const CBlock> &);
    void SetBestChain(const CBlockLocator &);
    void Inventory(const uint256 &);
    void Broadcast(int64_t nBestBlockTime, CConnman* connman);
    void BlockChecked(const CBlock&, const CValidationState&);
    void NewPoWValidBlock(const CBlockIndex *, const std::shared_ptr<const CBlock>&);
};

struct MainSignalsInstance {
    boost::signals2::signal<void (const CBlockIndex *, const CBlockIndex *, bool fInitialDownload)> UpdatedBlockTip;
    boost::signals2::signal<void (const CTransactionRef &)> TransactionAddedToMempool;
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &, const CBlockIndex *pindex, const std::vector<CTransactionRef>&)> BlockConnected;
    boost::signals2::signal<void (const std::shared_ptr<const CBlock> &)> BlockDisconnected;
    boost::signals2::signal<void (const CTransactionRef &)> TransactionRemovedFromMempool;
    boost::signals2::signal<void (const CBlockLocator &)> SetBestChain;
    boost::signals2::signal<void (const uint256 &)> Inventory;
    boost::signals2::signal<void (int64_t nBestBlockTime, CConnman* connman)> Broadcast;
    boost::signals2::signal<void (const CBlock&, const CValidationState&)> BlockChecked;
    boost::signals2::signal<void (const CBlockIndex *, const std::shared_ptr<const CBlock>&)> NewPoWValidBlock;

    // We are not allowed to assume the scheduler only runs in one thread,
    // but must ensure all callbacks happen in-order, so we end up creating
    // our own queue here :(
    SingleThreadedSchedulerClient m_schedulerClient;

    explicit MainSignalsInstance(CScheduler *pscheduler) : m_schedulerClient(pscheduler) {}
};

```

类型`CValidationInterface` 主要是统一表示某些对`MainSignalsInstance` 中定义的十个事件感兴趣的对象， 即这些事件的订阅者， 所有对这些事件感兴趣代码继承`CValidationInterface`类型，
提供自己版本的这些虚成员函数的实现，覆盖`baseCValidationInterface` 中对应的空方法， 表达对相应的事件感兴趣， 不感兴趣的事件的回调方法继续是那些继承自base class 的空方法。
```c++
class CValidationInterface {
protected:
    
    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) {}
    
    virtual void TransactionAddedToMempool(const CTransactionRef &ptxn) {}
   
    virtual void TransactionRemovedFromMempool(const CTransactionRef &ptx) {}
    
    virtual void BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted) {}
    
    virtual void BlockDisconnected(const std::shared_ptr<const CBlock> &block) {}
    
    virtual void SetBestChain(const CBlockLocator &locator) {}
   
    virtual void Inventory(const uint256 &hash) {}

    virtual void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman) {}
    
    virtual void BlockChecked(const CBlock&, const CValidationState&) {}
    
    virtual void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& block) {};
    friend void ::RegisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterValidationInterface(CValidationInterface*);
    friend void ::UnregisterAllValidationInterfaces();
};

```
CValidationInterface 有四个子类， CWallet , CZMQNotificationInterface,   submitblock_StateCatcher,  PeerLogicValidation 分别对应四个对`MainSignalsInstance` 中的事件感兴趣的订阅者。

```c++
class CWallet final : public CCryptoKeyStore, public CValidationInterface
{
       ...............
           void TransactionAddedToMempool(const CTransactionRef& tx) override;
           void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) override;
           void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
           void TransactionRemovedFromMempool(const CTransactionRef &ptx) override;
           void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman) override;
           void SetBestChain(const CBlockLocator& loc) override;
           void Inventory(const uint256 &hash) override
          {
              {
                  LOCK(cs_wallet);
                 std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
                 if (mi != mapRequestCount.end())
                             (*mi).second++;
              }
          }
       ...............
};

class CZMQNotificationInterface final : public CValidationInterface
{
     ................
    // CValidationInterface
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    .................
};
class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    explicit submitblock_StateCatcher(const uint256 &hashIn) : hash(hashIn), found(false), state() {}

protected:
    void BlockChecked(const CBlock& block, const CValidationState& stateIn) override {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    }
};

class PeerLogicValidation : public CValidationInterface, public NetEventsInterface 
{
private:
    CConnman* const connman;

public:
    explicit PeerLogicValidation(CConnman* connman, CScheduler &scheduler);

    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    void BlockChecked(const CBlock& block, const CValidationState& state) override;
    void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& pblock) override;
    ........
};

```

这四个订阅者通过调用`RegisterValidationInterface`， `UnregisterValidationInterface` 订阅，取消事件通知，函数接受参数是指向订阅者的指针。

```c++
void RegisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.m_internals->UpdatedBlockTip.connect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.m_internals->TransactionAddedToMempool.connect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.m_internals->BlockConnected.connect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.m_internals->BlockDisconnected.connect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1));
    g_signals.m_internals->TransactionRemovedFromMempool.connect(boost::bind(&CValidationInterface::TransactionRemovedFromMempool, pwalletIn, _1));
    g_signals.m_internals->SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.m_internals->Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.m_internals->Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.m_internals->BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.m_internals->NewPoWValidBlock.connect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn) {
    g_signals.m_internals->BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.m_internals->Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn, _1, _2));
    g_signals.m_internals->Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.m_internals->SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.m_internals->TransactionAddedToMempool.disconnect(boost::bind(&CValidationInterface::TransactionAddedToMempool, pwalletIn, _1));
    g_signals.m_internals->BlockConnected.disconnect(boost::bind(&CValidationInterface::BlockConnected, pwalletIn, _1, _2, _3));
    g_signals.m_internals->BlockDisconnected.disconnect(boost::bind(&CValidationInterface::BlockDisconnected, pwalletIn, _1));
    g_signals.m_internals->TransactionRemovedFromMempool.disconnect(boost::bind(&CValidationInterface::TransactionRemovedFromMempool, pwalletIn, _1));
    g_signals.m_internals->UpdatedBlockTip.disconnect(boost::bind(&CValidationInterface::UpdatedBlockTip, pwalletIn, _1, _2, _3));
    g_signals.m_internals->NewPoWValidBlock.disconnect(boost::bind(&CValidationInterface::NewPoWValidBlock, pwalletIn, _1, _2));
}
```

在程序启动，初始化阶段，启动调度器线程
```c++
bool AppInitMain()
{
      .................
     CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue, &scheduler);
     threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));
     .................
}
```
调用RegisterBackgroundSignalScheduler(), 初始化全局MainSignalsInstance 实例; 调用RegisterWithMempoolSignals(), 订阅全局内存池对象的NotifyEntryRemoved 事件通知,  MainSignalsInstance 只对由于超时，大小限制， blockchian 重组，替换等原因发生的离开内存池事件感兴趣， 收到后MainSignalsInstance 再作为事件发布者， 转发给其他订阅者， 如CWallet。

```c++
bool AppInitMain()
{
     ......
    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);
    GetMainSignals().RegisterWithMempoolSignals(mempool);
    ........
 }
```
初始化全局的连接管理对象connman 后， 初始化全局的peerLogic 对象， 然后调用RegisterValidationInterface(), peerLogic 成为MainSignalsInstance 对象的订阅者,  如果用户编译了`zeromq`支持模块，调用RegisterValidationInterface(),  pzmqNotificationInterface 订阅MainSignalsInstance 。
```c++
bool AppInitMain()
{
    .............
    assert(!g_connman);
    g_connman = std::unique_ptr<CConnman>(new CConnman(GetRand(std::numeric_limits<uint64_t>::max()), GetRand(std::numeric_limits<uint64_t>::max())));
    CConnman& connman = *g_connman;

    peerLogic.reset(new PeerLogicValidation(&connman, scheduler));
    RegisterValidationInterface(peerLogic.get());
    .............
    
#if ENABLE_ZMQ
    pzmqNotificationInterface = CZMQNotificationInterface::Create();

    if (pzmqNotificationInterface) {
        RegisterValidationInterface(pzmqNotificationInterface);
    }
#endif
    ...............
}
```
如果钱包功能开启， 启动后打开钱包过程，会把钱包注册成为MainSignalsInstance的订阅者。
```c++
bool AppInitMain()
{
      ........
#ifdef ENABLE_WALLET
    if (!OpenWallets())
        return false;
#else
    LogPrintf("No wallet support compiled in!\n");
#endif

      ........
}

bool OpenWallets()
{
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
        LogPrintf("Wallet disabled!\n");
        return true;
    }

    for (const std::string& walletFile : gArgs.GetArgs("-wallet")) {
        CWallet * const pwallet = CWallet::CreateWalletFromFile(walletFile);
        if (!pwallet) {
            return false;
        }
        vpwallets.push_back(pwallet);
    }

    return true;
}

CWallet* CWallet::CreateWalletFromFile(const std::string walletFile)
{
        ......
            CWallet *walletInstance = new CWallet(std::move(dbw));
            RegisterValidationInterface(walletInstance);
        ......
}    
```
在`submitblock rpc `调用中， 用户提交hex编码的原始block， 解析后， 调用ProcessNewBlock()检查处理，使用类型`submitblock_StateCatcher` 的对象sc 作为MainSignalsInstance 的订阅者，
对提交过去的block 的验证结果，作为事件通知返回给rpc 调用。
```c++
UniValue submitblock(const JSONRPCRequest& request)
{
     ...........
     
    submitblock_StateCatcher sc(block.GetHash());
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(Params(), blockptr, true, nullptr);
    UnregisterValidationInterface(&sc);

     ...........
}
```

`CMainSignals` 类型上面定义了一堆触发事件的方法, 别的代码模块调用这些方法， 触发相应的事件，把事件通知发给相关的订阅者。
```c++
void CMainSignals::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) {
    m_internals->m_schedulerClient.AddToProcessQueue([pindexNew, pindexFork, fInitialDownload, this] {
        m_internals->UpdatedBlockTip(pindexNew, pindexFork, fInitialDownload);
    });
}

void CMainSignals::TransactionAddedToMempool(const CTransactionRef &ptx) {
    m_internals->m_schedulerClient.AddToProcessQueue([ptx, this] {
        m_internals->TransactionAddedToMempool(ptx);
    });
}

void CMainSignals::BlockConnected(const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex, const std::shared_ptr<const std::vector<CTransactionRef>>& pvtxConflicted) {
    m_internals->m_schedulerClient.AddToProcessQueue([pblock, pindex, pvtxConflicted, this] {
        m_internals->BlockConnected(pblock, pindex, *pvtxConflicted);
    });
}

void CMainSignals::BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) {
    m_internals->m_schedulerClient.AddToProcessQueue([pblock, this] {
        m_internals->BlockDisconnected(pblock);
    });
}

void CMainSignals::SetBestChain(const CBlockLocator &locator) {
    m_internals->m_schedulerClient.AddToProcessQueue([locator, this] {
        m_internals->SetBestChain(locator);
    });
}

void CMainSignals::Inventory(const uint256 &hash) {
    m_internals->m_schedulerClient.AddToProcessQueue([hash, this] {
        m_internals->Inventory(hash);
    });
}

void CMainSignals::Broadcast(int64_t nBestBlockTime, CConnman* connman) {
    m_internals->Broadcast(nBestBlockTime, connman);
}

void CMainSignals::BlockChecked(const CBlock& block, const CValidationState& state) {
    m_internals->BlockChecked(block, state);
}

void CMainSignals::NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &block) {
    m_internals->NewPoWValidBlock(pindex, block);
}
```
CChainState 的`ActivateBestChain`方法里， 发布BlockConnected， UpdatedBlockTip 事件:

```c++
bool CChainState::ActivateBestChain(CValidationState &state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock) {
           ..............................
           for (const PerBlockConnectTrace& trace : connectTrace.GetBlocksConnected()) {
                assert(trace.pblock && trace.pindex);
                GetMainSignals().BlockConnected(trace.pblock, trace.pindex, trace.conflictedTxs);
            }

           ..............................
           GetMainSignals().UpdatedBlockTip(pindexNewTip, pindexFork, fInitialDownload);
           ...............................
}
```


CChainState 的AcceptBlock 方法里，发布NewPoWValidBlock 事件:

```c++
bool CChainState::AcceptBlock(const std::shared_ptr<const CBlock>& pblock, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const CDiskBlockPos* dbp, bool* fNewBlock)
{
        .......................
        
        if (!IsInitialBlockDownload() && chainActive.Tip() == pindex->pprev)
               GetMainSignals().NewPoWValidBlock(pindex, pblock);

        ......................
}
```
CChainState 的DisconnectTip 方法里，发布 `BlockDisconnected` 事件:
```c++
bool CChainState::DisconnectTip(CValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions *disconnectpool)
{
   ...................
     GetMainSignals().BlockDisconnected(pblock);
     ..............
}
```
CChainState 的ConnectTip 方法里，发布 `BlockChecked` 事件:
```c++
bool CChainState::ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions &disconnectpool)
{
     ................
     
        CCoinsViewCache view(pcoinsTip.get());
        bool rv = ConnectBlock(blockConnecting, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(blockConnecting, state);

     ................
}
```
PeerLogicValidation 的 `SendMessage` 方法里， 发布Broadcast事件, 通知钱包重新发送未确认的交易:
```c++
bool PeerLogicValidation::SendMessages(CNode* pto, std::atomic<bool>& interruptMsgProc)
{
    .............
    if (!fReindex && !fImporting && !IsInitialBlockDownload())
    {
            GetMainSignals().Broadcast(nTimeBestReceived, connman);
    }
    .............
}
```
从网络上收到`INV`消息后，通知给钱包，更新内部状态。
```c++
bool static ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, int64_t nTimeReceived, const CChainParams& chainparams, CConnman* connman, const std::atomic<bool>& interruptMsgProc)
{
            .................
            for (CInv &inv : vInv)
           {
               ............
               GetMainSignals().Inventory(inv.hash);

           }
            ................
}
```
`validation.cpp` 里面的 `ProcessNewBlock`在内部调用 CheckBlock 后，检查失败后，发布 BlockCheck s事件， 通告给相关订阅者。
```c++
bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool *fNewBlock)
{
      ............
      bool ret = CheckBlock(*pblock, state, chainparams.GetConsensus());
      if (!ret) {
            GetMainSignals().BlockChecked(*pblock, state);
            return error("%s: AcceptBlock FAILED (%s)", __func__, state.GetDebugMessage());
     }
      ...........

}
```
`FlushStateToDisk`, 发布`SetBestChain` 事件， 通知钱包
```c++
bool static FlushStateToDisk(const CChainParams& chainparams, CValidationState &state, FlushStateMode mode, int nManualPruneHeight) {
     ...............
     if (fDoFullFlush || ((mode == FLUSH_STATE_ALWAYS || mode == FLUSH_STATE_PERIODIC) && nNow > nLastSetChain + (int64_t)DATABASE_WRITE_INTERVAL * 1000000)) {
        // Update best block in wallet (so we can detect restored wallets).
        GetMainSignals().SetBestChain(chainActive.GetLocator());
        nLastSetChain = nNow;
    }
     ...............
}
```
`validation.cpp` 里面的`AcceptToMemoryPoolWorker`, 在结束前， 发布`TransactionAddedToMempool` 事件。
```c++
static bool AcceptToMemoryPoolWorker(const CChainParams& chainparams, CTxMemPool& pool, CValidationState& state, const CTransactionRef& ptx,
                              bool* pfMissingInputs, int64_t nAcceptTime, std::list<CTransactionRef>* plTxnReplaced,
                              bool bypass_limits, const CAmount& nAbsurdFee, std::vector<COutPoint>& coins_to_uncache)
{
         ................
         GetMainSignals().TransactionAddedToMempool(ptx);

}
```


***
本文由 `Copernicus团队` 喻建写作，转载无需授权
