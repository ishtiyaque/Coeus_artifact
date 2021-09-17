import os, sys
import numpy as np

scheme_names = ['Baseline', 'Coeus']
doc_sizes = ["300K", "1.2M", "5M"]
num_worker_arr = [32, 48, 64, 80, 96]

os.system("touch latency-vs-machines.dat")
dat_file = open("latency-vs-machines.dat", "w")

for num_worker in num_worker_arr:
	line = str(num_worker)
	for scheme in scheme_names:
		for doc in doc_sizes:
			dir_name = "result/{}/doc_{}/worker_{}/"
			dir_name = dir_name.format(scheme, doc,num_worker)
			client_file = open(dir_name+"client.txt")
			client_output = client_file.readlines()
			latency = int(client_output[3])
			line = line + "\t" + str(latency)
	dat_file.write(line+"\n")

dat_file.close()

cmd = "bash latency-vs-machines.sh "
os.system(cmd)
os.system("rm -f latency-vs-machines.dat")
os.system("rm -f plot.plt")