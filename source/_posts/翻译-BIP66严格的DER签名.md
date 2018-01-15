---
title: '[翻译]BIP66严格的DER签名'
date: 2018-01-15 11:35:30
tags:
---
## 综述：
该提案定义了比特币交易有规则的变化，用来限制签名字段必须为严格的DER编码。
## 动机：
当前比特币的签名验证实现依赖于OpenSSL，这意味着OpenSSL隐式的定义了比特币的区块验证规则。不幸的是，openssl并没有定义严格的共识行为(它不保证不同版本间的bug兼容);并且openssl库的改变将会影响比特币软件的稳定。
一个特别重要的地方是：签名编码。直到最近，openssl库的发布版才可以接收不同的DER标准编码，并且认为签名时有效的。当openssl 从1.0.0p和1.0.1k升级时，它使一些节点产生拒绝承认主链的行为。
本提案的目的是：将有效签名限制在DER规定的范围内，从而使共识规则不依赖于openssl的签名解析。如果想从共识代码中移除所有的openssl，则需要这样的修改。
## 规范
每个传递到OP_CHECKSIG, OP_CHECKSIGVERIFY, OP_CHECKMULTISIG, or OP_CHECKMULTISIGVERIFY操作码的签名，将采用ECDSA的验证，同时这个签名必须采用严格的DER编码。
在公/私钥对组中，所有执行ECDSA验证的操作，将从栈顶向后迭代。对于每个签名，如果没有通过下面IsValidSignatureEncoding()方法的检查，则整个脚本执行立即失败。如果签名时有效的DER编码，但是没有通过ECDSA验证，操作继续像以前一样执行，操作码执行停止并向栈顶push false(但是不会立即使脚本失败)，在一些案例中，可能跳过一些签名(不使这些签名调用IsValidSignatureEncoding).
## DER编码参考
下面的代码指定了严格的DER检查行为。注意：这个函数测试一个签名字节向量，这个字节向量包含了一个额外字节的比特币签名哈希类型的标识。这个函数不会被长度为0的签名调用，以便为有意填充的无效签名提供一个简单，简短，高效的签名验证。
DER定义在 https://www.itu.int/rec/T-REC-X.690/en .
```c++
bool static IsValidSignatureEncoding(const std::vector<unsigned char> &sig) {
    // Format: 0x30 [total-length] 0x02 [R-length] [R] 0x02 [S-length] [S] [sighash]
    // * total-length: 1-byte length descriptor of everything that follows,
    //   excluding the sighash byte.
    // * R-length: 1-byte length descriptor of the R value that follows.
    // * R: arbitrary-length big-endian encoded R value. It must use the shortest
    //   possible encoding for a positive integers (which means no null bytes at
    //   the start, except a single one when the next byte has its highest bit set).
    // * S-length: 1-byte length descriptor of the S value that follows.
    // * S: arbitrary-length big-endian encoded S value. The same rules apply.
    // * sighash: 1-byte value indicating what data is hashed (not part of the DER
    //   signature)

    // Minimum and maximum size constraints.
    if (sig.size() < 9) return false;
    if (sig.size() > 73) return false;

    // A signature is of type 0x30 (compound).
    if (sig[0] != 0x30) return false;

    // Make sure the length covers the entire signature.
    if (sig[1] != sig.size() - 3) return false;

    // Extract the length of the R element.
    unsigned int lenR = sig[3];

    // Make sure the length of the S element is still inside the signature.
    if (5 + lenR >= sig.size()) return false;

    // Extract the length of the S element.
    unsigned int lenS = sig[5 + lenR];

    // Verify that the length of the signature matches the sum of the length
    // of the elements.
    if ((size_t)(lenR + lenS + 7) != sig.size()) return false;
 
    // Check whether the R element is an integer.
    if (sig[2] != 0x02) return false;

    // Zero-length integers are not allowed for R.
    if (lenR == 0) return false;

    // Negative numbers are not allowed for R.
    if (sig[4] & 0x80) return false;

    // Null bytes at the start of R are not allowed, unless R would
    // otherwise be interpreted as a negative number.
    if (lenR > 1 && (sig[4] == 0x00) && !(sig[5] & 0x80)) return false;

    // Check whether the S element is an integer.
    if (sig[lenR + 4] != 0x02) return false;

    // Zero-length integers are not allowed for S.
    if (lenS == 0) return false;

    // Negative numbers are not allowed for S.
    if (sig[lenR + 6] & 0x80) return false;

    // Null bytes at the start of S are not allowed, unless S would otherwise be
    // interpreted as a negative number.
    if (lenS > 1 && (sig[lenR + 6] == 0x00) && !(sig[lenR + 7] & 0x80)) return false;

    return true;
}
```
## 示例 
符号：p1 和 p2是有效的，序列化后的公钥。 s1 和 s2是对应于p1与 p2的有效签名。s1'与s2'是非DER的编码，但是使用相同公钥的有效签名。F是所有无效的DER兼容签名(包含0，这个空字符串)。F'是无效且非DER兼容的签名。

1. `S1' P1 CHECKSIG` fails (**changed**)
2. `S1' P1 CHECKSIG` NOT fails (unchanged)
3.  `F P1 CHECKSIG` fails (unchanged）
4.  `F P1 CHECKSIG` NOT can succeed (unchanged)
5. `F' P1 CHECKSIG` fails (unchanged)
6. `F' P1 CHECKSIG` NOT fails (**changed**)
7.  `0 S1' S2 2 P1 P2 2 CHECKMULTISIG` fails (**changed**)
8.  `0 S1' S2 2 P1 P2 2 CHECKMULTISIG` NOT fails (unchanged)
9. `0 F S2' 2 P1 P2 2 CHECKMULTISIG` fails (unchanged)
10. `0 F S2' 2 P1 P2 2 CHECKMULTISIG` NOT fails (**changed**)
11. `0 S1' F 2 P1 P2 2 CHECKMULTISIG` fails (unchanged)
12. `0 S1' F 2 P1 P2 2 CHECKMULTISIG` NOT can succeed (unchanged)


注意：上面的例子表明：这种变化仅仅添加了额外的验证失败案例，正如软分叉所要求的那样。
## 部署
我们重复使用BIP34的双阈值切换机制，使用相同的阈值，但是版本号为3.这个新的规则影响版本号为3的所有区块，并且每1000个区块至少含有750个版本号为3的区块。更进一步，当每1000个区块含有950个以上的版本号为3的区块，则版本号为2的区块变为无效，并且后面所有的新区块都将强制采用新规则。
## 兼容
自0.8.0版本后，签名要求严格强制执行DER编码，已被作为中继策略，并且2015年1月后，几乎没有违反该规则的交易被添加到主链上。除此之外，每个非兼容的签名可以平滑的转换到兼容签名，所以不会带来任何功能的丢失。本提案还有降低交易延展性的好处。
## 引用
原文链接：[https://github.com/bitcoin/bips/blob/master/bip-0066.mediawiki](https://github.com/bitcoin/bips/blob/master/bip-0066.mediawiki)

****
本文由 Copernicus团队 姚永芯 翻译，转载无需授权。

