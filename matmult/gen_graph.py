import sys
from matplotlib import pyplot as plt

if len(sys.argv) != 2:
	print("usage: python gen_graph.py <max_blocks>" )
	exit()
dir_name = 'results'
schemes = [ 'baseline','opt1','opt2']

x = []
i = 1
while i <= int(sys.argv[1]):
	x.append(i)
	i = i * 2
compute_time = []

for scheme_num in range(len(schemes)):
    compute_time.append([])
    for i in x:
        file_name = "{}/{}/r_{}.txt".format(dir_name,schemes[scheme_num],i)
        f = open(file_name, "r")
        lines = f.readlines()
        compute_time[scheme_num].append(float(lines[13])/1e6)

plt.yscale('log')
for i in range(len(compute_time)):
    plt.plot(x,compute_time[i], label=schemes[i])
plt.xlabel("Number of matrix blocks")
plt.ylabel("Server computation time (s)")
plt.xticks(x)
plt.title("Server cpu time to perform secure matrix-vector product")
plt.legend()

plt.savefig(dir_name+"/matmult.png")