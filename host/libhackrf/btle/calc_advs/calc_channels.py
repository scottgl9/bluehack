# x*index == channel mod 37 -> x == inv(index)*channel mod 37
# x*1 == 7 mod 37 -> 1*7 = 7
# x*2 == 14 mod 37 -> 19*14 = 7
# x*6 == 5 mod 37
# 6*inv == 1 mod 37
# inverses_mod_37 = [1, 19]
"""
0
7
14
21
28
35
5
12
19
26
33
3
10
17
24
31
1
8
15
22
29
"""
import time

ch = 0
step = 7
inverses = []

def get_inverse_mod_37(x):
	for i in range(37):
		if i*x % 37 == 1:
			return i
	return -1
for x in range(1, 37):
	inv = get_inverse_mod_37(x)
	print("inv(%d) mod 37 = %d" % (x, inv))
	inverses.append("%d" % inv)

print("const uint8_t INVERSES_MOD37 = [%s];" % ", ".join(inverses))
