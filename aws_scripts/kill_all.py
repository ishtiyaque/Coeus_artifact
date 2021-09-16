import boto3
import numpy as np
import paramiko
import time
import os
import datetime

MASTER_NAME = "Coeus_master_96"
CLIENT_NAME = "Coeus_client"
WORKER_NAME = "Coeus_worker"

PEM_FILE_NAME = "Coeus_artifact.pem"

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
	
server_key = paramiko.RSAKey.from_private_key_file('./'+PEM_FILE_NAME)
client_key = paramiko.RSAKey.from_private_key_file('./'+PEM_FILE_NAME)

print("Terminating all processes. Please wait...\n")

worker_connections = []
for i in range(len(worker_public_ip)):
    worker_connections.append(paramiko.SSHClient())
    worker_connections[i].set_missing_host_key_policy(paramiko.AutoAddPolicy())
    worker_connections[i].connect(hostname=worker_public_ip[i], username="ubuntu", pkey=server_key)
#print("Connected to workers")	

master_connection = paramiko.SSHClient()
master_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
master_connection.connect(hostname=master_public_ip, username="ubuntu", pkey=server_key)
#print("Connected to master")	

client_connection = paramiko.SSHClient()
client_connection.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client_connection.connect(hostname=client_public_ip, username="ubuntu", pkey=client_key)
#print("Connected to client")


for i in range(len(worker_connections)):
	_win, _wout, _werr = worker_connections[i].exec_command('sudo pkill worker')
	_win, _wout, _werr = worker_connections[i].exec_command('sudo pkill split')
	_win, _wout, _werr = worker_connections[i].exec_command('sudo pkill merge')
_min, _mout, _merr = master_connection.exec_command('sudo pkill master')
_min, _mout, _merr = master_connection.exec_command('sudo pkill split')
_min, _mout, _merr = master_connection.exec_command('sudo pkill merge')

_cin, _cout, _cerr = client_connection.exec_command('sudo pkill client')
_cin, _cout, _cerr = client_connection.exec_command('sudo pkill split')
_cin, _cout, _cerr = client_connection.exec_command('sudo pkill merge')

for conn in worker_connections:
	conn.close()
master_connection.close()
client_connection.close()

print("All done!")