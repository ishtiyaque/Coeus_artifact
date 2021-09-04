import os, sys, getopt
import numpy as np

def main(argv):
	n = 0
	num_docs = 0
	master_ip = ""
	client_ip = ""
	try:
		opts, args = getopt.getopt(argv,"n:p:c:")
	except getopt.GetoptError:
		print("run_client.py -n <num_docs> -p <master_ip>  -c <client_ip> ")
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-n':
			n = int(arg)
			num_docs = int(np.ceil(96151 * (n/5000000)))
		elif opt == '-p':
			master_ip = arg
		elif opt == '-c':
			client_ip = arg
		else:
			print("unknown option!")
			sys.exit(2)

	if num_docs == 0:
		print("missing -n")
		sys.exit(2)
	if master_ip == "":
		print("missing -p")
		sys.exit(2)
	if client_ip == "":
		print("missing -c")
		sys.exit(2)

	client_cmd = 'bin/client -n {} -w 38 -p {} -c {}  > result/step3_client_n_{}.txt '
	client_cmd = client_cmd.format(num_docs, master_ip, client_ip, n)

	#print(client_cmd)
	os.system(client_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])