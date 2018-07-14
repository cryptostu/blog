---
title: 'Bitcoin源代码分析:VersionBits模块解析'
date: 2018-02-21 12:25:24
tags:
---
BIP9允许部署多个向后兼容的软分叉，通过旷工在一个目标周期内投票，如果达到激活阈值`nRuleChangeActivationThreshold`,就能成功的启用该升级。在实现方面，通过重定义区块头信息中的version字段，将version字段解释为bit vector，每一个bit可以用来跟踪一个独立的部署，在满足激活条件之后，该部署将会生效，同时该bit可以被其他部署使用。目前通过BIP9成功进行软分叉有`BIP68, 112, 113`, 于2016-07-04 ，高度：419328成功激活.

## BIP9部署设置

每一个进行部署的BIP9，都必须设置bit位、开始时间、过期时间。

```
struct BIP9Deployment {
    int bit;					
    int64_t nStartTime;		
    int64_t nTimeout;
};


// namespace:Consensus
struct Params {
    ...
    uint32_t nRuleChangeActivationThreshold;        
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS]; // BIP9
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    ...
  };

```

bit通过`1 << bit`方式转换成一个uint32_t的整数，在检验一个BIP9部署是否成功激活的时候使用了Condition(...)函数，来验证一个区块是否赞成该部署。

```
bool Condition(const CBlockIndex *pindex, const Consensus::Params &params) const {
    return ((
            (pindex->nVersion & VERSIONBITS_TOP_MASK) ==VERSIONBITS_TOP_BITS) && 
            (pindex->nVersion & Mask(params)) != 0);
}

uint32_t Mask(const Consensus::Params &params) const {
    return ((uint32_t)1) << params.vDeployments[id].bit;
}
```
逻辑分析

- 首先验证该version是有效的version设置(001)
- 验证块的版本号中是否设置了指定的bit位
	* Mask()函数通过将1左移BIP9部署中设定的bit，生成一个该区块代表的version

开始时间和过期时间主要为了在检查BIP9部署状态时，提供状态判断的依据和临界值。比如如果区块的中位数时间超过了过期时间`nTimeTimeout`，则判断该BIP9部署已经失败(后面会详细拆解)。

```
if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
    stateNext = THRESHOLD_FAILED;
} else if (pindexPrev->GetMedianTimePast() >= nTimeStart) {
    stateNext = THRESHOLD_STARTED;
}

if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
    stateNext = THRESHOLD_FAILED;
    break;
}
```

## 部署状态转换

BIP9部署中定义了所有软分叉升级的初始状态均为`THRESHOLD_DEFINED`,并定义创始区块状态为`THRESHOLD_DEFINED`, 另外如果在程序中遇到blockIndex为`nullptr`时，均返回`THRESHOLD_DEFINED`状态。

具体转换过程如下：`THRESHOLD_DEFINED`为软分叉的初始状态，如果过去中位数时间(MTP)大于nStartTIme，则状态转换为`THRESHOLD_STARTED`，如果MTP大于等于nTimeout，则状态转换成`THRESHOLD_FAILED`；如果在一个目标周期(2016个区块)内赞成升级的区块数量占95%以上(大约1915个区块)，则状态转换成`THRESHOLD_LOCKED_IN`，否则转换成`THRESHOLD_FAILED`；在`THRESHOLD_LOCKED_IN`之后的下一个目标周期，状态转换成`THRESHOLD_ACTIVE`，同时该部署将保持该状态。

