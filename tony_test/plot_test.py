import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl

mpl.rcParams['legend.fontsize'] = 16
mpl.rcParams['figure.titlesize'] = 18
mpl.rcParams['axes.titlesize'] = 18
mpl.rcParams['lines.linewidth'] = 2.0
mpl.rcParams['axes.labelsize'] = 18
mpl.rcParams['xtick.labelsize'] = 14
mpl.rcParams['ytick.labelsize'] = 14

plt.rcParams['text.usetex'] = False
plt.rcParams['font.family'] = 'cursive'

x = np.array(list(range(10)))
y = x * x + 2

fig, ax = plt.subplots(figsize=(8, 6))  # Create a new figure with a specific size
ax.plot(x, y)
ax.set_ylabel('y axis')
ax.set_xlabel('x axis')

# Save the plot as a PDF file
plt.savefig('test/plot.pdf', bbox_inches='tight')