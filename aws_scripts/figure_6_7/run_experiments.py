import boto3
import numpy as np
import paramiko
import time
import os
import shutil
import datetime
import aws_utils as util

MASTER_NAME = "Coeus_master_96"
CLIENT_NAME = "Coeus_client"
WORKER_NAME = "Coeus_worker"

PEM_FILE_NAME = "Coeus_artifact.pem"

##Step1

print("Copying step1 results from figure 5...\n")

doc_arr = ["300K", "1.2M", "5M"]
worker_arr = [32, 48, 64, 80, 96]

for doc in doc_arr:
	if os.path.exists("./result/doc_{}/step1/client.txt".format(doc)):
		continue
	min_latency = 9999999999
	min_worker = 0
	for num_worker in worker_arr:
		client_filename = "../figure_5/result/Coeus/doc_{}/worker_{}/client.txt".format(doc, num_worker)
		if (os.path.exists(client_filename) == False):
			print("Figure 5 results are not available!\nexiting\n")
			exit(0)
		client_file = open(client_filename)
		client_output = client_file.readlines()
		latency = int(client_output[3])
		if latency < min_latency:
			min_latency = latency
			min_worker = num_worker
	dest_dir = "./result/doc_{}/step1/".format(doc)
	os.system("mkdir -p "+dest_dir)
	shutil.copy("../figure_5/result/Coeus/doc_{}/worker_{}/client.txt".format(doc, min_worker), dest_dir + "client.txt" )
print("Step1 done!\n")	

print("Starting Step2...\n")
## Step 2
k = 16
factor = 1.5
num_thread_per_worker = 12
elem_size = 320

num_total_worker = int(np.ceil(k*factor))
num_worker_machine = 6
worker_per_machine = int(num_total_worker/num_worker_machine)


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
for i in range(num_worker_machine):
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
    for i in range(worker_per_machine):
        f.write(ip)
        f.write("\n")
f.close()

print("\nCreated local file worker_ip.txt")

master_ftp=master_connection.open_sftp()

worker_ftp = []
for i in range(num_worker_machine):
    worker_ftp.append(worker_connections[i].open_sftp())
	
master_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step2/common/worker_ip.txt")
for w_ftp in worker_ftp:
    w_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step2/common/worker_ip.txt")
print("Copied worker_ip.txt to master and workers.")

print("\nStarting experiments..."+ "\t" + datetime.datetime.now().strftime("%H:%M:%S") + "\n")

num_elements_arr = [{"key": "300K", "value": 300000}, 
					{"key": "1.2M", "value": 1200000},
					{"key": "5M", "value": 5000000}]


for elem in num_elements_arr:
	num_elements = elem["value"]

	worker_cmd = 'bin/worker -n {} -t {} -s {} -k {} -c {} -i '
	worker_cmd = worker_cmd.format(num_elements, num_thread_per_worker,elem_size,k, client_private_ip)
	master_cmd = 'bin/master -k {} -p {}'
	master_cmd = master_cmd.format(k, master_private_ip)
	client_cmd = 'bin/client -n {} -k {} -p {} -s {} -c {}'
	client_cmd = client_cmd.format(num_elements, k, master_private_ip,elem_size, client_private_ip)

	dir_name = 'result/doc_{}/step2/'
	dir_name = dir_name.format(elem["key"])
	os.system('mkdir -p '+dir_name)

	client_filename = dir_name+"client.txt"
	if os.path.exists(client_filename):
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) == 12:
			print("finished Step2: {} docs\tskipped".format(num_elements))
			client_file.close()
			continue
		client_file.close()

	
	win = []
	wout = []
	werr = []
	mtin, mtout, mterr=master_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step2/master;'+master_cmd)
	time.sleep(1)
	clin, clout, clerr=client_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step2/client;'+client_cmd)
	time.sleep(1)
	for i in range(num_worker_machine):
		for j in range(worker_per_machine):
			wid = ((i*worker_per_machine)+j)
			worker_cmd_temp = worker_cmd+str(wid)
			_win, _wout, _werr=worker_connections[i].exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step2/worker;'+worker_cmd_temp)
			win.append(_win)
			wout.append(_wout)
			werr.append(_werr)
	time.sleep(15)
	worker_output = []
	for _wout in wout:
		worker_output.append(_wout.readlines())

	client_output = clout.readlines()
	master_output = mtout.readlines()

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

	print("finished Step2: {} docs\t".format(num_elements) + datetime.datetime.now().strftime("%H:%M:%S"))

print("\nClosing connections...")
util.close_connections(master_connection, client_connection, worker_connections)
print("\nStep2 done!")



## Step 3
print("\nStarting step3 experiments...\n")
num_thread_per_worker = 16
num_total_worker = 38

num_elements_arr = [{"key": "300K", "value": 5770}, 
					{"key": "1.2M", "value": 23077},
					{"key": "5M", "value": 96151}]

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
for i in range(num_total_worker):
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
for i in range(num_total_worker):
    worker_ftp.append(worker_connections[i].open_sftp())
	
master_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step3/common/worker_ip.txt")
for w_ftp in worker_ftp:
    w_ftp.put("./worker_ip.txt", "/home/ubuntu/Coeus_artifact/step3/common/worker_ip.txt")
print("Copied worker_ip.txt to master and workers.")


for elem in num_elements_arr:
	num_elements = elem["value"]
	worker_cmd = 'bin/worker -n {} -t {} -w {} -c {} -i '
	worker_cmd = worker_cmd.format(num_elements, num_thread_per_worker, num_total_worker, client_private_ip)
	master_cmd = 'bin/master -w {} -p {}'
	master_cmd = master_cmd.format(num_total_worker, master_private_ip)
	client_cmd = 'bin/client -n {} -w {} -p {} -c {}'
	client_cmd = client_cmd.format(num_elements, num_total_worker, master_private_ip, client_private_ip)
	
	dir_name = 'result/doc_{}/step3/'
	dir_name = dir_name.format(elem["key"])
	os.system('mkdir -p '+dir_name)

	client_filename = dir_name+"client.txt"
	if os.path.exists(client_filename):
		client_file = open(client_filename)
		client_output = client_file.readlines()
		if len(client_output) == 12:
			print("finished Step3: {} docs\tskipped".format(num_elements))
			client_file.close()
			continue
		client_file.close()

	
	win = []
	wout = []
	werr = []
	mtin, mtout, mterr=master_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step3/master;'+master_cmd)
	time.sleep(3)
	clin, clout, clerr=client_connection.exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step3/client;'+client_cmd)
	time.sleep(3)
	for i in range(num_total_worker):
		_win, _wout, _werr=worker_connections[i].exec_command('ulimit -n 4096; cd ~/Coeus_artifact/step3/worker;'+worker_cmd+str(i))
		win.append(_win)
		wout.append(_wout)
		werr.append(_werr)
	time.sleep(15)
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

	print("finished Step3: {} docs\t".format(elem["key"]) + datetime.datetime.now().strftime("%H:%M:%S"))

print("\nClosing connections...")
util.close_connections(master_connection, client_connection, worker_connections)
print("\nStep3 done!")
