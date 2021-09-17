import os, sys
import numpy as np

os.system("touch latency-vs-rounds.dat")
dat_file = open("latency-vs-rounds.dat", "w")


doc_size = ["300K", "1.2M", "5M"]

cpu_time = {"300K":0, "1.2M":0, "5M":0}
upload = {"300K":0, "1.2M":0, "5M":0}
download = {"300K":0, "1.2M":0, "5M":0}

for doc in doc_size:
	line = doc
	
	step2_filename = "./result/doc_{}/step2/client.txt".format(doc)
	step2_file = open(step2_filename)
	client_output = step2_file.readlines()
	cpu_time[doc] = cpu_time[doc] + float(client_output[7])
	upload[doc] = upload[doc] + int(client_output[-2].split(" ")[2])
	download[doc] = download[doc] + int(client_output[-1].split(" ")[2])
	latency = int(client_output[1]) + int(client_output[9])
	line = line + "\t" + str(latency)

	step3_filename = "./result/doc_{}/step3/client.txt".format(doc)
	step3_file = open(step3_filename)
	client_output = step3_file.readlines()
	cpu_time[doc] = cpu_time[doc] + float(client_output[7])
	upload[doc] = upload[doc] + int(client_output[-2].split(" ")[2])
	download[doc] = download[doc] + int(client_output[-1].split(" ")[2])
	latency = int(client_output[1]) + int(client_output[9]) + int(client_output[11])
	line = line + "\t" + str(latency)

	step1_filename = "./result/doc_{}/step1/client.txt".format(doc)	
	step1_file = open(step1_filename)
	client_output = step1_file.readlines()
	cpu_time[doc] = cpu_time[doc] + float(client_output[9])
	upload[doc] = upload[doc] + int(client_output[15]) + int(client_output[17])
	download[doc] = download[doc] + int(client_output[-1])
	latency = int(client_output[3])
	line = line + "\t" + str(latency)
	dat_file.write(line+"\n")
	
	upload[doc] = float(upload[doc] / (1024*1024))
	download[doc] = float(download[doc] / (1024*1024))
	
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