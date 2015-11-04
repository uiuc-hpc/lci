import matplotlib.pyplot as plt

# yield data
data = '''
2835.697945 2318.322055 2577.010000
3015.191116 2335.679884 2675.435500
2439.930146 2025.052354 2232.491250
2159.104653 1446.259347 1802.682000
2452.718336 1781.116164 2116.917250
1921.600803 696.312447  1308.956625
2325.422715 1326.883160 1826.152938
1799.669527 1056.705973 1428.187750
2123.508048 1409.429890 1766.468969
1656.517708 744.130855  1200.324281
1843.317150 1054.652038 1448.984594
1709.214109 714.724797  1211.969453'''

data = [float(x) for x in data.split()]
low = []
high = []
mean = []

for (i, x) in zip(range(len(data)), data):
    if i % 3 == 0:
        high.append(x)
    if i % 3 == 1:
        low.append(x)
    if i % 3 == 2:
        mean.append(x)

low1 = []
low2 = []
high1 = []
high2 = []
mean1 = []
mean2 = []

for (i, x) in zip(range(len(low)), data):
    if i % 2 == 0:
        low1.append(low[i])
        high1.append(high[i])
        mean1.append(mean[i])
    else:
        low2.append(low[i])
        high2.append(high[i])
        mean2.append(mean[i])

x = [2 ** i for i in range(6)]

plt.plot(x, mean1)
plt.plot(x, mean2)

plt.show()
