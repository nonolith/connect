from pylab import *
data = open('inData.bin').read()

i = 64*256

cnt = []
inbuf = []
outbuf = []
lastpkt = ord(data[i])-1
lost = 0

while i<len(data):
	p = ord(data[i])
	diffpkt = p-(lastpkt+1)%256
	lost += abs(diffpkt)
	cnt.append(diffpkt)
	lastpkt = p
	inbuf.append(ord(data[i+2]))
	outbuf.append(ord(data[i+3]))
	i+=64

print "Lost %s packets"%(lost)
plot(cnt)
plot(inbuf)
plot(outbuf)
legend(['cnt', 'inbuf', 'outbuf'])
show()
