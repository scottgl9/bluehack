import sys

#advaddr = "CCB11A8AC6F5"
advaddr = "78BDBCCFAB3E"
#fname = "advs.txt"

HEXLEN = len("00000000344B989C")
fh = open(sys.argv[1], "r")

sample_ch37 = []
sample_ch38 = []
sample_ch39 = []

for line in fh:
	#if not advaddr in line or "crc=0 " in line:
	if not advaddr in  line:
		print("nonvalid line: %r" % line)
		continue
	s = line.split(" ")[0]
	if len(s) != HEXLEN:
		print("non hex: %r" % s)
		continue
	if "37 ADV_IND" in line:
		l = sample_ch37
	elif "38 ADV_IND" in line:
		l = sample_ch38
	elif "39 ADV_IND" in line:
		l = sample_ch39
	else:
		print("nonvalid line: %r" % line)
		continue

	l.append(int(line[:HEXLEN], 16))

fh.close()

def min_diff_ms(numbers):
	return min([(numbers[idx+1] - numbers[idx])/(2*8000) for idx in range(len(numbers)-1)])

print(sample_ch37)
print(min_diff_ms(sample_ch37))
print(sample_ch38)
print(min_diff_ms(sample_ch38))
print(sample_ch39)
print(min_diff_ms(sample_ch39))

def samples_to_ms(samples):
	return samples / (2 * 8000)

def ms_to_sample(ms):
	return ms * 2 * 8000

def show_diffs(sample1, sample2, sample3, std_diff_samples):
	while True:
		if sample2 - std_diff_samples <= sample1:
			break
		sample2 -= std_diff_samples

	while True:
		if sample3 - std_diff_samples <= sample1:
			break
		sample3 -= std_diff_samples

	diff_ms_12 = samples_to_ms(sample2 - sample1)
	diff_ms_13 = samples_to_ms(sample3 - sample1)

	print("diff ms 1->2: %d, 1->3: %d" % (diff_ms_12, diff_ms_13))

idx = 0
sample_37 = sample_ch37[idx]
sample_38 = sample_ch38[idx]
sample_39 = sample_ch39[idx]

print("Taking sample for channel 37/38/39:\n%d\n%d\n%d" % (sample_37, sample_38, sample_39))

diff_ms = 92
#diff_ms = 100
std_diff_samples = ms_to_sample(diff_ms)

if sample_37 < sample_38 and sample_38 < sample_39:
	show_diffs(sample_37, sample_38, sample_39, std_diff_samples)
elif sample_39 < sample_38 and sample_38 < sample_37:
	show_diffs(sample_39, sample_38, sample_37, std_diff_samples)
elif sample_38 < sample_37 and sample_37 < sample_39:
	show_diffs(sample_38, sample_37, sample_39, std_diff_samples)
