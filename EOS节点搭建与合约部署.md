## EOS节点搭建与合约部署


本文介绍一下如何从零开始搭建一个EOS节点，以及如何完成EOS的合约部署

#### EOS节点搭建

查看服务器操作系统版本

```
cat /proc/version

Linux version 4.4.0-102-generic (buildd@lgw01-amd64-055) (gcc version 5.4.0 20160609 (Ubuntu 5.4.0-6ubuntu1~16.04.5) ) #125-Ubuntu SMP Tue Nov 21 15:15:11 UTC 2017
```

查看服务器磁盘使用

```
df -hl
Filesystem      Size  Used Avail Use% Mounted on
udev            3.9G     0  3.9G   0% /dev
tmpfs           799M  4.7M  794M   1% /run
/dev/vda1       345G  243G   85G  75% /
```

看这行：/dev/vda1  345G  243G   85G  75% ，还剩下85G可用空间。

一个EOS节点大约需要20G。

执行下面命令安装EOS节点

```
wget https://github.com/eosio/eos/releases/download/v1.4.4/eosio_1.4.4-1-ubuntu-16.04_amd64.deb
$ sudo apt install ./eosio_1.4.4-1-ubuntu-16.04_amd64.deb
```

执行完毕后nodeos将安装到/usr/bin下。

任意找个位置创建一个测试节点启动脚本

```
touch eos-test.sh
chmod 744 eos-test.sh 
```

输入如下内容

```
nodeos -e -p eosio -d /eosdata --plugin eosio::chain_api_plugin --plugin eosio::history_api_plugin --contracts-console --filter-on "*" --access-control-allow-origin "*"
```

解释下其中的几个参数：

-p eosio 区块产生者的账户，这里设定为系统账户eosio

-d /eosdata 区块链数据存放的目录

--plugin 开启指定插件

--contracts-console 开启控制台

启动节点

```
./eos-test.sh
```

出现下面的生产区块日志，就表示启动成功。

```
info  2018-11-21T06:14:48.000 thread-0  producer_plugin.cpp:1490      produce_block        ] Produced block 00000985e9a86774... #2437 @ 2018-11-21T06:14:48.000 signed by eosio [trxs: 0, lib: 2436, confirmed: 0]
info  2018-11-21T06:14:48.500 thread-0  producer_plugin.cpp:1490      produce_block        ] Produced block 0000098659626754... #2438 @ 2018-11-21T06:14:48.500 signed by eosio [trxs: 0, lib: 2437, confirmed: 0]
info  2018-11-21T06:14:49.000 thread-0  producer_plugin.cpp:1490      produce_block        ] Produced block 00000987ddfe113b... #2439 @ 2018-11-21T06:14:49.000 signed by eosio 
```

重新打开一个窗口

通过cleos与EOS节点和钱包交互

```
cleos get info
{
  "server_version": "59626f1e",
  "chain_id": "cf057bbfb72640471fd910bcb67639c22df9f92470936cddc1ade0e2f2e7dc4f",
  "head_block_num": 2779,
  "last_irreversible_block_num": 2778,
  "last_irreversible_block_id": "00000ada125d953aba94e94b2aa8a15b1773345766bab1863c6f6e977e2343d6",
  "head_block_id": "00000adb93c1f0fda3e8e21a180f98e49cb0840e71416476b402dd4837358f2f",
  "head_block_time": "2018-11-21T06:17:39.000",
  "head_block_producer": "eosio",
  "virtual_block_cpu_limit": 3214242,
  "virtual_block_net_limit": 16884231,
  "block_cpu_limit": 199900,
  "block_net_limit": 1048576,
  "server_version_string": "v1.4.4"
}
```

chain id唯一标识了一条EOS链，这里我启动的是条本地测试链。

可以通过指定主网peer节点来连接主网，比如我们连接主网节点http://mainnet.genereos.io:80

```
 cleos --url=http://mainnet.genereos.io:80 get info
{
  "server_version": "99d94446",
  "chain_id": "aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906",
  "head_block_num": 28055941,
  "last_irreversible_block_num": 28055607,
  "last_irreversible_block_id": "01ac183760816ad2812e4c442ca53bb3e41040841b577d81e2d08ade18850f62",
  "head_block_id": "01ac19850756798d85bdb52ab48d7ef4432113f991e6d795cc99e5bbe713c8b7",
  "head_block_time": "2018-11-21T06:27:05.000",
  "head_block_producer": "eos42freedom",
  "virtual_block_cpu_limit": 4920210,
  "virtual_block_net_limit": 1048576000,
  "block_cpu_limit": 176543,
  "block_net_limit": 1045016,
  "server_version_string": "mainnet-1.4.1"
}
```

主链的chain id是aca376f206b8fc25a6ed44dbdc66547c36c6c33e3a119ffbeaef943642f0e906

