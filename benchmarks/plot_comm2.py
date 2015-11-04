import matplotlib.pyplot as plt
import numpy as np

data1 = ''' --- 2.156727 1.614250 2.108636 --- 1.650853 1.741649
1.648297 --- 1.470455 1.522993 1.594724 --- 1.600991 1.725173 1.744840
--- 1.893003 1.755538 1.995876 --- 1.937071 1.859432 3.104449 ---
1.861123 2.127025 3.848253 --- 2.003477 1.961271 4.633959 --- 1.999292
1.987207 5.644303 --- 1.995949 2.131427 6.309057
'''

data2 = ''' --- 2.618134 2.321836 2.009340 --- 2.045956 2.653926 5.173820 ---
2.202363 4.730412 6.139855 --- 2.424521 6.027249 6.959325 --- 2.053347
9.832497 9.048761 --- 2.572897 9.716418 9.410102 --- 2.637566 14.397223
10.133870 --- 2.200092 15.791309 10.664514 --- 2.188679 18.455151
11.554782 --- 2.946506 26.400428 13.267509'''

data3 = ''' --- 6.535571 4.384857 3.208545 --- 6.267135 5.196358 7.972263 ---
5.969297 10.102079 9.247632 --- 7.181597 13.033643 10.356529 ---
6.873629 17.858045 12.934601 --- 3.875604 19.931283 13.299187 ---
5.030700 22.493612 14.303669 --- 5.946853 23.731317 14.944572 ---
6.974124 27.751637 16.609467 --- 7.739094 33.845267 21.359223'''

def parse(data):
    d = [x.strip() for x in data.split('---') if x.strip() != '']
    c = []
    cq = []
    abt = []
    for data in d:
        (p1, p2, p3) = data.split()
        c.append(float(p1))
        cq.append(float(p2))
        abt.append(float(p3))
    return c, cq, abt

(c1, cq1, abt1) = parse(data1)
(c2, cq2, abt2) = parse(data2)
(c3, cq3, abt3) = parse(data3)

x = [i * 64 for i in range(4, 40, 4)]
x = [1 * 64] + x 

fig = plt.figure()
ax = fig.add_subplot(1,1,1)                                                      
#plt.plot(x, c1, 'r', label='hash-table(0)')
#plt.plot(x, cq1, 'b', label='queue(0)')
#plt.plot(x, abt1, 'g', label='argobot(0)')

plt.plot(x, c2, 'r', label='hash-table(8K)')
plt.plot(x, cq2, 'b', label='queue(8K)')
plt.plot(x, abt2, 'g', label='argobot(8K)')

plt.plot(x, c3, 'r--', label='hash-table(16K)')
plt.plot(x, cq3, 'b--', label='queue(16K)')
plt.plot(x, abt3, 'g--', label='argobot(16K)')

plt.legend(loc='best')

ax.set_ylabel('usec')
ax.set_xlabel('#threads')
ax.set_xticks(x)
ax.set_ylim(ymin=0)

plt.show()
