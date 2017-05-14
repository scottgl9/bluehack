import sys
import re

#advaddr = "78BDBCCFAB3E"
#advaddr = "CCB11A8AC6F5"
#advaddr = "78BDBCCFAB3E"
advaddr = "CCB11A8AC6F5"
#fname = "advs.txt"

HEXLEN = len("00000000344B989C")
#pattern_channel = re.compile(" (\d+) ADV_IND")
pattern_channel = re.compile(" (\d+) ADV_")

def samples_to_ms(samples):
	return samples / (2 * 8000)

def ms_to_sample(ms):
	return ms * 2 * 8000

sampleidx_last = 0

fh = open(sys.argv[1], "r")

for line in fh:
	#if not advaddr in line or "crc=0 " in line:
	if not advaddr in  line:
		#print("invalid line: %r" % line)
		continue
	sampleidx_str = line.split(" ")[0]

	if len(sampleidx_str) != HEXLEN:
		print("non hex: %r" % s)
		continue

	sampleidx = int(line[6:HEXLEN], 16)
	channel = pattern_channel.findall(line[HEXLEN: HEXLEN+20])[0]
	#sample_diff = (sampleidx - sampleidx_last) * 32
	sample_diff = (sampleidx - sampleidx_last)

	print("diff (sample, ms): %d %d (%s)" %
		(sample_diff,
		samples_to_ms(sample_diff),
		channel))
	sampleidx_last = sampleidx

fh.close()
