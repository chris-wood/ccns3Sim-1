import sys
import math
import numpy as np
import matplotlib.pyplot as plt

fname = sys.argv[1]
fout = sys.argv[2]

observed = []
actual = []
with open(fname, "r") as fh:
    observed = map(lambda l: float(l), fh.readline().split(","))
    total = sum(observed)
    observed = map(lambda o : (float(o) / total) * 100.0, observed)
    actual = map(lambda a : float(a), fh.readline().split(","))

fig, ax = plt.subplots()

indices = range(1, len(observed) + 1)
num_values = len(observed)
rectA = plt.plot(indices, observed, color="r") #, markevery=int(num_values * 0.1)) # log=True
rectB = plt.plot(indices, actual, color="b") #, markevery=int(num_values * 0.1)) # log=True

legendLabels = ["Observed", "Actual"]
ax.legend(legendLabels, loc=1)
ax.set_ylabel('Frequency Percentage [%]')
ax.set_xlabel('Content Index')
# plt.ylim([0, 100.0])
plt.grid(True)
plt.subplots_adjust(bottom=0.15)

plt.savefig(fout + '.pdf')
plt.close()