区块数28055941

可以和浏览器https://eospark.com/MainNet/数据对应起来。

可以通过添加--p2p-peer-address p2p.meet.one:9876  --genesis-json /genesis.json来启动nodeos，这样节点可以加入主网。

genesis.json文件从https://github.com/EOS-Mainnet/eos/blob/launch-rc-1.0.2/mainnet-genesis.json获取，放置在eosdata目录下。

全节点上网找或者https://docs.google.com/spreadsheets/u/1/d/1K_un5Vak3eDh_b4Wdh43sOersuhs0A76HMCfeQplDOY/htmlview?sle=true#gid=0里取一个。

#### 钱包生成

```
cleos wallet create --to-console -n test 
Creating wallet: test
Save password to use in the future to unlock this wallet.
Without password imported keys will not be retrievable.
"PW5HwmHNRee71Hke6ADGCSYFWrjjbssrzuNY95hmyQAGdCS57uPDR"
```

记住这个钱包密码，后续用于解锁钱包。

还需要把eosio账户的私钥导入到钱包中才能控制该账户，eosio账户的私钥在config.ini文件中。

系统默认的配置文件：~/.local/share/eosio/nodeos/config/config.ini

其中signature-provider = EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV=KEY:5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3

记录了默认区块生产者eosio的公私钥对，前面的是公钥，key后面的是私钥。

导入eosio账户的私钥到test钱包。

```
cleos wallet import -n test --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
imported private key for: EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
```

接下来再用钱包test创建一个个人账户test，下面的命令生成了一对新的公私钥对并导入到钱包里。

```
cleos wallet create_key -n test
Created new private key with a public key of: "EOS5mPPFpdAjMKWxYeYyWiyjNhghbUUyiqXw4yhhRnUApJQbnPACQ"
```

生成账户test，并关联上面生成的公钥，这里省略了一个active公钥，只填了一个owner公钥。

```
# cleos wallet keys
[
  "EOS5mPPFpdAjMKWxYeYyWiyjNhghbUUyiqXw4yhhRnUApJQbnPACQ",
  "EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV"
]
# cleos create account eosio test EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
executed transaction: f6d69af4b265fae07a27842d88be7e145b95977461b2b1eb79aef95dbbe17b82  200 bytes  332 us
#         eosio <= eosio::newaccount            {"creator":"eosio","name":"test","owner":{"threshold":1,"keys":[{"key":"EOS6MRyAjQq8ud7hVNYcfnVPJqcV...
warning: transaction executed locally, but may not be confirmed by the network yet         ] 
```

#### 生成合约

安装合约开发工具EOS.cdt

```
$ wget https://github.com/eosio/eosio.cdt/releases/download/v1.4.1/eosio.cdt-1.4.1.x86_64.deb
$ sudo apt install ./eosio.cdt-1.4.1.x86_64.deb
```

新建hello.cpp文件

