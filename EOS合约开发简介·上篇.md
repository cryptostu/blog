## EOS合约开发简介·上篇

**EOSIO.CDT ( Contract Development Toolkit )，一套用来构建智能合约的工具集**

Wormhole在开发智能合约的过程中，我们对市场上现有的几种合约平台都做了调研。本系列文章我们主要介绍一下基于EOS合约平台如何去开发一个合约，我们会由简入深分成好几篇文章来对EOS开发合约这一过程进行介绍，本篇是这一系列文章的第一篇，我们先从最简单的一个合约：hello合约开始。

##Hello合约
hello.hpp中定义了一个最简单的hello合约，只包含一个名为hi的action，所谓action就是添加到apply函数中的一个case分支。 而apply函数是EOS虚拟机中合约的执行入口。

看一下hello.hpp

```c++
#include <eosiolib/eosio.hpp>

using namespace eosio;

CONTRACT hello : public eosio::contract {
  public:
      using contract::contract;

      ACTION hi( name user );

      // accessor for external contracts to easily send inline actions to your contract
      using hi_action = action_wrapper<"hi"_n, &hello::hi>;
};
```

每一个合约都要继承contract基类

```c++
class contract {
   public:
      /**
       * Construct a new contract given the contract name
       *
       * @brief Construct a new contract object.
       * @param receiver - The name of this contract
       * @param code - The code name of the action this contract is processing.
       * @param ds - The datastream used 
       */
      contract( name receiver, name code, datastream<const char*> ds ):_self(receiver),_code(code),_ds(ds) {}
```

contract基类里只有三个成员和三个获取成员的方法，很简单。

using contract::contract；是c++11的新特性，继承构造。

```c++
#define CONTRACT class [[eosio::contract]]
#define ACTION   [[eosio::action]] void
```

ACTION hi( name user )，声明了一个hi方法，其实就是一个普通函数声明，[[eosio::action]]属性用于生成ABI，重点是下面的这句：

```c++
using hi_action = action_wrapper<"hi"_n, &hello::hi>;
```

上面语句用hi方法的名字和函数引用实例化了action包装器，之后可以用hi_action来实例化包装器对象。

这里的“hi”_n等价于name("hi")，实例化一个name对象，并用“hi”初始化，那么action包装器的机构到底是什么样子的？

##action_wrapper结构
action_wrapper结构定义如下：

```c++
template <eosio::name::raw Name, auto Action>
   struct action_wrapper {
      template <typename Code>
      constexpr action_wrapper(Code&& code, std::vector<eosio::permission_level>&& perms)
         : code_name(std::forward<Code>(code)), permissions(std::move(perms)) {}
      template <typename Code>
      constexpr action_wrapper(Code&& code, const std::vector<eosio::permission_level>& perms)
         : code_name(std::forward<Code>(code)), permissions(perms) {}
      template <typename Code>
      constexpr action_wrapper(Code&& code, eosio::permission_level&& perm)
         : code_name(std::forward<Code>(code)), permissions({1, std::move(perm)}) {}
      template <typename Code>
      constexpr action_wrapper(Code&& code, const eosio::permission_level& perm)
         : code_name(std::forward<Code>(code)), permissions({1, perm}) {}
      static constexpr eosio::name action_name = eosio::name(Name);
      eosio::name code_name;
      std::vector<eosio::permission_level> permissions;
      static constexpr auto get_mem_ptr() {
         return Action;
      }
      template <typename... Args>
      action to_action(Args&&... args)const {
         static_assert(detail::type_check<Action, Args...>());
         return action(permissions, code_name, action_name, detail::deduced<Action>{std::forward<Args>(args)...});
      }
      template <typename... Args>
      void send(Args&&... args)const {
         to_action(std::forward<Args>(args)...).send();
      }

      template <typename... Args>
      void send_context_free(Args&&... args)const {
         to_action(std::forward<Args>(args)...).send_context_free();
      }

   };
```

注意里面的send方法，通过该方法可以构建一个action对象，并调用该action对象的send方法将序列化后的action对象通过系统函数send_inline发送出去。这个方法实际是EOS合约调用两种方式中的inline调用，还有一种是外部调用。action的相关组件都在action.hpp中。

action类的send方法：

```c++
 void send() const {
         auto serialize = pack(*this);
         ::send_inline(serialize.data(), serialize.size());
      }
```

其中pack方法定义如下，位于datastream.hpp中，这个文件专门用于对象的序列化，反序列化，其中最重要的是action对象的序列化，里面一大堆模板特化。

```c++
template<typename T>
std::vector<char> pack( const T& value ) {
  std::vector<char> result;
  result.resize(pack_size(value));
  datastream<char*> ds( result.data(), result.size() );
  ds << value;
  return result;
}
```

