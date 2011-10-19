from pylab import *

data = [[], [], [], []]

for line in open('inData.csv'):
	for arr, dp in zip(data, line.split(',')):
		arr.append(int(dp))

for i, arr, in enumerate(data):
	subplot(4, 1, i+1)
	plot(arr)

show()
	