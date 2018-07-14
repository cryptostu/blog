# Original block reward for miners was 50 BTC start_block_reward = 50 
# 210000 is around every 4 years with a 10 minute block interval 

reward_interval = 210000 

def max_money():
	# 50 BTC = 50 0000 0000 Satoshis 
	current_reward = 50 * 10**8 
	total = 0 
	i = 0;
	while current_reward > 0:
		print "loops", i, current_reward
		total += reward_interval * current_reward 
		current_reward /= 2 
		i = i + 1
	return total 


print "Total BTC to ever be created:"
print max_money(), "Satoshis"
