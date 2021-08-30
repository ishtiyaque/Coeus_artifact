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
	num_docs = 0
	try:
		opts, args = getopt.getopt(argv,"n:p:f:c:w:")
	except getopt.GetoptError:
		print("run_client.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>")
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
		
	num_group = num_total_worker
	num_rows = int((np.ceil(num_rows/num_group) * num_group))
	client_cmd = 'bin/client -r {} -s {} -p {} -q {} -c {} -g {} > result/client_n_{}.txt '
	client_cmd = client_cmd.format(num_rows, factor, master_ip, num_cols, client_ip, num_group, num_docs)

	#print(client_cmd)
	os.system(client_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])