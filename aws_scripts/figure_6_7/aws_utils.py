import boto3
import numpy as np
import paramiko
import time
import os
import datetime

def update_buffer_size(master_connection, client_connection, worker_connections):
    cmd = 'sudo sysctl -w net.core.rmem_default=1073741824;'
    cmd += 'sudo sysctl -w net.core.wmem_default=1073741824;'
    cmd += 'sudo sysctl -w net.core.rmem_max=1073741824;'
    cmd += 'sudo sysctl -w net.core.wmem_max=1073741824'
    for conn in worker_connections:
        conn.exec_command(cmd)
    master_connection.exec_command(cmd)
    client_connection.exec_command(cmd)
	
def synch_time(master_connection, client_connection, worker_connections):
    cmd = 'sudo timedatectl set-ntp off; sudo timedatectl set-ntp on'
    for conn in worker_connections:
        conn.exec_command(cmd)
    master_connection.exec_command(cmd)
    client_connection.exec_command(cmd)
	
def close_connections(master_connection, client_connection, worker_connections):
    for conn in worker_connections:
        conn.close()
    master_connection.close()
    client_connection.close()
	