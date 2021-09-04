import os, sys, getopt
import numpy as np

def main(argv):
	num_docs = 0
	n = 0
	client_ip = ""
	worker_id = -1
	try:
		opts, args = getopt.getopt(argv,"n:c:i:")
	except getopt.GetoptError:
		print("run_worker_latency.py -n <num_docs> -c <client_ip>  -i <worker_id>")
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-n':
			n = int(arg)
			num_docs = int(np.ceil(96151 * (n/5000000)))
		elif opt == '-c':
			client_ip = arg
		elif opt == '-i':
			worker_id = int(arg)

		else:
			print("unknown option!")
			sys.exit(2)

	if num_docs == 0:
		print("missing -n")
		sys.exit(2)
	if client_ip == "":
		print("missing -c")
		sys.exit(2)
	if worker_id < 0:
		print("missing -i")
		sys.exit(2)
	
	worker_cmd = 'bin/worker -n {} -w 38 -t 12 -c {} -i {} > result/step3_worker_n_{}.txt'
	worker_cmd = worker_cmd.format( num_docs, client_ip, worker_id, n )

	#print(worker_cmd)
	os.system(worker_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])