
s1 = """
00
11
333333
333333
333333
333333
333333
333333
333333
333333
333333
333333
00
"""

s2 = """
11
222222
222222
222222
222222
222222
222222
222222
222222
222222
222222"""

def tobin(p): return p.replace('\n', '').decode('hex')

import sys

sys.stdout.write(tobin(s1)*30000)
sys.stdout.write(tobin(s2)*20000)