```
enum ThresholdState {
    THRESHOLD_DEFINED,			
    THRESHOLD_STARTED,
    THRESHOLD_LOCKED_IN,
    THRESHOLD_ACTIVE,
    THRESHOLD_FAILED,
};
```
![states](http://upload-images.jianshu.io/upload_images/4694144-9b8a21d3b8299e0a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

## 业务逻辑

基类`AbstractThresholdConditionChecker`定义了通过共识规则检查BIP9部署的状态。有如下方法，其中最后两个方法在基类中实现，子类继承了该方法的实现:

- Condition(...)检测一个区块是否赞成一个软分叉升级：首先验证该区块version是否有效的version格式, 然后检测该version是否设置了相应个bit位
- BeginTime(...)返回共识规则中的开始投票时间(采用MTP验证 pindexPrev->GetMedianTimePast() >= nTimeStart)
- EndTime(...)返回共识规则中的设置的过期时间
- Period(...)返回共识规则中的一个目标周期(当前主链的目标周期为2016个区块)
- Threshold(...)返回nRuleChangeActivationThreshold，表示满足软分叉升级的最低要求
- GetStateFor(...)在提供共识规则、开始检索的区块索引、以及之前缓存的状态数据判断当前部署的状态(后面会详细分析其逻辑)
- GetStateSinceHeightFor(...)函数的作用是查找从哪个区块高度开始，该部署的状态就已经和当前一致

```
class AbstractThresholdConditionChecker {
protected:
    virtual bool Condition(const CBlockIndex *pindex, const Consensus::Params &params) const = 0;
    virtual int64_t BeginTime(const Consensus::Params &params) const = 0;
    virtual int64_t EndTime(const Consensus::Params &params) const = 0;
    virtual int Period(const Consensus::Params &params) const = 0;
    virtual int Threshold(const Consensus::Params &params) const = 0;
    

public:
    ThresholdState GetStateFor(const CBlockIndex *pindexPrev, const Consensus::Params &params, ThresholdConditionCache &cache) const;
    int GetStateSinceHeightFor(const CBlockIndex *pindexPrev, const Consensus::Params &params, ThresholdConditionCache &cache) const;
};
```

类`VersionBitsConditionChecker`继承了`AbstractThresholdConditionChecker`。实现了:

- BeginTime(const Consensus::Params &params)
- EndTime(const Consensus::Params &params)
- Period(const Consensus::Params &params)
- Threshold(const Consensus::Params &params)
- Condition(const CBlockIndex *pindex, const Consensus::Params &params)

```
class VersionBitsConditionChecker : public AbstractThresholdConditionChecker {
private:
	// maybe: DEPLOYMENT_TESTDUMMY,DEPLOYMENT_CSV,MAX_VERSION_BITS_DEPLOYMENTS
    const Consensus::DeploymentPos id;

protected:
    int64_t BeginTime(const Consensus::Params &params) const {
        return params.vDeployments[id].nStartTime;
    }
    int64_t EndTime(const Consensus::Params &params) const {
        return params.vDeployments[id].nTimeout;
    }
    int Period(const Consensus::Params &params) const {
        return params.nMinerConfirmationWindow;
    }
    int Threshold(const Consensus::Params &params) const {
        return params.nRuleChangeActivationThreshold;
    }

    bool Condition(const CBlockIndex *pindex, const Consensus::Params &params) const {
        return ((
                (pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) && (pindex->nVersion & Mask(params)) != 0);
    }
    
    ...
}
```

另个一重要的类`VersionBitsCache`,包括一个方法和一个数组。该数组作为内存缓存使用，该数组的成员是一个map，当检查一个BIP9部署的状态时，如果在检查过程中判断出部署状态，该map会以区块索引为键值，以状态信息(int)为值,缓存起来，在下次检查时可以在该区块位置直接得到其状态信息，对程序起到了优化的作用，避免重复的检索。

```
struct VersionBitsCache {
    ThresholdConditionCache caches[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

    void Clear();
};

typedef std::map<const CBlockIndex *, ThresholdState> ThresholdConditionCache;
```

另外`WarningBitsConditionChecker`类也继承了`AbstractThresholdConditionChecker`类，实现了对未知升级的追踪与警告。一旦nVersion中有未预料到的位被设置成1，mask将会生成非零的值。当未知升级被检测到处`THRESHOLD_LOCKED_IN`状态，软件应该警告用户即将到来未知的软分叉。在下一个目标周期，处于`THRESHOLD_ACTIVE`状态是，更应该强调警告用户。

> 需要说明的是：未知升级只有处于LOCKED_IN或ACTIVE的条件下才会发出警告

```
...
WarningBitsConditionChecker checker(bit);
ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
if (state == THRESHOLD_ACTIVE || state == THRESHOLD_LOCKED_IN) {
    if (state == THRESHOLD_ACTIVE) {
        std::string strWarning =
            strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
        SetMiscWarning(strWarning);
        if (!fWarned) {
            AlertNotify(strWarning);
            fWarned = true;
        }
    } else {
        warningMessages.push_back(
            strprintf("unknown new rules are about to activate (versionbit %i)", bit));
    }
}
...
```

## 代码拆解

1. GetAncestor(int height)函数在整个模块中的使用率非常高，其作用就是为了返回指定高度的区块索引，作用非常简单但是其代码逻辑不太好理解。可以把整个区块链简单的看成就是一个链表结构，为了获得指定高度的节点信息，一般通过依次移动指针到指定区块即可。在该模块中，使用CBlockIndex类中的pskip字段，配合`GetSkipHeight(int height)`函数，能够快速定位到指定高度的区块，优化了执行的效率。

	```
	CBlockIndex *CBlockIndex::GetAncestor(int height) {
	    if (height > nHeight || height < 0) {
	        return nullptr;
	    }
	
	    CBlockIndex *pindexWalk = this;
	    int heightWalk = nHeight;
	    while (heightWalk > height) {
	        int heightSkip = GetSkipHeight(heightWalk);
	        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
	        if (pindexWalk->pskip != nullptr &&
	            (heightSkip == height || (heightSkip > height && !(heightSkipPrev < heightSkip - 2 && heightSkipPrev >= height)))) {
	
	            pindexWalk = pindexWalk->pskip;
	            heightWalk = heightSkip;
	        } else {
	            assert(pindexWalk->pprev);
	            pindexWalk = pindexWalk->pprev;
	            heightWalk--;
	        }
	    }
	    return pindexWalk;
	}
	
	static inline int GetSkipHeight(int height) {
	    if (height < 2) {
	        return 0;
	    }
	    
	    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
	}
	```

2. 在整个模块中进行时间比较判断是都使用了GetMedianTimePast(), 其作用就是找出当前区块前的10个区块，排序后，返回第5个元素的nTime

	```
	enum { nMedianTimeSpan = 11 };
	
	int64_t GetMedianTimePast() const {
	    int64_t pmedian[nMedianTimeSpan];
	    
	    int64_t *pbegin = &pmedian[nMedianTimeSpan];
	    int64_t *pend = &pmedian[nMedianTimeSpan];
	
	    const CBlockIndex *pindex = this;

	    for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev) {
	        *(--pbegin) = pindex->GetBlockTime();
	    }
	
	    std::sort(pbegin, pend);
	    
	    return pbegin[(pend - pbegin) / 2];
	} 
	```
	
	逻辑如下:
	
	- 创建包含11个元素的数组，包括该区块和之前的10个区块
	- pbegin、pend两个游标(数组游标)指向数组末端
	- 遍历11个区块，pindex游标不断地向前移动
	- 数组游标向前移动，并将pindex获取的时间戳赋值给数组
	- 对数组排序(排序的原因是:区块时间戳是不可靠的字段，其大小与创建区块顺序可能不一致)
	- 11个区块去中间的元素，也就是数组下标为5的元素，因为是奇数个元素，所以不用进行判断下标无效的问题

3. GetStateFor(...)函数在整个模块中至关重要，负责获取BIP9部署的状态信息。首先说明的是在一个目标周期之内，一个BIP9部署的状态是相同的，也就是说部署状态只会在难度目标发生改变之后才会更新。GetStateFor(...)函数获取的是上一个目标周期的最后一个区块的状态，如果该状态可以判断出部署状态则得出结果，并将结果保存在`VersionBitsCache`结构体中；如果该状态已经存在于缓存中则直接返回结果；最后如果该区块无法得出状态信息，则会依次寻找(pindexPrev.nHeight - nPeriod)高度的状态信息，直到能够得出结果。如果直到nullptr也没有，则返回`THRESHOLD_DEFINED`。其中比较重要的是，如果一个区块表明该部署状态处于`THRESHOLD_STARTED`，则会进行更为详细的判断，以证明其状态是否以及失败或者可以进入LOCKED_IN阶段。

	```
	ThresholdState AbstractThresholdConditionChecker::GetStateFor(...){
		...
		if (pindexPrev != nullptr) {
	        pindexPrev = pindexPrev->GetAncestor(
	            pindexPrev->nHeight - ((pindexPrev->nHeight + 1) % nPeriod));
	    }
	    
	    std::vector<const CBlockIndex *> vToCompute;
	    
	    while (cache.count(pindexPrev) == 0) { 
	        if (pindexPrev == nullptr) {
	            cache[pindexPrev] = THRESHOLD_DEFINED;
	            break;
	        }
	        if (pindexPrev->GetMedianTimePast() < nTimeStart) {
	            cache[pindexPrev] = THRESHOLD_DEFINED;
	            break;
	        }
	
	        vToCompute.push_back(pindexPrev);
	        pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);      
		}
	    assert(cache.count(pindexPrev));
	    ThresholdState state = cache[pindexPrev];

	    while (!vToCompute.empty()) {
	        ThresholdState stateNext = state;          
	        pindexPrev = vToCompute.back();            
	        vToCompute.pop_back();                     
	
	        switch (state) {
	            case THRESHOLD_DEFINED: {
	                if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
	                    stateNext = THRESHOLD_FAILED;
	                } else if (pindexPrev->GetMedianTimePast() >= nTimeStart) {
	                    stateNext = THRESHOLD_STARTED;
	                }
	                break;
	            }
	            case THRESHOLD_STARTED: {	
	                if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
	                    stateNext = THRESHOLD_FAILED;
	                    break;
	                }
	                const CBlockIndex *pindexCount = pindexPrev;
	                int count = 0;
	                for (int i = 0; i < nPeriod; i++) {
	                    if (Condition(pindexCount, params)) { 
	                        count++;
	                    }
	                    pindexCount = pindexCount->pprev;
	                }
	                if (count >= nThreshold) {      
	                		stateNext = THRESHOLD_LOCKED_IN;
	                }
	                break;
	            }
	            case THRESHOLD_LOCKED_IN: {
	                stateNext = THRESHOLD_ACTIVE;
	                break;
	            }
	            case THRESHOLD_FAILED:
	            case THRESHOLD_ACTIVE: {
	                break;
	            }
	        }
	        cache[pindexPrev] = state = stateNext;
	    }
	}
	```

	举例说明:
	
	- 从0 -> 2015 -> 4031 -> 6047;
	    * 状态转换：`THRESHOLD_DEFINED` -> `THRESHOLD_STARTED` -> `THRESHOLD_LOCKED_IN` -> `THRESHOLD_ACTIVE`
	
	- bitcoin 中的版本检测按照 `nMinerConfirmationWindow` 为一轮进行检测，在本轮之间的所有区块，都与本轮的第一个块状态相同。
	    * 即 2015 -> 4030 之间所有块的状态，都与索引为2015 的块的部署状态相同。
	- 示例：
	    * 针对某个 bit 位的部署，height( 0 -> 2014 )区块的所有状态都为THRESHOLD_DEFINED； 
	    * 当父区块的高度为 2015 时(即当每次获取本轮第二个区块时，才会对本轮的第一个块的状态进行赋值，然后本轮所有块的时间都与本轮第一个块的状态相同)，因为它不在全局缓存中，则进入条件，且它的MTP时间 >= startTime, 将该块的索引加入临时集合中，并将指针向前推至上一轮的初始块(此时这个块在集合中),进入接下来的条件执行。
	        * 遍历临时集合，因为上一轮的撞态为`THRESHOLD_DEFINED`，且本轮初始块的时间 >= startTime,将本轮的状态转换为`THRESHOLD_STARTED`；
	    * 当父区块的高度为 4031(即当前的块为4032时)，它不在全局缓存中，进入条件，且它的MTP时间 >= startTime,将该块的索引加入临时集合中，并将指针向前推至上一轮的初始块(此时这个块在集合中),进入接下来的条件执行。
	        * 遍历临时集合，因为上一轮的状态为`THRESHOLD_STARTED`，且本轮初始块的时间 < timeout, 将统计上一轮部署该bit位的区块个数(即从 2016 ->4031),假设部署的个数超过阈值(95%),将本轮的状态转换为`LOCKED_IN`；
	    * 当父区块的高度为 6047(即当前的块为6048时)，它不在全局状态中，进入条件，且它的MTP时间 >= startTime,将该块的索引加入临时集合中，并将指针向前推至上一轮的初始块(此时这个块在集合中),进入接下来的条件执行。
	        * 遍历临时集合，因为上一轮的状态为`THRESHOLD_LOCKED_IN`，将本轮的状态自动切换为`THRESHOLD_ACTIVE`。

4. GetStateSinceHeightFor()函数获取本轮状态开始时的区块所在高度； 开始这个状态轮次的第二个区块的高度(因为每轮块的状态更新，都是当计算每轮第二个块时，才会去计算，然后把计算的结果缓存在全局缓存中；因为所有块的状态都是根据它的父区块确定的);

	```
	int AbstractThresholdConditionChecker::GetStateSinceHeightFor(
	    const CBlockIndex *pindexPrev, const Consensus::Params &params,
	    ThresholdConditionCache &cache) const {
	    const ThresholdState initialState = GetStateFor(pindexPrev, params, cache);
	
	    // BIP 9 about state DEFINED: "The genesis block is by definition in this
	    if (initialState == THRESHOLD_DEFINED) {
	        return 0;
	    }
	
	    const int nPeriod = Period(params);
	    

	    pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight - ((pindexPrev->nHeight + 1) % nPeriod));     
	    const CBlockIndex *previousPeriodParent = pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);    
	
	    while (previousPeriodParent != nullptr &&
	           GetStateFor(previousPeriodParent, params, cache) == initialState) {
	        pindexPrev = previousPeriodParent;
	        previousPeriodParent =
	            pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);
	    }
	    
	    // Adjust the result because right now we point to the parent block.
	    return pindexPrev->nHeight + 1;
	}
	
	```    
	
	逻辑如下:
	
	- 获取本轮的块的状态, 如果为`THRESHOLD_DEFINED`直接返回0
	- 获取本目标周期的初始块和上一目标周期的初始块
	- 当上一轮的初始块不为NULL，并且状态与本轮状态相同时，进入循环逻辑
		* 如果其状态与当前状态相同则向上一个目标周期寻找
		* 当状态某个轮次的状态与本轮的状态不同时，退出上述循环，然后返回这种状态开始时的高度
	
***
本文由`Copernicus团队` 戚帅、姚永芯写作，转载无需授权

