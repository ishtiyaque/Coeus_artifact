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


worker_connections = []

master_connection = paramiko.SSHClient()
master_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
master_connection.connect(hostname=master_public_ip, username="ubuntu", pkey=server_key)
print("Connected to master")	

client_connection = paramiko.SSHClient()
client_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client_connection.connect(hostname=client_public_ip, username="ubuntu", pkey=client_key)
print("Connected to client")	


num_thread_per_worker = 48
num_query = 8
num_iter = 1

coeus_mapping = [{"doc":"300K", "num_response":12, "num_worker":32, "split_factor":4},
				 {"doc":"1.2M", "num_response":48, "num_worker":32, "split_factor":4},
				 {"doc":"5M", "num_response":204, "num_worker":32, "split_factor":4},
				 {"doc":"300K", "num_response":12, "num_worker":48, "split_factor":6},
				 {"doc":"1.2M", "num_response":48, "num_worker":48, "split_factor":2},
				 {"doc":"5M", "num_response":204, "num_worker":48, "split_factor":2},
				 {"doc":"300K", "num_response":12, "num_worker":64, "split_factor":8},
				 {"doc":"1.2M", "num_response":48, "num_worker":64, "split_factor":8},
				 {"doc":"5M", "num_response":204, "num_worker":64, "split_factor":2},
				 {"doc":"300K", "num_response":12, "num_worker":80, "split_factor":10},
				 {"doc":"1.2M", "num_response":48, "num_worker":80, "split_factor":10},
				 {"doc":"5M", "num_response":204, "num_worker":80, "split_factor":10},
				 {"doc":"300K", "num_response":12, "num_worker":96, "split_factor":12},
				 {"doc":"1.2M", "num_response":48, "num_worker":96, "split_factor":4},
				 {"doc":"5M", "num_response":204, "num_worker":96, "split_factor":2}]

baseline_mapping = [{"doc":"300K", "num_response":12, "num_worker":32, "blocks":1},
					{"doc":"1.2M", "num_response":48, "num_worker":32, "blocks":4},
					{"doc":"5M", "num_response":200, "num_worker":32, "blocks":2},
					{"doc":"300K", "num_response":12, "num_worker":48, "blocks":2},
					{"doc":"1.2M", "num_response":48, "num_worker":48, "blocks":4},
					{"doc":"5M", "num_response":204, "num_worker":48, "blocks":2},
					{"doc":"300K", "num_response":8, "num_worker":64, "blocks":1},
					{"doc":"1.2M", "num_response":48, "num_worker":64, "blocks":2},
					{"doc":"5M", "num_response":192, "num_worker":64, "blocks":4},
					{"doc":"300K", "num_response":10, "num_worker":80, "blocks":1},
					{"doc":"1.2M", "num_response":40, "num_worker":80, "blocks":2},
					{"doc":"5M", "num_response":200, "num_worker":80, "blocks":4},
					{"doc":"300K", "num_response":12, "num_worker":96, "blocks":1},
					{"doc":"1.2M", "num_response":48, "num_worker":96, "blocks":2},
					{"doc":"5M", "num_response":204, "num_worker":96, "blocks":1}]

print("\nStarting Coeus experiments"+ "\t" + datetime.datetime.now().strftime("%H:%M:%S") + "\n")

