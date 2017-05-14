import sys

#a, b, c = int(sys.argv[1], 16), int(sys.argv[2], 16), int(sys.argv[3], 16)

def min_diff_ms(numbers):
	return min([(numbers[idx+1] - numbers[idx])/(2*8000) for idx in range(len(numbers)-1)])

def samples_to_ms(samples):
	return samples / (2 * 8000)

def ms_to_sample(ms):
	return ms * 2 * 8000

for x in range(1, len(sys.argv) - 1):
	diff = int(sys.argv[x+1], 16) - int(sys.argv[x], 16)
	print("%d (%d ms)" % (diff, samples_to_ms(diff)))

"""
> 37 38 39
samples: 343980778 345336310 346707520
diffs: 1355532 1371210

samples: 502799682 504312134 505737004
diffs: 1512452 1424870

samples: 570892882 572474520 574134942
diffs: 1581638 1660422


> 37 39 38
samples: 1870473998 1871979828 1873310310
diffs: 1505830 1330482

samples: 2104140004 2105766548 2108750150
diffs: 1626544 2983602


> Small diff
000000009A93BADE [ 38 ADV_IND         crc=0 sig=    17/128 len=46 t=  24120 AdvA=CCB11A8AC6F5 content=%uB@O$W
000000009AA9E5B4 [ 37 ADV_IND         crc=0 sig=    29/128 len=46 t=  92764 AdvA=CCB11A8AC6F5 content=%uB@O$(W
000000009AAA4DA6 [ 39 ADV_IND         crc=0 sig=    14/128 len=46 t=   1739 AdvA=CCB11A8AC6F5 content=%[uB`O1$P0W
000000009AC162E8 [ 38 ADV_IND         crc=1 sig=    17/128 len=46 t=  69948 AdvA=CCB11A8AC6F5 content=%uB@O$W

00000000E28D5BC2 [ 37 ADV_IND         crc=0 sig=    15/128 len=46 t= 405573 AdvA=CCB11A8AC6F5 content=%uBHO$W
00000000E28DC3B0 [ 39 ADV_IND         crc=0 sig=    17/128 len=46 t=   1890 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000E2A36C6A [ 38 ADV_IND         crc=1 sig=    33/128 len=46 t=  99274 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000E2BB4DE6 [ 37 ADV_IND         crc=0 sig=    43/128 len=46 t= 102059 AdvA=CCB11A8AC2F5 content=%u@Br2Bssaw9

0000000005B72328 [ 39 ADV_NONCONN_IND crc=1 sig=    58/128 len=43 t=  70621 AdvA=78BDBCCFAB3E content=">xuBx>z:?
0000000005BDA014 [ 39 ADV_IND         crc=1 sig=    43/128 len=46 t=  26926 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000005D5BBAE [ 39 ADV_IND         crc=0 sig=    43/128 len=46 t=  98326 AdvA=CCB11A8AC6F5 content=%uB`o+$W
000000000603C0A4 [ 39 ADV_IND         crc=1 sig=    43/128 len=46 t= 194997 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000061A5526 [ 39 ADV_IND         crc=1 sig=    43/128 len=46 t=  93757 AdvA=CCB11A8AC6F5 content=%uB@O$W
000000000624ED90 [ 39 ADV_NONCONN_IND crc=1 sig=    58/128 len=43 t=  43121 AdvA=78BDBCCFAB3E content=">xuBx>z:?


0000000024C8A012 [ 38 ADV_IND         crc=1 sig=    52/128 len=46 t=1295146762 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000250A4758 [ 38 ADV_IND         crc=1 sig=    52/128 len=46 t= 246328 AdvA=CCB11A8AC6F5 content=%uB@O$W
000000002522ABB4 [ 38 ADV_IND         crc=1 sig=    52/128 len=46 t=  30849 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000253A606A [ 38 ADV_IND         crc=1 sig=    52/128 len=46 t=   9013 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000025AC3B02 [ 38 ADV_IND         crc=0 sig=    45/128 len=46 t=  12633 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000025C39E40 [ 38 ADV_IND         crc=1 sig=    45/128 len=46 t= 102811 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000025ED23CC [ 38 ADV_IND         crc=1 sig=    46/128 len=46 t=  33493 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000026021D50 [ 38 ADV_IND         crc=1 sig=    45/128 len=46 t= 101381 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000262F6F7C [ 38 ADV_IND         crc=1 sig=    48/128 len=46 t=  28491 AdvA=CCB11A8AC6F5 content=%uB@O$W
00000000265E1B86 [ 38 ADV_IND         crc=1 sig=    44/128 len=46 t=  22032 AdvA=CCB11A8AC6F5 content=%uB@O$W
000000002676EF68 [ 38 ADV_IND         crc=1 sig=    43/128 len=46 t= 103593 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000026A2F212 [ 38 ADV_IND         crc=1 sig=    39/128 len=46 t=  13806 AdvA=CCB11A8AC6F5 content=%uB@O$W
0000000026B94F3A [ 38 ADV_IND         crc=0 sig=    41/128 len=46 t=  98800 AdvA=CCB11A8AC6F5 content=%uB@O:$W

"""
