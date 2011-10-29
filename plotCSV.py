from pylab import *
import sys

data = [[], [], [], []]

for line in open(sys.argv[1]):
	for arr, dp in zip(data, line.split(',')):
		arr.append(int(dp))

for i, arr, in enumerate(data):
	subplot(4, 1, i+1)
	plot(arr)

figure(10)

ttotal = 5

x = array(xrange(0, 100000))/5


semilogx(x, abs(fft(data[1][1000:]))[0:100000])
semilogx(x, abs(fft(data[1][1000:]))[0:100000])
semilogx(x, abs(fft(data[2][1000:]))[0:100000])
semilogx(x, abs(fft(data[3][1000:]))[0:100000])

ylim(0, 6e7)
legend(['a_v', 'a_i', 'b_v', 'b_i'])

show()
	
