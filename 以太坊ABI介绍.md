# 以太坊ABI介绍



## Calldata介绍

由于ABI主要约定了Calldata的编码方式，所以首先简单介绍一下EVM存储和Calldata。EVM能够访问的存储空间主要有四类：

* **Stack** EVM是基于栈的虚拟机，EVM栈最多有**1024**个元素，每个元素都是**256比特**宽（也就是一个Word）。栈可以通过**PUSH**、**POP**、**SWAP**、**DUP**等指令直接操作，**ADD**、**SUB**等指令的执行则完全依赖于栈。
* **Memory** EVM的Memory类似于物理计算机的**内存（RAM）**，是非持久化的，合约执行完毕之后就会丢弃。EVM的Memory本质上是一个无限可扩展（只要有足够的Gas）的字节数组，可以按字节寻址和存取。操作Memory的指令有四条：
  * **MLOAD**指令把Memory里的一个Word载入栈顶
  * **MSTORE**指令把栈顶的一个Word写回Memory
  * **MSTORE8**指令和MSTORE指令类似，但是每次只写入一个字节
  * **MSIZE**指令把当前Memory的字节数推入栈顶
* **Storage** EVM的Storage类似于物理计算机的**硬盘**，是持久化的，合约的状态就存储在Storage里（最终会写到链上）。EVM的Storage本质上是一个近似无限大（2^256）的KV存储，键和值都是256比特Word。操作Storage的指令有两条：
  * **SLOAD**指令从栈顶弹出键，然后从Storage里取相应的值并推入栈顶
  * **SSTORE**指令从栈顶弹出键值对，然后根据键把值写回Storage
* **Calldata** 这是一块只读存储（ROM），也是按字节寻址。当通过交易调用合约，或者在合约内部调用其他合约的**外部**函数时，参数通过Calldata提供。操作Calldata的指令有三条：
  * **CALLDATASIZE**指令和MSIZE指令类似，把Calldata的字节数推入栈顶
  * **CALLDATALOAD**指令把Calldata里的某个Word载入栈顶
  * **CALLDATACOPY**指令把Calldata里的一块数据拷贝到Memory里



## Calldata编码

