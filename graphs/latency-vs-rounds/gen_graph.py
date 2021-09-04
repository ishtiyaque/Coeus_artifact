import os, sys
import numpy as np


if len(sys.argv) != 4:
	print("usage: gen_graph.py <doc_size1> <doc_size2> <doc_size3>")
	sys.exit(2)
	
doc_sizes = sys.argv[1:]
doc_sizes = sorted(doc_sizes)

files = os.listdir('raw/')
step1_data = {}

os.system("touch latency-vs-rounds.dat")
dat_file = open("latency-vs-rounds.dat", "w")

for filename in files:
	if filename.startswith('Coeus'):
		arr = filename.split('_')
		client_file = open('raw/'+filename)
		client_output = client_file.readlines()
		latency = int(client_output[3])
		step1_data[arr[5]] = latency

for doc_size in doc_sizes:
	line = doc_size
	client_file = open('raw/step2_client_n_'+ doc_size + '.txt')
	client_output = client_file.readlines()
	latency = int(client_output[1]) + int(client_output[9])
	line = line + "\t" + str(latency)
	
	client_file = open('raw/step3_client_n_'+ doc_size + '.txt')
	client_output = client_file.readlines()
	latency = int(client_output[1]) + int(client_output[9]) + int(client_output[11])
	line = line + "\t" + str(latency)
	
	line = line + "\t" + str(step1_data[doc_size]) + "\n"
	dat_file.write(line)

dat_file.close()


cmd = "bash latency-vs-rounds.sh "
#print(cmd)
os.system(cmd)
os.system("rm -f multi-machine-matrix-vector.dat")
os.system("rm -f plot.plt")