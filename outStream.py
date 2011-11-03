
s1 = """
00
11
444444
444444
444444
444444
444444
444444
444444
444444
444444
444444
"""

s2 = """
00
11
555555
555555
555555
555555
555555
555555
555555
555555
555555
555555
"""

def tobin(p): return p.replace('\n', '').decode('hex')

import sys

sys.stderr.write(repr((len(tobin(s1)), len(tobin(s2)))))

sys.stdout.write(tobin(s1)*5000)
sys.stdout.write(tobin(s2)*5000)
