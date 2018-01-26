---
title: Bitcoin序列化库使用
date: 2018-01-26 15:31:38
tags:
---
Bitcoin序列化功能主要实现在`serialize.h`文件，整个代码主要是围绕`stream`和参与序列化反序列化的类型**T**展开。 

stream这个模板形参表达具有`read(char**, size_t)` 和` write(char**, size_t) `方法的对象， 类似Golang 的io.reader ,io.writer。

简单的使用例子：

```c++
#include <serialize.h>
#include <streams.h>
#include <hash.h>
#include <test/test_bitcoin.h>

#include <stdint.h>
#include <memory>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(serialize_tests, BasicTestingSetup)


struct  student
{
	std::string name;
	double midterm, final;
	std::vector<double> homework;

	ADD_SERIALIZE_METHODS;

	template <typename Stream, typename Operation>
	inline void SerializationOp(Stream& s, Operation ser_action) {
		READWRITE(name);
		READWRITE(midterm);
		READWRITE(final);
		READWRITE(homework);
	}
		
};

bool operator==(student const& lhs,  student const& rhs){
		return lhs.name == rhs.name &&  \
		       lhs.midterm ==  rhs.midterm && \
		       lhs.final  ==  rhs.final && \
		       lhs.homework == rhs.homework;
}

std::ostream& operator<<(std::ostream& os, student const& st){
		os << "name: " << st.name << '\n' 
		   << "midterm: " << st.midterm << '\n'
		   << "final: "   << st.final  << '\n'
		   << "homework: " ;
		for (auto e : st.homework) {
			os << e <<  ' ';
		}
		return os;
}


BOOST_AUTO_TEST_CASE(normal)
{
    student  s, t;
    s.name = "john";
    s.midterm = 77;
    s.final = 82;
    auto  v = std::vector<double> {83, 50, 10, 88, 65};
    s.homework = v;

    CDataStream ss(SER_DISK, 0);
    ss <<  s;
    ss >>  t; 

    BOOST_CHECK(t.name  == "john");
    BOOST_CHECK(t.midterm  == 77);
    BOOST_CHECK(t.final  == 82);
    BOOST_TEST(t.homework == v,  boost::test_tools::per_element()); 
    

    CDataStream sd(SER_DISK, 0);
    CDataStream sn(SER_NETWORK, PROTOCOL_VERSION);
    sd << s;
    sn << s;
    BOOST_CHECK(Hash(sd.begin(), sd.end()) == Hash(sn.begin(), sn.end()));
}

BOOST_AUTO_TEST_CASE(vector)
{
    auto vs = std::vector<student>(3);
    vs[0].name = "bob";
    vs[0].midterm = 90;
    vs[0].final = 76;
    vs[0].homework = std::vector<double> {85, 53, 12, 75, 55};

    vs[1].name = "jim";
    vs[1].midterm = 96;
    vs[1].final = 72;
    vs[1].homework = std::vector<double> {91, 46, 19, 70, 59};

    vs[2].name = "tom";
    vs[2].midterm = 85;
    vs[2].final = 57;
    vs[2].homework = std::vector<double> {91, 77, 45, 50, 35};


    CDataStream ss(SER_DISK, 0);
    auto vt = std::vector<student>(3);
    ss <<  vs;
    ss >>  vt; 

    BOOST_TEST(vs == vt,  boost::test_tools::per_element()); 
}

BOOST_AUTO_TEST_CASE(unique_ptr){
	auto hex = "0100000001b14bdcbc3e01bdaad36cc08e81e69c82e1060bc14e518db2b49aa43ad90ba26000000000490047304402203f16c6f40162ab686621ef3000b04e75418a0c0cb2d8aebeac894ae360ac1e780220ddc15ecdfc3507ac48e1681a33eb60996631bf6bf5bc0a0682c4db743ce7ca2b01ffffffff0140420f00000000001976a914660d4ef3a743e3e696ad990364e555c271ad504b88ac00000000";
	CDataStream stream(ParseHex(hex), SER_NETWORK, PROTOCOL_VERSION);
        //CTransaction tx(deserialize, stream);
	auto utx = std::unique_ptr<const CTransaction>(nullptr);
	::Unserialize(stream, utx);
	BOOST_TEST(utx->vin.size() == std::size_t(1));
	BOOST_TEST(utx->vout[0].nValue == 1000000);
}

BOOST_AUTO_TEST_SUITE_END()
```