```
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

编译

```
eosio-cpp -o hello.wasm hello.cpp --abigen
```

生成了hello.abi和hello.wasm

```
cat hello.abi 
{
    "____comment": "This file was generated with eosio-abigen. DO NOT EDIT Wed Nov 21 16:04:26 2018",
    "version": "eosio::abi/1.1",
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
    "variants": [],
    "abi_extensions": []
}
```

#### 部署合约

记住，这时我们有两个账户，一个默认账户eosio，一个个人账户test

如果钱包锁住的话先解锁钱包

```
# cleos wallet unlock -n test --password PW5HwmHNRee71Hke6ADGCSYFWrjjbssrzuNY95hmyQAGdCS57uPDR
Unlocked: test
```

然后用账户eosio发布合约

```
cleos set contract eosio /root/contract/ hello.wasm hello.abi -p eosio@active
Reading WASM from /root/contract/hello.wasm...
Publishing contract...
executed transaction: 4f1f10536dc09095366f8d18ecd2d4ef6b3755cbcf38865f6a8103f25730500c  1432 bytes  747 us
#         eosio <= eosio::setcode               "0000000000ea30550000bf110061736d0100000001390b60027f7e006000017f60027f7f017f60027f7f0060037f7f7f017...
#         eosio <= eosio::setabi                "0000000000ea3055320e656f73696f3a3a6162692f312e31000102686900010475736572046e616d6501000000000000806...
warning: transaction executed locally, but may not be confirmed by the network yet         ] 
```

这笔交易长这样

```
# cleos get transaction 4f1f10536dc09095366f8d18ecd2d4ef6b3755cbcf38865f6a8103f25730500c
{
  "id": "4f1f10536dc09095366f8d18ecd2d4ef6b3755cbcf38865f6a8103f25730500c",
  "trx": {
    "receipt": {
      "status": "executed",
      "cpu_usage_us": 747,
      "net_usage_words": 179,
      "trx": [
        1,{
          "signatures": [
            "SIG_K1_K4WjRqmiMAArWxmVmXwjQgrwt3JeqUJFGdehsYj99ZG1tA25x5mejKNGopLYGDZdWkjQkQKm2xoCruiTJifqcgeFgbJM6A"
          ],
          "compression": "zlib",
          "packed_context_free_data": "",
          "packed_trx": "78da85564d8b1c45187eeba3a77ba767b29d8322bb1eaadb081b309ae4b0c178c854309b84200121a78093ded94e323ddf1f89595976d6e041047f8007ef1e3c09426eae924340bcfa0b3c048d472107617ddeaa99dd450467d9eeaab7df8fe77deaa9ea6ebdf2d7ed8fde5ffdfeeb6b849fe40bfd7ef616ae8d37bff8ee2771cc40dffc79fef
```

很长，不在这里显示全部了。

#### 调用合约

```
# cleos push action eosio hi '["wormhole"]' -p test@active
executed transaction: aff91ac7a7d4f1467c610e4eb593071810f230ce8d53b5bd946fdc3753bcf59f  104 bytes  539 us
#         eosio <= eosio::hi                    {"user":"wormhole"}
>> Hello, wormhole
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```

这里的eosio为被调用合约代码的账户，hi为调用方法，'["wormhole"]'为参数，实际的参数用'[ ]'包起来，-p表示哪个账户发起调用，发起这个动作用的什么权限，权限分为active和owner。

可以看到合约已成功执行：>> Hello, wormhole

查看调用交易

```
cleos get transaction aff91ac7a7d4f1467c610e4eb593071810f230ce8d53b5bd946fdc3753bcf59f
{
  "id": "aff91ac7a7d4f1467c610e4eb593071810f230ce8d53b5bd946fdc3753bcf59f",
  "trx": {
    "receipt": {
      "status": "executed",
      "cpu_usage_us": 539,
      "net_usage_words": 13,
      "trx": [
        1,{
          "signatures": [
            "SIG_K1_K7Q9MV1kY3j4s4haTCwwhEsK4PmjDoZjfstCHcADdGRdvUvhiTDCx8PYKZbZ9GVrtDUsDpDrCX99GSseXqBUVgNrRstNPn"
          ],
          "compression": "none",
          "packed_context_free_data": "",
          "packed_trx": "8517f55ba248e9e70b2400000000010000000000ea3055000000000000806b01000000000090b1ca00000000a8ed3232080000002ad2262fe500"
        }
      ]
    },
    "trx": {
      "expiration": "2018-11-21T08:29:57",
      "ref_block_num": 18594,
      "ref_block_prefix": 604760041,
      "max_net_usage_words": 0,
      "max_cpu_usage_ms": 0,
      "delay_sec": 0,
      "context_free_actions": [],
      "actions": [{
          "account": "eosio",
          "name": "hi",
          "authorization": [{
              "actor": "test",
              "permission": "active"
            }
          ],
          "data": {
            "user": "wormhole"
          },
          "hex_data": "0000002ad2262fe5"
        }
      ],
      "transaction_extensions": [],
      "signatures": [
        "SIG_K1_K7Q9MV1kY3j4s4haTCwwhEsK4PmjDoZjfstCHcADdGRdvUvhiTDCx8PYKZbZ9GVrtDUsDpDrCX99GSseXqBUVgNrRstNPn"
      ],
      "context_free_data": []
    }
  },
  "block_time": "2018-11-21T08:29:27.500",
  "block_num": 18596,
  "last_irreversible_block": 19189,
  "traces": [{
      "receipt": {
        "receiver": "eosio",
        "act_digest": "9f7f4bf7be81ecc70381ef8e202070900f94fae225e5cd72feba0d4e3e41a0e2",
        "global_sequence": 18599,
        "recv_sequence": 18599,
        "auth_sequence": [[
            "test",
            1
          ]
        ],
        "code_sequence": 1,
        "abi_sequence": 1
      },
      "act": {
        "account": "eosio",
        "name": "hi",
        "authorization": [{
            "actor": "test",
            "permission": "active"
          }
        ],
        "data": {
          "user": "wormhole"
        },
        "hex_data": "0000002ad2262fe5"
      },
      "context_free": false,
      "elapsed": 120,
      "console": "Hello, wormhole",
      "trx_id": "aff91ac7a7d4f1467c610e4eb593071810f230ce8d53b5bd946fdc3753bcf59f",
      "block_num": 18596,
      "block_time": "2018-11-21T08:29:27.500",
      "producer_block_id": null,
      "account_ram_deltas": [],
      "except": null,
      "inline_traces": []
    }
  ]
}

```
