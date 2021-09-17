import boto3
import numpy as np
import paramiko
import time
import os
import datetime
import aws_utils as util

MASTER_NAME = "Coeus_master_96"
CLIENT_NAME = "Coeus_client"
WORKER_NAME = "Coeus_worker"

PEM_FILE_NAME = "Coeus_artifact.pem"

POLY_MODULUS_DEGREE = 8192
num_total_worker = 64
num_thread_per_worker = 48
num_response = 128
num_query = 8
num_iter = 1

split_factor_arr = [8,4,2]
scale_factor_arr = [1,2,4,8]

server_session = boto3.Session(region_name="us-east-2")
server_ec2 = server_session.resource('ec2')

client_session = boto3.Session(region_name="us-east-2")
client_ec2 = client_session.resource('ec2')

worker_public_ip=[]
worker_private_ip=[]


worker_instances = server_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [WORKER_NAME]}])

for instance in worker_instances:
    worker_public_ip.append(instance.public_ip_address)
    worker_private_ip.append(instance.private_ip_address)


master_instances = server_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [MASTER_NAME]}])

for instance in master_instances:
    master_public_ip = instance.public_ip_address
    master_private_ip = instance.private_ip_address

client_public_ip = ""
client_private_ip = ""
client_instances = client_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [CLIENT_NAME]}])

for instance in client_instances:
    client_public_ip = instance.public_ip_address
    client_private_ip = instance.private_ip_address
	
server_key = paramiko.RSAKey.from_private_key_file('../'+PEM_FILE_NAME)
client_key = paramiko.RSAKey.from_private_key_file('../'+PEM_FILE_NAME)

print("Establishing connections...\n")

worker_connections = []
for i in range(len(worker_public_ip)):
    worker_connections.append(paramiko.SSHClient())
    worker_connections[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
    worker_connections[i].connect(hostname=worker_public_ip[i], username="ubuntu", pkey=server_key)
print("Connected to workers")	

master_connection = paramiko.SSHClient()
master_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
master_connection.connect(hostname=master_public_ip, username="ubuntu", pkey=server_key)
print("Connected to master")	

client_connection = paramiko.SSHClient()
client_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client_connection.connect(hostname=client_public_ip, username="ubuntu", pkey=client_key)
print("Connected to client")	

util.update_buffer_size(master_connection, client_connection, worker_connections)
util.synch_time(master_connection, client_connection, worker_connections)
print("\nSynchronized clock for all the servers.")

f = open("./worker_ip.txt", "w")
for ip in worker_private_ip:
    f.write(ip)
    f.write("\n")
f.close()
print("\nCreated local file worker_ip.txt")

master_ftp=master_connection.open_sftp()

worker_ftp = []
for i in range(len(worker_public_ip)):
    worker_ftp.append(worker_connections[i].open_sftp())
	
master_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step1/common/worker_ip.txt")
for w_ftp in worker_ftp:
    w_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step1/common/worker_ip.txt")
print("Copied worker_ip.txt to master and workers.")

print("\nStarting experiments..."+ "\t" + datetime.datetime.now().strftime("%H:%M:%S") + "\n")

for split_factor in split_factor_arr:
	b = int(np.log2(POLY_MODULUS_DEGREE / split_factor))
	num_group = int(num_total_worker / (num_query * split_factor))
	num_worker_per_group = int(num_total_worker / num_group)
	dir_name = "result/b_{}/".format(b)
	os.system('mkdir -p '+dir_name)
	client_filename = dir_name+"client.txt"
	master_filename = dir_name+"master.txt"
	
	if (os.path.exists(client_filename)) and (os.path.exists(master_filename)):
		skip = True
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) != 20:
			skip = False
		client_file.close()

		master_file = open(master_filename)
		master_output = master_file.readlines()
		if len(master_output) != 8:
			skip = False
		master_file.close()
		
		if (skip == True):
			for i in range(num_total_worker):
				worker_filename = dir_name+"worker_{}.txt".format(i)
				if (os.path.exists(worker_filename)):
					worker_file = open(worker_filename)
					worker_output = worker_file.readlines()
					if len(worker_output) != 18:
						skip = False
					worker_file.close()
				else:
					skip = False
					
		if (skip == True):
			print("finished width = 2^"+str(b)+ "\tskipped" )
			continue
	
	worker_cmd = 'bin/split -w {} -s {} -t {} -c {} -r {} -g {} -i '
	worker_cmd = worker_cmd.format(num_worker_per_group, split_factor, num_thread_per_worker,client_private_ip, int(num_response/num_group), num_group)
	master_cmd = 'bin/split -r {} -s {} -p {} -w {} -g {} '
	master_cmd = master_cmd.format(num_response, split_factor, master_private_ip, num_worker_per_group, num_group)
	client_cmd = 'bin/split -r {} -s {} -p {} -q {} -c {} -g {}'
	client_cmd = client_cmd.format(num_response, split_factor, master_private_ip, num_query, client_private_ip, num_group)

	win = []
	wout = []
	werr = []
	mtin, mtout, mterr=master_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/server/master;'+master_cmd)
	time.sleep(3)
	clin, clout, clerr=client_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/client;'+client_cmd)
	time.sleep(3)
	for i in range(num_total_worker):
		_win, _wout, _werr=worker_connections[i].exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/server/worker;'+worker_cmd+str(i))
		win.append(_win)
		wout.append(_wout)
		werr.append(_werr)
	time.sleep(30)
	client_output = clout.readlines()
	master_output = mtout.readlines()
	worker_output = []
	for _wout in wout:
		worker_output.append(_wout.readlines())

	client_file = open(client_filename, "w")
	for line in client_output:
		client_file.write(line)
	client_file.close()

	master_file = open(master_filename, "w")
	for line in master_output:
		master_file.write(line)
	master_file.close()

	for i in range(len(worker_output)):
		worker_file = open(dir_name+"worker_"+str(i)+".txt", "w")
		for line in worker_output[i]:
			worker_file.write(line)
		worker_file.close()

	print("finished width = 2^"+str(b)+ "\t" + datetime.datetime.now().strftime("%H:%M:%S"))