需要在用户的自定义类型内部 添加 **ADD_SERIALIZE_METHODS**  调用， 宏展开后：
```c++
template<typename Stream>                                         \
    void Serialize(Stream& s) const {                                 \
        NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize());  \
    }                                                                 \
    template<typename Stream>                                         \
    void Unserialize(Stream& s) {                                     \
        SerializationOp(s, CSerActionUnserialize());                  \
    }

```
这个宏为用户自定义类型添加了两个成员函数： **Serialize** 和 **Unserialize**， 它们内部调用需要用户自定义的模板成员函数**SerializationOp** ,  在 **SerializationOp** 函数内部， 主要使用 **READWRITE** 和 **READWRITEMANY** 宏，完成对自定义类型每个数据成员的序列化与反序列化。

```c++
#define READWRITE(obj)      (::SerReadWrite(s, (obj), ser_action))
#define READWRITEMANY(...)      (::SerReadWriteMany(s, ser_action, __VA_ARGS__))

struct CSerActionSerialize
{
    constexpr bool ForRead() const { return false; }
};
struct CSerActionUnserialize
{
    constexpr bool ForRead() const { return true; }
};

template<typename Stream, typename T>
inline void SerReadWrite(Stream& s, const T& obj, CSerActionSerialize ser_action)
{
    ::Serialize(s, obj);
}

template<typename Stream, typename T>
inline void SerReadWrite(Stream& s, T& obj, CSerActionUnserialize ser_action)
{
    ::Unserialize(s, obj);
}

template<typename Stream, typename... Args>
inline void SerReadWriteMany(Stream& s, CSerActionSerialize ser_action, Args&&... args)
{
    ::SerializeMany(s, std::forward<Args>(args)...);
}

template<typename Stream, typename... Args>
inline void SerReadWriteMany(Stream& s, CSerActionUnserialize ser_action, Args&... args)
{
    ::UnserializeMany(s, args...);
}

```

需要在用户的自定义类型内部 添加 ****ADD_SERIALIZE_METHODS**** 调用， 宏展开后：

```c++

template<typename Stream>  \

 void Serialize(Stream& s) const {  \

 NCONST_PTR(this)->SerializationOp(s, CSerActionSerialize()); \

 }  \

 template<typename Stream>  \

 void Unserialize(Stream& s) {  \

 SerializationOp(s, CSerActionUnserialize()); \

 }

```

这个宏为用户自定义类型添加了两个成员函数：  `Serialize`  和  `Unserialize`，  它们内部调用需要用户自定义的模板成员函数`SerializationOp` , 在  `SerializationOp`  函数内部，  主要使用  `READWRITE` 和  `READWRITEMANY`  宏，完成对自定义类型每个数据成员的序列化与反序列化。

```c++
#define READWRITE(obj)      (::SerReadWrite(s, (obj), ser_action))
#define READWRITEMANY(...)      (::SerReadWriteMany(s, ser_action, __VA_ARGS__))

struct CSerActionSerialize
{
    constexpr bool ForRead() const { return false; }
};
struct CSerActionUnserialize
{
    constexpr bool ForRead() const { return true; }
};

template<typename Stream, typename T>
inline void SerReadWrite(Stream& s, const T& obj, CSerActionSerialize ser_action)
{
    ::Serialize(s, obj);
}

template<typename Stream, typename T>
inline void SerReadWrite(Stream& s, T& obj, CSerActionUnserialize ser_action)
{
    ::Unserialize(s, obj);
}

template<typename Stream, typename... Args>
inline void SerReadWriteMany(Stream& s, CSerActionSerialize ser_action, Args&&... args)
{
    ::SerializeMany(s, std::forward<Args>(args)...);
}

template<typename Stream, typename... Args>
inline void SerReadWriteMany(Stream& s, CSerActionUnserialize ser_action, Args&... args)
{
    ::UnserializeMany(s, args...);
}

```

