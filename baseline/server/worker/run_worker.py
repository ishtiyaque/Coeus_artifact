import os, sys, getopt
import numpy as np

def main(argv):
	num_rows = 0
	master_ip = ""
	num_cols = 0
	client_ip = ""
	factor = 0
	num_group = 0
	num_total_worker = 0
	num_thread_per_worker = 0
	worker_id = -1
	try:
		opts, args = getopt.getopt(argv,"n:p:f:c:w:t:i:")
	except getopt.GetoptError:
		print("run_worker.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>")
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-n':
			num_rows = int(np.ceil(int(arg) / (8192*3)))
			num_docs = int(arg)
		elif opt == '-p':
			master_ip = arg
		elif opt == '-f':
			num_cols = int(np.ceil(int(arg) / 8192))
			factor = num_cols
		elif opt == '-c':
			client_ip = arg
		elif opt == '-w':
			num_total_worker = int(arg)
		elif opt == '-t':
			num_thread_per_worker = int(arg)
		elif opt == '-i':
			worker_id = int(arg)

		else:
			print("unknown option!")
			sys.exit(2)

	if num_rows == 0:
		print("missing -n")
		sys.exit(2)
	if master_ip == "":
		print("missing -p")
		sys.exit(2)
	if num_cols == 0:
		print("missing -f")
		sys.exit(2)
	if client_ip == "":
		print("missing -c")
		sys.exit(2)
	if num_total_worker == 0:
		print("missing -w")
		sys.exit(2)
	if num_thread_per_worker == 0:
		print("missing -t")
		sys.exit(2)
	if worker_id < 0:
		print("missing -i")
		sys.exit(2)
	
	num_group = num_total_worker
	num_worker_per_group = int(num_total_worker / num_group)
	num_rows = int((np.ceil(num_rows/num_group) * num_group))
	
	worker_cmd = 'bin/worker -w {} -s {} -t {} -c {} -r {} -g {} -i {} > result/worker_{}_n_{}.txt'
	worker_cmd = worker_cmd.format(num_worker_per_group, factor, num_thread_per_worker,client_ip, int(num_rows/num_group), num_group, worker_id, worker_id, num_docs)

	#print(worker_cmd)
	os.system(worker_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])