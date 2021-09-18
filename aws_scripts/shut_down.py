import boto3
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

worker_instances = server_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [WORKER_NAME]}])

print("Shutting down workers...\n")
for instance in worker_instances:
	instance.stop()

master_instances = server_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [MASTER_NAME]}])
print("Shutting down master...\n")
for instance in master_instances:
	instance.stop()

client_instances = client_ec2.instances.filter(
        Filters=[{'Name': 'instance-state-name', 'Values': ['running']},
                {'Name': 'tag:Name', 'Values': [CLIENT_NAME]}])

print("Shutting down client...\n")
for instance in client_instances:
    instance.stop()
print("All done!")