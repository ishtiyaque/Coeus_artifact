import os, sys
import numpy as np


if len(sys.argv) != 4:
	print("usage: gen_graph.py <doc_size1> <doc_size2> <doc_size3>")
	sys.exit(2)
	
doc_sizes = sys.argv[1:]
doc_sizes = sorted(doc_sizes)

files = os.listdir('raw/')
files = sorted(files)
step1_data = {}
cpu_time = {}
upload = {}
download = {}

os.system("touch latency-vs-rounds.dat")
dat_file = open("latency-vs-rounds.dat", "w")

for filename in files:
	if filename.startswith('Coeus'):
		arr = filename.split('_')
		doc_size = arr[5]
		client_file = open('raw/'+filename)
		client_output = client_file.readlines()
		latency = int(client_output[3])
		step1_data[doc_size] = latency
		cpu_time[doc_size] = float(client_output[9])
		upload[doc_size] = int(client_output[-3]) + int(client_output[-5])
		download[doc_size] = int(client_output[-1])

for doc_size in doc_sizes:
	line = doc_size
	client_file = open('raw/step2_client_n_'+ doc_size + '.txt')
	client_output = client_file.readlines()
	latency = int(client_output[1]) + int(client_output[9])
	line = line + "\t" + str(latency)
	cpu_time[doc_size] = cpu_time[doc_size] + float(client_output[7])
	download[doc_size] = download[doc_size] + int(client_output[-1])
	upload[doc_size] = upload[doc_size] + int(client_output[-3])
	
	client_file = open('raw/step3_client_n_'+ doc_size + '.txt')
	client_output = client_file.readlines()
	latency = int(client_output[1]) + int(client_output[9]) + int(client_output[11])
	line = line + "\t" + str(latency)
	cpu_time[doc_size] = cpu_time[doc_size] + float(client_output[7])
	download[doc_size] = download[doc_size] + int(client_output[-1])
	upload[doc_size] = upload[doc_size] + int(client_output[-3])
	download[doc_size] = download[doc_size] + int(client_output[-1])
	upload[doc_size] = upload[doc_size] + int(client_output[-3])

	download[doc_size] = download[doc_size]/ (1024*1024)
	upload[doc_size] = upload[doc_size]/ (1024*1024)
	line = line + "\t" + str(step1_data[doc_size]) + "\n"
	dat_file.write(line)

dat_file.close()


cmd = "bash latency-vs-rounds.sh "
#print(cmd)
os.system(cmd)
os.system("rm -f latency-vs-rounds.dat")
os.system("rm -f plot.plt")

os.system("touch client_cost.txt")
cost_file = open("client_cost.txt", "w")

cost_file.write("Client CPU time (sec):\n")
cost_file.write(str(cpu_time))
cost_file.write("\n\n")

cost_file.write("Total Upload (MiB):\n")
cost_file.write(str(upload))
cost_file.write("\n\n")

cost_file.write("Total Download (MiB):\n")
cost_file.write(str(download))
cost_file.write("\n\n")

cost_file.close();