EOS官方在send_inline这个合约中定义了ACTION test，这个test里调用了合约hello里的hi方法，实现了合约的inline调用，代码如下：

```c++
 ACTION test( name user, name inline_code ) {
         print_f( "Hello % from send_inline", user );
         // constructor takes two arguments (the code the contract is deployed on and the set of permissions)
         hello::hi_action hi(inline_code, {_self, "active"_n});
         hi.send(user);
      }
```

可以看到这里用到了我们在hello合约中定义的ACTION包装器hi_action。

语句{_self, "active"_n}构造了一个permission_level结构体对象，也定义在action.hpp中，这里有一个遗留，ACTION中的各个成员各自对应什么需要后续研究下。 

##Action hi的实现
让我们的思路再回到hello合约，上面的篇幅讲到了hello合约的定义，下面看一下action hi的实现，其实现位于hello.cpp中。

```c++
ACTION hello::hi( name user ) {
   print_f( "Hello % from hello", user );
}
EOSIO_DISPATCH( hello, (hi) )
```

可以看到玄机都在最后一个宏里，这个宏位于dispatcher.hpp中，用于Action分发。

```c++
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

这里为合约定义了虚拟机入口函数apply，如果合约名字和被调用方法的代码名字相同，说明调用的方法是该合约的，name执行switch-case进行具体的action处理。

```
#define EOSIO_DISPATCH_HELPER( TYPE,  MEMBERS ) \
   BOOST_PP_SEQ_FOR_EACH( EOSIO_DISPATCH_INTERNAL, TYPE, MEMBERS )
```

EOSIO_DISPATCH_HELPER将MEMBERS序列中的各个参数区分开，并按参数个数重复执行三元宏EOSIO_DISPATCH_INTERNAL，其中MEMBERS的格式是(hi)(bye)(say)。

```c++
#define EOSIO_DISPATCH_INTERNAL( r, OP, elem ) \
   case eosio::name( BOOST_PP_STRINGIZE(elem) ).value: \
      eosio::execute_action( eosio::name(receiver), eosio::name(code), &OP::elem ); \
      break;
```

到这里可以看到依次执行MEMBERS中的参数，实际上是执行一个个case语句，这里的elem就是某一个MEMBERS中的元素，也就是合约中定义的某个action。

这里BOOST_PP_STRINGIZE(elem) 将elem转化为字符串，并实例化了name类，最终和switch里的action比较的是name的value值，它实际是elem字符串经过base32解码后的uint64值。这个值在同一个合约中是唯一的。

这里要提一句，EOS中的函数，合约名，代码名都属于叫做name的类，该类约定了上述名称必须是小于等于13个字符，且字符选取自集合{.12345a-z}。name类中提供了base32编解码方法。

那么case语句中真正执行action的语句是

```
eosio::execute_action( eosio::name(receiver), eosio::name(code), &OP::elem );
```

注意下这里的三个参数：合约名，代码名，action的引用。

```
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

这个函数详细拆解下，首先它是一个函数模板，第三个参数是类成员函数指针，还用到了c++11的参数包。这个函数实例了一个合约对象，并调用了合约对象里相应的action方法。

```
 size_t size = action_data_size();

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      constexpr size_t max_stack_buffer_size = 512;
      void* buffer = nullptr;
      if( size > 0 ) {
         buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
         read_action_data( buffer, size );
      }
```

这一段是虚拟机根据action的data size分配虚拟机内存。然后通过系统方法read_action_data将action的参数部分copy进buffer里。

```
std::tuple<std::decay_t<Args>...> args;
      datastream<const char*> ds((char*)buffer, size);
      ds >> args;
```

这一段是按照成员函数的参数列表定义一个元组，然后用存有action参数的buffer实例化一个datastream，并通过datasteam的反序列化方法，将ds中参数放置到元组args中。其中std::decay_t实现参数退化，去掉参数中的const和引用。

```
T inst(self, code, ds);
auto f2 = [&]( auto... a ){
   ((&inst)->*func)( a... );
};
boost::mp11::tuple_apply( f2, args );
if ( max_stack_buffer_size < size ) {
    free(buffer);
}
```

这一段首先实例化了一个合约类，这里是hello对象。接下来是一个lamda表达式，意思是以引用的方式捕捉周围作用域内的变量，这些变量可以用在接下来定义的匿名函数中，这个匿名函数被赋值给f2的函数指针，该函数实际是执行了hello类中的hi方法。boost::mp11::tuple_apply( f2, args )将元组args传给f2函数，并执行。到这一步，我们就完成了一个action的调用。






