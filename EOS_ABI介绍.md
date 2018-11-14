# EOS ABI介绍

本文对[EOS](https://eos.io/) [ABI](https://developers.eos.io/eosio-cpp/docs/abi)进行简单介绍。



## Wasm介绍

EOS底层使用了[Wasm](https://webassembly.org/)（WebAssembly）技术，所以为了更好的理解EOS ABI，这里先对Wasm虚拟机进行一些介绍。**Wasm**这个词实际上有多重含义，既可以指[W3C](https://www.w3.org/)发布的[Wasm规范](https://webassembly.github.io/spec/)，也可以指[Wasm二进制文件](https://webassembly.github.io/spec/core/binary/index.html)（由[Binaryen](https://github.com/WebAssembly/binaryen)等编译器生成，以.wasm为后缀），还可以指Wasm虚拟机。本文出现的Wasm主要指Wasm虚拟机或者二进制格式，具体因上下文而异。下面列出Wasm虚拟机的一些要点，更详细的信息可以从Wasm规范获取：

* Wasm虚拟机只支持四种内置类型：**i32**（32比特整数）、**i64**（64比特整数）、**f32**（32比特浮点数）、**f64**（64比特浮点数）。其他比如布尔、字符串、结构体、数组、指针等类型需要由编译器提供支持。

* Wasm虚拟机能够操作的存储空间主要包括三部分：

  * **栈** Wasm是[基于栈](https://en.wikipedia.org/wiki/Stack_machine)的虚拟机，并且执行的是[字节码](https://en.wikipedia.org/wiki/Bytecode)，这一点和[JVM](https://en.wikipedia.org/wiki/Java_virtual_machine)、[EVM](https://en.wikipedia.org/wiki/Ethereum#Virtual_Machine)等虚拟机类似。和其他基于栈的虚拟机一样，Wasm指令集里的很大一部分指令都是直接对栈进行操作，比如`i32.const`、 `i32.add`、 `i32.sub`、 `drop`等。
  * **内存** Wasm虚拟机可以操作一个按字节寻址的线性内存空间。内存可以由Wasm虚拟机自己分配，也可以从外部引入（import），但是在[MVP](https://webassembly.org/docs/mvp/)阶段最多只能有一块内存。不管Wasm内存来自于哪儿，都可以按页进行扩展，一页是64KiB。下面是内存操作相关的一些指令：
    * `memory.grow` 使可访问内存增加一页
    * `memory.size` 把当前内存字节数推入栈顶
    * `load`系列指令（比如`i32.load`）把内存数据载入栈顶
    * `store` 系列指令（比如`i32.store`）把栈顶数据写回内存
  * **全局变量** Wasm[模块](https://webassembly.org/docs/modules/)可以从外部引入全局变量，也可以在内部自己定义全局变量，这些全局变量使用同一个**索引空间**。有两条指令可以操作全局变量：
    * `get_global`  获取指定索引处的全局变量值，并推入栈顶
    * `set_global` 从栈顶弹出一个值，并用它设置指定索引处的全局变量



## 生成ABI

EOS智能合约使用C/C++语言编写，可以使用EOS提供的[CDT](https://github.com/EOSIO/eosio.cdt)（Contract Development Toolkit）对合约进行编译。以EOS开发文档里的[hello合约](https://developers.eos.io/eosio-home/docs/your-first-contract)为例：

```cpp
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>

using namespace eosio;

class hello : public contract {
  public:
      using contract::contract;

      [[eosio::action]]
      void hi( name user ) {
         print( "Hello, ", name{user});
      }
};
EOSIO_DISPATCH( hello, (hi))
```

使用`eosio-cpp -o hello.wasm hello.cpp --abigen`命令可以编译出hello.wasm和hello.abi文件。下面是hello.abi文件的内容：

```json
{
    "____comment": "This file was generated with eosio-abigen. DO NOT EDIT Wed Nov 14 14:20:08 2018",
    "version": "eosio::abi/1.0",
    "structs": [
        {
            "name": "hi",
            "base": "",
            "fields": [
                {
                    "name": "user",
                    "type": "name"
                }
            ]
        }
    ],
    "types": [],
    "actions": [
        {
            "name": "hi",
            "type": "hi",
            "ricardian_contract": ""
        }
    ],
    "tables": [],
    "ricardian_clauses": [],
    "abi_extensions": []
}
```

从上面的信息可以看出：

* EOS智能合约的ABI描述文件采用**JSON**格式
* 智能合约的ABI描述文件实际上是由CDT里的**eosio-abigen**这个工具生成的
* ABI描述文件对智能合约的每一个**[action handler](https://github.com/EOSIO/eos/wiki/Glossary)**进行了描述，根据这些描述就可以知道action handler接收的参数类型和数量，从而可以发起action调用handler

那么EOS智能合约到底是怎么开始执行的呢？下面来详细讨论一下。



## EOSIO_DISPATCH魔法

由[ABI文档](https://developers.eos.io/eosio-cpp/docs/abi)可知，每一个EOS智能合约都必须提供一个叫做`apply`的action handler，这个apply先接收到action，然后再把action分发给具体的handler进行处理。上面的hello合约并没有直接定义这个apply handler，那么必然是`EOSIO_DISPATCH`帮我们干了这件事。我们可以从[eosio.cdt](https://github.com/EOSIO/eosio.cdt)的源代码中找到这个宏，具体在[dispatcher.hpp](https://github.com/EOSIO/eosio.cdt/blob/master/libraries/eosiolib/dispatcher.hpp#L123)文件里，下面是它的代码：

```cpp
// dispatcher.hpp#L123
#define EOSIO_DISPATCH( TYPE, MEMBERS ) \
extern "C" { \
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
      if( code == receiver ) { \
         switch( action ) { \
            EOSIO_DISPATCH_HELPER( TYPE, MEMBERS ) \
         } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } \
   } \
} \
```

可见`apply()`函数的确是由这个宏生成的。虽然并不能完全理解这段代码的含义，但大致也可以看出是根据code和action参数去选择具体的handler。我们接着看一下`EOSIO_DISPATCH_HELPER`这个宏：

```cpp
// dispatcher.hpp#L102
// Helper macro for EOSIO_DISPATCH
#define EOSIO_DISPATCH_HELPER( TYPE,  MEMBERS ) \
   BOOST_PP_SEQ_FOR_EACH( EOSIO_DISPATCH_INTERNAL, TYPE, MEMBERS )
```

这个宏又用了`EOSIO_DISPATCH_INTERNAL`宏，只好顺藤摸瓜继续看：

```cpp
// dispatcher.hpp#L96
// Helper macro for EOSIO_DISPATCH_INTERNAL
#define EOSIO_DISPATCH_INTERNAL( r, OP, elem ) \
   case eosio::name( BOOST_PP_STRINGIZE(elem) ).value: \
      eosio::execute_action( eosio::name(receiver), eosio::name(code), &OP::elem ); \
      break;
```

可见最后调用的是`execute_action()`函数，为了更清楚的了解事实，我们可以使用`-E`选项（告诉eosio-cpp编译器仅进行**预处理**）重新编译hello合约。下面是预处理之后的`apply()`方法代码：

```cpp
extern "C" {
    void apply( uint64_t receiver, uint64_t code, uint64_t action ) { 
        if( code == receiver ) { 
            switch( action ) { 
                case eosio::name( "hi" ).value: 
                    eosio::execute_action( 
                        eosio::name(receiver), 
                        eosio::name(code), 
                        &hello::hi ); 
                    break; 
            }
        }
    }
}
```

预处理器生成的`apply()`方法基本上跟我们理解的一致，接下来看一下`execute_action()`方法。



## eosio::execute_action()方法

下面是`execute_action()`的代码，这是一个模版方法：

```cpp
// dispatcher.hpp#L65
   template<typename T, typename... Args>
   bool execute_action( name self, name code, void (T::*func)(Args...)  ) {
      size_t size = action_data_size();

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      constexpr size_t max_stack_buffer_size = 512;
      void* buffer = nullptr;
      if( size > 0 ) {
         buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
         read_action_data( buffer, size );
      }
      
      std::tuple<std::decay_t<Args>...> args;
      datastream<const char*> ds((char*)buffer, size);
      ds >> args;
      
      T inst(self, code, ds);

      auto f2 = [&]( auto... a ){
         ((&inst)->*func)( a... );
      };

      boost::mp11::tuple_apply( f2, args );
      if ( max_stack_buffer_size < size ) {
         free(buffer);
      }
      return true;
   }
```

虽然代码比较难懂，但还是可以看出，`execute_action()`函数调用了`action_data_size()`函数获取action数据的字节数，还调用了`read_action_data()`函数读取action数据。从[action.h](https://github.com/EOSIO/eosio.cdt/blob/master/libraries/eosiolib/action.h)头文件中可以找到这两个函数的声明：

```c
#pragma once
#include <eosiolib/system.h>

extern "C" {
   uint32_t read_action_data( void* msg, uint32_t len );
   uint32_t action_data_size();
   ... // 其他函数省略
}
```

我们用[Wagon](https://github.com/go-interpreter/wagon)提供的wasm-dump工具查看hello合约编译出来的hello.wasm，由[import段](https://webassembly.org/docs/modules/#imports)可以确认上面两个函数的确是由外部提供的，也就是说是由EOS的Wasm实现提供的：

```
import:
 - function[0] sig=1 <- env.action_data_size
 - function[1] sig=2 <- env.read_action_data
 - function[2] sig=3 <- env.eosio_assert
 - function[3] sig=4 <- env.memcpy
 - function[4] sig=5 <- env.prints
 - function[5] sig=6 <- env.printn
 - function[6] sig=3 <- env.set_blockchain_parameters_packed
 - function[7] sig=2 <- env.get_blockchain_parameters_packed
 - function[8] sig=4 <- env.memset
```

到这里基本上可以得出结论，EOS智能合约的action数据是由合约字节码通过**外部函数**读入的，而且合约字节码也已经包含了action数据的解码逻辑，ABI仅仅在编码action数据时使用。



## 总结

1. 使用`eosio-cpp`编译EOS智能合约时，可以通过`--abi`选项生成合约的ABI描述文件；ABI描述采用JSON格式。
2. EOS智能合约需要一个`apply()`入口函数，这个函数使用一个`switch-case`语句进行action分派，可以通过EOS提供的`EOSIO_DISPATCH`宏生成。
3. 解码action数据的逻辑由`eosio::execute_action()`函数实现，这个函数通过两个外部函数读入action数据，然后进行数据解码，最后调用具体的handler函数。