for scale_factor in scale_factor_arr:
	b = int(np.log2(POLY_MODULUS_DEGREE * scale_factor))
	num_group = int(num_total_worker / (num_query / scale_factor))
	num_worker_per_group = int(num_total_worker / num_group)
	dir_name = "result/b_{}/".format(b)
	os.system('mkdir -p '+dir_name)

	client_filename = dir_name+"client.txt"
	master_filename = dir_name+"master.txt"
	
	if (os.path.exists(client_filename)) and (os.path.exists(master_filename)):
		skip = True
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) != 16:
			skip = False
		client_file.close()

		master_file = open(master_filename)
		master_output = master_file.readlines()
		if len(master_output) != 8:
			skip = False
		master_file.close()
		
		if (skip == True):
			for i in range(num_total_worker):
				worker_filename = dir_name+"worker_{}.txt".format(i)
				if (os.path.exists(worker_filename)):
					worker_file = open(worker_filename)
					worker_output = worker_file.readlines()
					if len(worker_output) != 18:
						skip = False
					worker_file.close()
				else:
					skip = False
					
		if (skip == True):
			print("finished width = 2^"+str(b)+ "\tskipped" )
			continue

	worker_cmd = 'bin/merge -w {} -s {} -t {} -c {} -r {} -g {} -i '
	worker_cmd = worker_cmd.format(num_worker_per_group, scale_factor, num_thread_per_worker,client_private_ip, int(num_response/num_group), num_group)
	master_cmd = 'bin/merge -r {} -s {} -p {} -w {} -g {} '
	master_cmd = master_cmd.format(num_response, scale_factor, master_private_ip, num_worker_per_group, num_group)
	client_cmd = 'bin/merge -r {} -s {} -p {} -q {} -c {} -g {}'
	client_cmd = client_cmd.format(num_response, scale_factor, master_private_ip, num_query, client_private_ip, num_group)
	
	win = []
	wout = []
	werr = []
	mtin, mtout, mterr=master_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/server/master;'+master_cmd)
	time.sleep(3)
	clin, clout, clerr=client_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/client;'+client_cmd)
	time.sleep(3)
	for i in range(num_total_worker):
		_win, _wout, _werr=worker_connections[i].exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step1/server/worker;'+worker_cmd+str(i))
		win.append(_win)
		wout.append(_wout)
		werr.append(_werr)
	time.sleep(30)
	client_output = clout.readlines()
	master_output = mtout.readlines()
	worker_output = []
	for _wout in wout:
		worker_output.append(_wout.readlines())

	client_file = open(client_filename, "w")
	for line in client_output:
		client_file.write(line)
	client_file.close()

	master_file = open(master_filename, "w")
	for line in master_output:
		master_file.write(line)
	master_file.close()

	for i in range(len(worker_output)):
		worker_file = open(dir_name+"worker_"+str(i)+".txt", "w")
		for line in worker_output[i]:
			worker_file.write(line)
		worker_file.close()

	print("finished width = 2^"+str(b)+ "\t" + datetime.datetime.now().strftime("%H:%M:%S"))

print("\nClosing connections...")
util.close_connections(master_connection, client_connection, worker_connections)
print("\nAll done!")
