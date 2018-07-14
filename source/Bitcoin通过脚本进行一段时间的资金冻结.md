---
title: Bitcoin通过脚本进行一段时间的资金冻结
date: 2018-03-15 11:33:38
tags:
---

## 该脚本的格式
* 锁定脚本: < expiry time > OP_CHECKLOCKTIMEVERIFY OP_DROP OP_DUP OP_HASH160 < pubKeyHash > OP_EQUALVERIFY OP_CHECKSIG
* 解锁脚本: < sig > < pubKey >
* 允许一个交易的输出在未来某个时间之后才可以进行花费。即可以将资金锁定在未来的某个时间之后才可以使用。

## 程序的执行

### 操作码的执行
```
bool EvalScript(...){
    ...
    
    case OP_CHECKLOCKTIMEVERIFY: {
        if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {
            // not enabled; treat as a NOP2  
            if (flags &
                SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                return set_error(
                    serror,
                    SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
            }
            break;
        }
        if (stack.size() < 1) {
            return set_error(serror, SCRIPT_ERR_INVALID_STACK_OPERATION);
        }
        
        const CScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);
        if (nLockTime < 0) {
            return set_error(serror, SCRIPT_ERR_NEGATIVE_LOCKTIME);
        }
        if (!checker.CheckLockTime(nLockTime)) {
            return set_error(serror, SCRIPT_ERR_UNSATISFIED_LOCKTIME);
        }
        break;
    }
    ...
}
```

上述为脚本中包含OP_CHECKLOCKTIMEVERIFY 操作码时，执行的验证过程:
* 查看时间检测功能是否启用；如果未启用，接着查看客户端是否允许启用OP_NOPn 操作码，禁止的情况下，直接报错；否则执行OP_NOPn 原始操作，即无操作，跳出。
* 此时执行时间的检查：此时栈上的数据量，因为此时栈中应至少含有脚本的锁定时间。
* 将栈顶的时间数据转换为可比较类型；
* 进行锁定时间的检测
    
### 时间的检测
```
bool TransactionSignatureChecker::CheckLockTime(const CScriptNum &nLockTime) const {
    if (!((txTo->nLockTime < LOCKTIME_THRESHOLD && nLockTime < LOCKTIME_THRESHOLD) ||
          (txTo->nLockTime >= LOCKTIME_THRESHOLD && nLockTime >= LOCKTIME_THRESHOLD))) {
        return false;
    }
    if (nLockTime > int64_t(txTo->nLockTime)) {
        return false;
    }
    if (CTxIn::SEQUENCE_FINAL == txTo->vin[nIn].nSequence) {
        return false;
    }

    return true;
}
```

上述为拿到脚本的锁定时间后进行的检测:
* 时间锁定分为两种：一种是基于区块高度的锁定，一种是基于时间的锁定；二者通过与 LOCKTIME_THRESHOLD=500000000 进行比较来区分；
* 当小于LOCKTIME_THRESHOLD时，即为高度；否则为时间。
* 此时交易的时间戳应该与脚本的锁定时间处于同一 区间(高度或时间)；否则无法进行比较，直接返回错误。
* 只有当交易的时间大于等于脚本时间时，该笔资金才会解冻；否则直接返回错误。
* 当该笔交易输入的 nSequence 字段 = SEQUENCE_FINAL该值时，相当于绕过了脚本的时间锁定，不允许这样做。
    
## 操作码的描述

OP_CHECKLOCKTIMEVERIFY : 如果栈顶项大于交易的时间戳字段，标识该交易无效，否则脚本继续执行。在以下几种情况时，同样标识脚本无效：
* 栈为空；
* 栈顶项为负数；
* 栈顶项大于等于LOCKTIME_THRESHOLD(500000000)；而交易的时间戳小于LOCKTIME_THRESHOLD； 反之也无效。
* 交易输入的nSequence等于SEQUENCE_FINAL(0xffffffff).
    
OP_DROP : 移除栈顶项；

## 脚本执行流程图
![脚本执行.png](https://upload-images.jianshu.io/upload_images/5181674-89508e0f08cbcf26.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

***
本文由 `Copernicus团队 姚永芯`写作，转载无需授权。

