import os, sys
import numpy as np
#from matplotlib import pyplot as plt

if len(sys.argv) != 2:
	print("usage: python gen_graph.py <max_blocks>" )
	exit()
dir_name = 'results'
schemes = [ 'baseline','opt1','opt2']

os.system("touch single-machine-matrix-vector.dat")
dat_file = open("single-machine-matrix-vector.dat", "w")

i = 1
while i <= int(sys.argv[1]):
	output_line = str(i)
	for scheme in schemes:
		file_name = "{}/{}/r_{}.txt".format(dir_name,scheme,i)
		f = open(file_name, "r")
		lines = f.readlines()
		comp_time = int(lines[13])
		output_line += " " + str(comp_time)
	output_line += "\n"
	dat_file.write(output_line)
	i *= 2

dat_file.close()


cmd = "bash single-machine-matrix-vector.sh {}".format(i/2)
os.system(cmd)
os.system("rm -f single-machine-matrix-vector.dat")
os.system("rm -f plot.plt")