for elem in coeus_mapping:
	num_total_worker = elem["num_worker"]
	num_response = elem["num_response"]
	split_factor = elem["split_factor"]
	num_group = int(num_total_worker / (num_query * split_factor))
	num_worker_per_group = int(num_total_worker / num_group)
	num_docs = elem["doc"]

	dir_name = "result/Coeus/doc_{}/worker_{}/".format(num_docs, num_total_worker)
	os.system('mkdir -p '+dir_name)
	client_filename = dir_name+"client.txt"
	if os.path.exists(client_filename):
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) == 20:
			print("finished Coeus: {} workers {} docs\tskipped".format(num_total_worker, num_docs))
			client_file.close()
			continue
		client_file.close()	
		
	if num_total_worker > len(worker_connections):
		util.close_connections(master_connection, client_connection, worker_connections)
		print("\nConnecting to {} workers...".format(num_total_worker))
		worker_connections = []
		for i in range(num_total_worker):
			worker_connections.append(paramiko.SSHClient())
			worker_connections[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
			worker_connections[i].connect(hostname=worker_public_ip[i], username="ubuntu", pkey=server_key)
		master_connection = paramiko.SSHClient()
		master_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		master_connection.connect(hostname=master_public_ip, username="ubuntu", pkey=server_key)
		client_connection = paramiko.SSHClient()
		client_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		client_connection.connect(hostname=client_public_ip, username="ubuntu", pkey=client_key)
		print("Connections established")

		util.update_buffer_size(master_connection, client_connection, worker_connections)
		util.synch_time(master_connection, client_connection, worker_connections)
		print("Synchronized clocks")
		f = open("./worker_ip.txt", "w")
		for i in range(len(worker_connections)):
			f.write(worker_private_ip[i])
			f.write("\n")
		f.close()
		#print("\nCreated local file worker_ip.txt")
		master_ftp=master_connection.open_sftp()
		worker_ftp = []
		for i in range(len(worker_connections)):
			worker_ftp.append(worker_connections[i].open_sftp())

		master_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step1/common/worker_ip.txt")
		for w_ftp in worker_ftp:
			w_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step1/common/worker_ip.txt")
		print("Copied worker_ip.txt to master and workers.\n")

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

	master_file = open(dir_name+"master.txt", "w")
	for line in master_output:
		master_file.write(line)
	master_file.close()

	for i in range(len(worker_output)):
		worker_file = open(dir_name+"worker_"+str(i)+".txt", "w")
		for line in worker_output[i]:
			worker_file.write(line)
		worker_file.close()

	print("finished Coeus: {} workers {} docs\t".format(num_total_worker, num_docs) + datetime.datetime.now().strftime("%H:%M:%S"))

util.close_connections(master_connection, client_connection, worker_connections)
worker_connections = []

print("\nStarting Baseline experiments"+ "\t" + datetime.datetime.now().strftime("%H:%M:%S") + "\n")

for elem in baseline_mapping:
	num_total_worker = elem["num_worker"]
	num_response = elem["num_response"]
	scale_factor = elem["blocks"]
	num_group = int(num_total_worker / (num_query / scale_factor))
	num_worker_per_group = int(num_total_worker / num_group)
	num_docs = elem["doc"]

	dir_name = "result/Baseline/doc_{}/worker_{}/".format(num_docs, num_total_worker)
	os.system('mkdir -p '+dir_name)
	client_filename = dir_name+"client.txt"
	if os.path.exists(client_filename):
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) == 10:
			print("finished Baseline: {} workers {} docs\tskipped".format(num_total_worker, num_docs))
			client_file.close()
			continue
		client_file.close()

	if num_total_worker > len(worker_connections):
		util.close_connections(master_connection, client_connection, worker_connections)
		print("\nConnecting to {} workers...".format(num_total_worker))
		worker_connections = []
		for i in range(num_total_worker):
			worker_connections.append(paramiko.SSHClient())
			worker_connections[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
			worker_connections[i].connect(hostname=worker_public_ip[i], username="ubuntu", pkey=server_key)
		master_connection = paramiko.SSHClient()
		master_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		master_connection.connect(hostname=master_public_ip, username="ubuntu", pkey=server_key)
		client_connection = paramiko.SSHClient()
		client_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		client_connection.connect(hostname=client_public_ip, username="ubuntu", pkey=client_key)
		print("Connections established")

		util.update_buffer_size(master_connection, client_connection, worker_connections)
		util.synch_time(master_connection, client_connection, worker_connections)
		print("Synchronized clocks")

		f = open("./worker_ip.txt", "w")
		for i in range(len(worker_connections)):
			f.write(worker_private_ip[i])
			f.write("\n")
		f.close()
		#print("\nCreated local file worker_ip.txt")

		master_ftp=master_connection.open_sftp()

		worker_ftp = []
		for i in range(len(worker_connections)):
			worker_ftp.append(worker_connections[i].open_sftp())

		master_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/baseline/common/worker_ip.txt")
		for w_ftp in worker_ftp:
			w_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/baseline/common/worker_ip.txt")
		print("Copied worker_ip.txt to master and workers.\n")

	worker_cmd = 'bin/worker -w {} -s {} -t {} -c {} -r {} -g {} -i '
	worker_cmd = worker_cmd.format(num_worker_per_group, scale_factor, num_thread_per_worker,client_private_ip, int(num_response/num_group), num_group)
	master_cmd = 'bin/master -r {} -s {} -p {} -w {} -g {} '
	master_cmd = master_cmd.format(num_response, scale_factor, master_private_ip, num_worker_per_group, num_group)
	client_cmd = 'bin/client -r {} -s {} -p {} -q {} -c {} -g {}'
	client_cmd = client_cmd.format(num_response, scale_factor, master_private_ip, num_query, client_private_ip, num_group)

	win = []
	wout = []
	werr = []
	mtin, mtout, mterr=master_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/baseline/server/master;'+master_cmd)
	time.sleep(3)
	clin, clout, clerr=client_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/baseline/client;'+client_cmd)
	time.sleep(3)
	for i in range(num_total_worker):
		_win, _wout, _werr=worker_connections[i].exec_command('ulimit -n 4096; cd ~/Coeus_artifact/baseline/server/worker;'+worker_cmd+str(i))
		win.append(_win)
		wout.append(_wout)
		werr.append(_werr)
	time.sleep(30)
	client_output = clout.readlines()
	master_output = mtout.readlines()
	worker_output = []
	for _wout in wout:
		worker_output.append(_wout.readlines())

	client_file = open(dir_name+"client.txt", "w")
	for line in client_output:
		client_file.write(line)
	client_file.close()

	master_file = open(dir_name+"master.txt", "w")
	for line in master_output:
		master_file.write(line)
	master_file.close()

	for i in range(len(worker_output)):
		worker_file = open(dir_name+"worker_"+str(i)+".txt", "w")
		for line in worker_output[i]:
			worker_file.write(line)
		worker_file.close()

	print("finished Baseline: {} workers {} docs\t".format(num_total_worker, num_docs) + datetime.datetime.now().strftime("%H:%M:%S"))


print("\nClosing connections...")
util.close_connections(master_connection, client_connection, worker_connections)
print("\nAll done!")