Calldata编码格式由[以太坊ABI规范文档](https://solidity.readthedocs.io/en/latest/abi-spec.html)约定。简单来说，Calldata包含两部分数据：

* **函数选择器（Function Selector）** 占用四个字节，用于定位合约中将要被调用的函数。这四个字节来自于函数签名的Keccak-256哈希的前四个字节，由编译器生成，可以用伪代码表示为：`keecak256(signature(function))[0:4]`
* **编码后的参数列表（Encoded Parameters）** 从第五个字节开始是编码后的参数列表，具体字节数因函数的参数类型和数量而定

ABI具体支持的参数类型和编码方式请参考ABI规范文档，这里要说明的是，Calldata的编码格式并不是自描述的，所以必须要了解函数签名，或者有描述文件才能对Calldata进行解码。换句话说，Calldata的编码方式更像[Protobuf](https://developers.google.com/protocol-buffers/)而非[JSON](https://www.json.org/)。



## ABI生成

编译[Solidity](https://solidity.readthedocs.io/en/latest/index.html)智能合约时，可以通过`--abi`选项告诉编译器生成ABI描述。比如下面给出[ABI规范](https://solidity.readthedocs.io/en/latest/abi-spec.html)里的一个例子：

```solidity
pragma solidity >=0.4.16 <0.6.0;

contract Foo {
  function bar(bytes3[2] memory) public pure {}
  function baz(uint32 x, bool y) public pure returns (bool r) { r = x > 32 || y; }
  function sam(bytes memory, bool, uint[] memory) public pure {}
}
```

把上面的智能合约代码保存在foo.sol文件里，用`solc --abi foo.sol`命令编译该文件，可以在控制台看到输出的ABI描述（JSON格式）：

```
$ solc --abi foo.sol 

======= foo.sol:Foo =======
Contract JSON ABI 
[
   {
      "constant":true,
      "inputs":[
         { "name":"", "type":"bytes" },
         { "name":"", "type":"bool" },
         { "name":"", "type":"uint256[]" }
      ],
      "name":"sam",
      "outputs":[],
      "payable":false,
      "stateMutability":"pure",
      "type":"function"
   },
   {
      "constant":true,
      "inputs":[
         { "name":"x", "type":"uint32" },
         { "name":"y", "type":"bool" }
      ],
      "name":"baz",
      "outputs":[
         { "name":"r", "type":"bool" }
      ],
      "payable":false,
      "stateMutability":"pure",
      "type":"function"
   },
   {
      "constant":true,
      "inputs":[
         { "name":"", "type":"bytes3[2]" }
      ],
      "name":"bar",
      "outputs":[],
      "payable":false,
      "stateMutability":"pure",
      "type":"function"
   }
]
```

可以看到，ABI中包含了函数名，参数数量和类型，返回值数量和类型等信息。当通过交易调用合约时，需要ABI描述来把被调函数名称和参数列表编码成Calldata数据。



## 合约调用

前面我们简单介绍了Solidity编译器（solc）的`--abi`选项，这里再介绍几个其他选项：

* **`--bin`** 这个选项告诉编译器把编译后的智能合约字节码以hex格式输出到控制台，这样输出的实际上是**部署代码**，真正的**运行时代码**以数据的形式保护在部署代码后半段中
* **`--bin-runtime`** 这个选项告诉编译器把编译后的智能合约**运行时代码**以hex格式输出到控制台
* **`--opcodes`** 这个选项告诉编译器把编译后的智能合约字节码以Opcode形式输出到控制台。相比hex格式，Opcode形式更容易阅读
* **`--asm`** 这个选项告诉编译器把编译后的汇编码输出到控制台，相比hex和Opcode形式，汇编码更适合人类阅读

仍以foo.sol为例，下面是用`--bin-runtime`选项进行编译的输出结果：

```
$ solc --bin-runtime foo.sol 

======= foo.sol:Foo =======
Binary of the runtime part: 
608060405260043610610057576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063a5643bf21461005c578063cdcd77c014610114578063fce353f61461016b575b600080fd5b34801561006857600080fd5b50610112600480360381019080803590602001908201803590602001908080601f0160208091040260200160405190810160405280939291908181526020018383808284378201915050505050509192919290803515159060200190929190803590602001908201803590602001908080602002602001604051908101604052809392919081815260200183836020028082843782019150505050505091929192905050506101bd565b005b34801561012057600080fd5b50610151600480360381019080803563ffffffff1690602001909291908035151590602001909291905050506101c2565b604051808215151515815260200191505060405180910390f35b34801561017757600080fd5b506101bb60048036038101908080604001906002806020026040519081016040528092919082600260200280828437820191505050505091929192905050506101de565b005b505050565b600060208363ffffffff1611806101d65750815b905092915050565b505600a165627a7a72305820547efbdaf31172da7c479533ce74ac5c90219733d7d14406cd5d836bd111ab130029

```

不管是hex、Opcode还是ASM形式，都不是很容易就能理解其中的内容。我们把上面的hex字节码输入[Solidity在线反编译器](https://ethervm.io/decompile)，可以得到更直观易懂的Solidity代码：

```solidity
contract Contract {
    function main() {
        memory[0x40:0x60] = 0x80;
    
        if (msg.data.length < 0x04) { revert(memory[0x00:0x00]); }
    
        var var0 = msg.data[0x00:0x20] / 0x0100000000000000000000000000000000000000000000000000000000 & 0xffffffff;
    
        if (var0 == 0xa5643bf2) {
            // Dispatch table entry for 0xa5643bf2 (unknown)
            var var1 = msg.value;
        
            if (var1) { revert(memory[0x00:0x00]); }
        
            var1 = 0x0112;
            var temp0 = msg.data[0x04:0x24] + 0x04;
            var temp1 = msg.data[temp0:temp0 + 0x20];
            var temp2 = memory[0x40:0x60];
            memory[0x40:0x60] = temp2 + (temp1 + 0x1f) / 0x20 * 0x20 + 0x20;
            memory[temp2:temp2 + 0x20] = temp1;
            memory[temp2 + 0x20:temp2 + 0x20 + temp1] = msg.data[temp0 + 0x20:temp0 + 0x20 + temp1];
            var var2 = temp2;
            var var3 = !!msg.data[0x24:0x44];
            var temp3 = msg.data[0x44:0x64] + 0x04;
            var temp4 = msg.data[temp3:temp3 + 0x20];
            var temp5 = memory[0x40:0x60];
            memory[0x40:0x60] = temp5 + temp4 * 0x20 + 0x20;
            memory[temp5:temp5 + 0x20] = temp4;
            var temp6 = temp4 * 0x20;
            memory[temp5 + 0x20:temp5 + 0x20 + temp6] = msg.data[temp3 + 0x20:temp3 + 0x20 + temp6];
            var var4 = temp5;
            func_01BD(var2, var3, var4);
            stop();
        } else if (var0 == 0xcdcd77c0) {
            // Dispatch table entry for baz(uint32,bool)
            var1 = msg.value;
        
            if (var1) { revert(memory[0x00:0x00]); }
        
            var1 = 0x0151;
            var2 = msg.data[0x04:0x24] & 0xffffffff;
            var3 = !!msg.data[0x24:0x44];
            var1 = baz(var2, var3);
            var temp7 = memory[0x40:0x60];
            memory[temp7:temp7 + 0x20] = !!var1;
            var temp8 = memory[0x40:0x60];
            return memory[temp8:temp8 + (temp7 + 0x20) - temp8];
        } else if (var0 == 0xfce353f6) {
            // Dispatch table entry for 0xfce353f6 (unknown)
            var1 = msg.value;
        
            if (var1) { revert(memory[0x00:0x00]); }
        
            var1 = 0x01bb;
            var temp9 = memory[0x40:0x60];
            memory[0x40:0x60] = temp9 + 0x20 * 0x02;
            memory[temp9:temp9 + 0x20 * 0x02] = msg.data[0x04:0x44];
            var2 = temp9;
            func_01DE(var2);
            stop();
        } else { revert(memory[0x00:0x00]); }
    }
    
    function func_01BD(var arg0, var arg1, var arg2) {}
    
    function baz(var arg0, var arg1) returns (var r0) {
        var var0 = 0x00;
        var var1 = arg0 & 0xffffffff > 0x20;
    
        if (var1) { return var1; }
        else { return arg1; }
    }
    
    function func_01DE(var arg0) {}
}
```

从上面的反编译结果可以看到，编译后的智能合约字节码，实际上有一个**入口函数**（`main()`函数）。当EVM执行智能合约时，实际上会先进入这个入口函数。然后入口函数负责解码Calldata，取出函数签名哈希，选择被调函数，并传入解码后的参数值。



## 总结

1. 调用合约时，需要给定函数名和参数列表。函数名和参数列表通过ABI约定的格式编码成Calldata数据提供给合约，合约字节码可以通过特定的指令访问只读的Calldata数据。
2. Calldata编码格式是非自描述的，需要了解参数数量和类型才能解码Calldata数据。
3. 编译合约时，可以通过`--abi`选项输出合约的ABI描述；ABI描述采用JSON格式。
4. Calldata解码逻辑（以及函数分派逻辑）是由Solidity编译器生成的，硬编码在运行时字节码里，因此合约执行时，只需要Calldata即可，并不需要ABI描述文件。