这里SerReadWrite 和 SerReadWriteMany 各自有两个overload 实现， 区别是末尾分别传入了不同的类型`CSerActionSerialize` 和 `CSerActionUnserialize` , 而且 形参 ser_action 根本没有在内部使用， 查阅了相关资料， 这里使用了c++ 泛型编程常用的一种模式：

[tag dispatch 技术](https://akrzemi1.wordpress.com/examples/overloading-tag-dispatch/)](https://akrzemi1.wordpress.com/examples/overloading-tag-dispatch/), 另一个解释:[https://arne-mertz.de/2016/10/tag-dispatch/)(https://arne-mertz.de/2016/10/tag-dispatch/)，

通过携带不同的类型，在编译时选择不同的overload 实现， CSerActionSerialize 对应序列化的实现， CSerActionUnserialize 对应反序列化的实现。

`SerializeMany`  和  `SerializeMany`是通过变长模板parameter pack 展开技术来实现，  以  `SerializeMany` 为例子：

```c++

template<typename Stream>
void SerializeMany(Stream& s)
{
}

template<typename Stream, typename Arg>
void SerializeMany(Stream& s, Arg&& arg)
{
    ::Serialize(s, std::forward<Arg>(arg));
}

template<typename Stream, typename Arg, typename... Args>
void SerializeMany(Stream& s, Arg&& arg, Args&&... args)
{
    ::Serialize(s, std::forward<Arg>(arg));
    ::SerializeMany(s, std::forward<Args>(args)...);
}

```

`SerializeMany`有三个overload 实现,假设从上倒下,分别编号为1， 2， 3; 当我们传入两个以上的实参是,编译器选择版本3，版本3内部从parameter pack 弹出一个参数,然后传给版本2调用,剩下的参数列表，传给版本3，递归调用,直到parameter pack 为空时,选择版本1。

迂回这么长，  最终序列化真正使用全局名称空间的  `Serialize` 来完成，  反序列化通过调用`Unserialize`实现。

而 `Serialize` 和`Unserialize` 又有一堆的overload 实现， Bitcoin 作者实现一些常见类型的模板特化，比如，std::string,  主要设计表达脚本的prevector , std::vector, std::pair, std::map, std::set, std::unique_ptr, std::share_ptr 。  c++ 的模板匹配根据参数列表的匹配程度选择不同的实现， 优先精准匹配，最后选择类型T的成员函数实现：

```c++
template<typename Stream, typename T>
inline void Serialize(Stream& os, const T& a)
{
    a.Serialize(os);
}

template<typename Stream, typename T>
inline void Unserialize(Stream& is, T& a)
{
    a.Unserialize(is);
}
```

在序列化string, map, set, vector, prevector 等可能包含多元素的集合类型时， 内部会调用 `ReadCompactSize`和 `WriteCompactSize`读取写入紧凑编码的元素个数：

```c++
template<typename Stream>
void WriteCompactSize(Stream& os, uint64_t nSize)
{
    if (nSize < 253)
    {
        ser_writedata8(os, nSize);
    }
    else if (nSize <= std::numeric_limits<unsigned short>::max())
    {
        ser_writedata8(os, 253);
        ser_writedata16(os, nSize);
    }
    else if (nSize <= std::numeric_limits<unsigned int>::max())
    {
        ser_writedata8(os, 254);
        ser_writedata32(os, nSize);
    }
    else
    {
        ser_writedata8(os, 255);
        ser_writedata64(os, nSize);
    }
    return;
}

template<typename Stream>
uint64_t ReadCompactSize(Stream& is)
{
    uint8_t chSize = ser_readdata8(is);
    uint64_t nSizeRet = 0;
    if (chSize < 253)
    {
        nSizeRet = chSize;
    }
    else if (chSize == 253)
    {
        nSizeRet = ser_readdata16(is);
        if (nSizeRet < 253)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    }
    else if (chSize == 254)
    {
        nSizeRet = ser_readdata32(is);
        if (nSizeRet < 0x10000u)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    }
    else
    {
        nSizeRet = ser_readdata64(is);
        if (nSizeRet < 0x100000000ULL)
            throw std::ios_base::failure("non-canonical ReadCompactSize()");
    }
    if (nSizeRet > (uint64_t)MAX_SIZE)
        throw std::ios_base::failure("ReadCompactSize(): size too large");
    return nSizeRet;
}

```

针对位宽1,2,4,8的基础类型，`Serialize` 和 `Unserialize` 最终调用ser_writedata*, ser_readdata8* 完成实现。

```c++
template<typename Stream> inline void Serialize(Stream& s, char a    ) { ser_writedata8(s, a); } // TODO Get rid of bare char
template<typename Stream> inline void Serialize(Stream& s, int8_t a  ) { ser_writedata8(s, a); }
template<typename Stream> inline void Serialize(Stream& s, uint8_t a ) { ser_writedata8(s, a); }
template<typename Stream> inline void Serialize(Stream& s, int16_t a ) { ser_writedata16(s, a); }
template<typename Stream> inline void Serialize(Stream& s, uint16_t a) { ser_writedata16(s, a); }
template<typename Stream> inline void Serialize(Stream& s, int32_t a ) { ser_writedata32(s, a); }
template<typename Stream> inline void Serialize(Stream& s, uint32_t a) { ser_writedata32(s, a); }
template<typename Stream> inline void Serialize(Stream& s, int64_t a ) { ser_writedata64(s, a); }
template<typename Stream> inline void Serialize(Stream& s, uint64_t a) { ser_writedata64(s, a); }
template<typename Stream> inline void Serialize(Stream& s, float a   ) { ser_writedata32(s, ser_float_to_uint32(a)); }
template<typename Stream> inline void Serialize(Stream& s, double a  ) { ser_writedata64(s, ser_double_to_uint64(a)); }

template<typename Stream> inline void Unserialize(Stream& s, char& a    ) { a = ser_readdata8(s); } // TODO Get rid of bare char
template<typename Stream> inline void Unserialize(Stream& s, int8_t& a  ) { a = ser_readdata8(s); }
template<typename Stream> inline void Unserialize(Stream& s, uint8_t& a ) { a = ser_readdata8(s); }
template<typename Stream> inline void Unserialize(Stream& s, int16_t& a ) { a = ser_readdata16(s); }
template<typename Stream> inline void Unserialize(Stream& s, uint16_t& a) { a = ser_readdata16(s); }
template<typename Stream> inline void Unserialize(Stream& s, int32_t& a ) { a = ser_readdata32(s); }
template<typename Stream> inline void Unserialize(Stream& s, uint32_t& a) { a = ser_readdata32(s); }
template<typename Stream> inline void Unserialize(Stream& s, int64_t& a ) { a = ser_readdata64(s); }
template<typename Stream> inline void Unserialize(Stream& s, uint64_t& a) { a = ser_readdata64(s); }
template<typename Stream> inline void Unserialize(Stream& s, float& a   ) { a = ser_uint32_to_float(ser_readdata32(s)); }
template<typename Stream> inline void Unserialize(Stream& s, double& a  ) { a = ser_uint64_to_double(ser_readdata64(s)); }

template<typename Stream> inline void Serialize(Stream& s, bool a)    { char f=a; ser_writedata8(s, f); }
template<typename Stream> inline void Unserialize(Stream& s, bool& a) { char f=ser_readdata8(s); a=f; }

```
另外代码开始处的
```c++
struct deserialize_type {};
constexpr deserialize_type deserialize {};
```
作为tag 类型， tag 对象，  主要为多个实现签名有以下形式：

```c++
template <typename Stream> 
T::T(deserialize_type, Stream& s)
```
的反序列化构造器做分发， 目前主要是CTransaction, CMutableTransaction 类型:
```c++
template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}
    
    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

```


