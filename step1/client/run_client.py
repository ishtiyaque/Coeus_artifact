import os, sys, getopt
import numpy as np

def main(argv):
	num_rows = 0
	master_ip = ""
	b = 0
	num_cols = 0
	client_ip = ""
	executable = ""
	factor = 0
	num_group = 0
	num_total_worker = 0
	try:
		opts, args = getopt.getopt(argv,"n:b:p:f:c:w:")
	except getopt.GetoptError:
		print("run_client.py -n <num_docs> -b <submatrix_width> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>")
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-n':
			num_rows = int(np.ceil(int(arg) / (8192*3)))
		elif opt == '-b':
			b = int(arg)
			if b < 13:
				executable = "split"
				factor = int((2 ** (13-int(arg))))
			else:
				executable = "merge"
				factor = int((2 ** (int(arg) - 13)))
		elif opt == '-p':
			master_ip = arg
		elif opt == '-f':
			num_cols = int(np.ceil(int(arg) / 8192))
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
	if executable == "":
		print("missing -b")
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

	
	
	if executable == "split":
		if num_total_worker%(num_cols * factor) != 0:
			print("Error: w must be a multiple of f / 2^b")
			sys.exit(2)
		num_group = int(num_total_worker / (num_cols * factor))
	elif executable == "merge":
		if num_total_worker%(num_cols / factor) != 0:
			print("Error: w must be a multiple of f / 2^b")
			sys.exit(2)
		num_group = int(num_total_worker / (num_cols / factor))

	num_rows = int((np.ceil(num_rows/num_group) * num_group))
	client_cmd = 'bin/{} -r {} -s {} -p {} -q {} -c {} -g {} > result/client_b_{}.txt '
	client_cmd = client_cmd.format(executable, num_rows, factor, master_ip, num_cols, client_ip, num_group, b)

	#print(client_cmd)
	os.system(client_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])