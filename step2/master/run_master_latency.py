import os, sys, getopt
import numpy as np

def main(argv):
	num_docs = 0
	master_ip = ""
	try:
		opts, args = getopt.getopt(argv,"n:p:")
	except getopt.GetoptError:
		print("run_master_latency.py -p <master_ip> -n num_docs")
		sys.exit(2)
	for opt, arg in opts:
		if opt == '-p':
			master_ip = arg
		elif opt == '-n':
			num_docs = int(arg)
		else:
			print("unknown option!")
			sys.exit(2)

	if master_ip == "":
		print("missing -p")
		sys.exit(2)
			


	master_cmd = 'bin/master -k 16 -p {} > result/step2_master_n_{}.txt '
	master_cmd = master_cmd.format( master_ip, num_docs)

	#print(master_cmd)
	os.system(master_cmd)

	
if __name__ == "__main__":
	os.system("mkdir -p result")
	main(sys.argv[1:])