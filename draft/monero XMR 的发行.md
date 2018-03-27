# monero XMR 的发行

## 理论公式
BR_nom = max( 0.6, floor( (M - A) / 2<sup>19</sup> ) / 10<sup>12</sup> )
>* 注意：M, A 都是uint64 类型。
 
如果区块大于 60kB时，实际的块奖励将低于BR_nom。这种情况下，块奖励如下计算：
中位块: M100 = max(Median_100, 60kB);  // Median_100: 前100个块的中位块大小。
实际块奖励: BR_actual = BR_nom * (1 - (max(CurrentBlickSize, M100)/M100 -1)<sup>2</sup>)
>* 注意：CurrentBlickSize 不允许大于 Median_100 *2；但是允许CurrentBlickSize不管 Median_100多大，都可以在任何块大于60kB; 即此处总共有两种限制措施。

## 代码实现
```
 //获取块的奖励；median_size(in):中位数块大小；current_block_size(in):当前块大小；already_generated_coins(in):已经生成的币；
  // reward(out):当前块的奖励； version(in):块的版本
  bool get_block_reward(size_t median_size, size_t current_block_size, uint64_t already_generated_coins, uint64_t &reward, uint8_t version) {
    //难度目标必须是60的整数倍
    static_assert(DIFFICULTY_TARGET_V2%60==0&&DIFFICULTY_TARGET_V1%60==0,"difficulty targets must be a multiple of 60");
    //依据版本号选择不同的难度目标间隔
    const int target = version < 2 ? DIFFICULTY_TARGET_V1 : DIFFICULTY_TARGET_V2;
    const int target_minutes = target / 60;   //出块的间隔，以分钟标识
    //根据出块的时间间隔，获取偏移系数；1分钟间隔时，系数为向左偏移20位，发币更小；2分钟出块时，系数为向左偏移19位，每个块的发币量增大。
    const int emission_speed_factor = EMISSION_SPEED_FACTOR_PER_MINUTE - (target_minutes-1);

    //获取基础的块奖励； 主曲线
    uint64_t base_reward = (MONEY_SUPPLY - already_generated_coins) >> emission_speed_factor;
    //
    if (base_reward < FINAL_SUBSIDY_PER_MINUTE*target_minutes)
    {
      base_reward = FINAL_SUBSIDY_PER_MINUTE*target_minutes;
    }
    //获取块的另一个基础限制；不管前中位数的块大小为多少，下个块都可以大于这个字节。
    uint64_t full_reward_zone = get_min_block_size(version);

    //make it soft
    if (median_size < full_reward_zone) {
      median_size = full_reward_zone;
    }

    // 当前的块小于中位数的块时，获取全部的块奖励
    if (current_block_size <= median_size) {
      reward = base_reward;
      return true;
    }
    // 当前块大于中位数的两倍时，共识出错。直接报错。
    if(current_block_size > 2 * median_size) {
      MERROR("Block cumulative size is too big: " << current_block_size << ", expected less than " << 2 * median_size);
      return false;
    }

    //最大块的限制；为2**32 - 1
    assert(median_size < std::numeric_limits<uint32_t>::max());
    assert(current_block_size < std::numeric_limits<uint32_t>::max());

    uint64_t product_hi;
    // BUGFIX: 32-bit saturation bug (e.g. ARM7), the result was being
    // treated as 32-bit by default.
    // bug修复；32位的饱和bug，结果默认为32位。
    // 获取当前最大块剩余空间的大小
    uint64_t multiplicand = 2 * median_size - current_block_size;
    multiplicand *= current_block_size;
    uint64_t product_lo = mul128(base_reward, multiplicand, &product_hi);

    uint64_t reward_hi;
    uint64_t reward_lo;
    div128_32(product_hi, product_lo, static_cast<uint32_t>(median_size), &reward_hi, &reward_lo);
    div128_32(reward_hi, reward_lo, static_cast<uint32_t>(median_size), &reward_hi, &reward_lo);
    assert(0 == reward_hi);
    assert(reward_lo < base_reward);    //此时由于惩罚措施，导致实际块奖励 小于理论块奖励

    reward = reward_lo;
    return true;
  }
```



