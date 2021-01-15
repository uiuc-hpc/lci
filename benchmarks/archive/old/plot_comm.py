import matplotlib.pyplot as plt
import numpy as np

data1 = ''' --- 2.156727 1.614250 2.108636 --- 1.650853 1.741649
1.648297 --- 1.470455 1.522993 1.594724 --- 1.600991 1.725173 1.744840
--- 1.893003 1.755538 1.995876 --- 1.937071 1.859432 3.104449 ---
1.861123 2.127025 3.848253 --- 2.003477 1.961271 4.633959 --- 1.999292
1.987207 5.644303 --- 1.995949 2.131427 6.309057'''
data2 = '''
--- 2.181530 1.630887 1.612292 --- 1.543195 1.629800 1.532519 ---
1.808011 1.503759 1.517726 --- 1.625489 1.641847 1.667035 --- 1.792775
1.758948 1.898239 --- 1.836242 1.829811 3.197321 --- 1.863754 2.007097
4.004408 --- 1.898989 1.947854 4.772608 --- 2.017808 2.067514 5.476246
--- 2.081745 2.077985 7.561323'''
data3 ='''
--- 2.165888 1.620352 1.817464 --- 1.595384 1.643472 1.645782 ---
1.508559 1.575696 1.579508 --- 1.741481 1.613062 1.719271 --- 1.849704
1.802562 1.989223 --- 1.765813 1.838282 3.236189 --- 1.825205 1.865934
4.112970 --- 1.965247 2.031873 4.697794 --- 2.037376 2.824254 5.724007
--- 2.039910 2.208104 6.930074'''

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

c = np.mean(np.array([c1, c2, c3]), axis=0)
cq = np.mean(np.array([cq1, cq2, cq3]), axis=0)
abt = np.mean(np.array([abt1, abt2, cq3]), axis=0)

x = [i * 64 for i in range(4, 40, 4)]
x = [1 * 64] + x 
'''print (c)
print (cq)
print (abt)
print (x)'''
fig = plt.figure()
ax = fig.add_subplot(1,1,1)                                                      
plt.plot(x, c, label='hash-table')
plt.plot(x, cq, label='queue')
plt.plot(x, abt, label='argobot')
plt.legend(loc='best')

ax.set_ylabel('usec')
ax.set_xlabel('#threads')
ax.set_xticks(x)
ax.set_ylim(ymin=0)

plt.show()
