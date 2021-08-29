import os, sys
import numpy as np

if len(sys.argv) != 4:
	print("usage: python gen_graph.py <num_worker> <min_b> <max_b>" )
	exit()

num_total_worker = int(sys.argv[1])
min_b = int(sys.argv[2])
max_b = int(sys.argv[3])
os.system("touch multi-machine-matrix-vector.dat")
dat_file = open("multi-machine-matrix-vector.dat", "w")
max_latency = 0
for b in range(min_b, max_b+1):
	width = (2**b)
	client_file = open("raw/client_b_{}.txt".format(b), "r")
	client_output = client_file.readlines()
	master_file = open("raw/master_b_{}.txt".format(b),"r")
	master_output = master_file.readlines()
	worker_output = []
	for i in range(num_total_worker):
		w_out = open("raw/worker_{}_b_{}.txt".format(i, b), "r")
		worker_output.append(w_out.readlines())
			
	latency = int(client_output[3])
	if latency > max_latency:
		max_latency = latency

	arr = master_output[3].split("\t")
	temp_arr = np.zeros(num_total_worker)
	temp_arr2 = np.zeros(num_total_worker)
	for i in range(num_total_worker):
		temp_arr[i] = int(arr[i])
	earliest_master_send_time = int(np.min(temp_arr))
	for i in range(num_total_worker):
		temp_arr[i] = int(worker_output[i][1])
	last_worker_rcv_time = int(np.max(temp_arr))
	dist_time = last_worker_rcv_time - earliest_master_send_time

	for i in range(num_total_worker):
		temp_arr[i] = int(worker_output[i][5])
	comp_time = int(np.max(temp_arr))

	for i in range(num_total_worker):
		temp_arr[i] = int(worker_output[i][3].split("\t")[1]) 
	last_processing_end = int(np.max(temp_arr))

	for i in range(num_total_worker):
		temp_arr[i] = int(worker_output[i][13]) 
	last_resonse_send = int(np.max(temp_arr))
	agg_time = last_resonse_send - last_processing_end

	output_line = "{}\t{}\t{}\t{}\t{}\n"
	output_line = output_line.format(width, dist_time, comp_time, agg_time, latency)
	dat_file.write(output_line)

dat_file.close()

cmd = "bash multi-machine-matrix-vector.sh {} {} {}".format(2**min_b, 2**max_b, np.ceil(max_latency / 1000000))
os.system(cmd)
os.system("rm -f multi-machine-matrix-vector.dat")
os.system("rm -f plot.plt")