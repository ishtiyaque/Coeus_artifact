import os, sys
import numpy as np

def get_latency(arr, scheme, w, n):
    for elem in arr:
        if (elem['scheme'] == scheme) and(int(elem['n']) == n) and (int(elem['w']) == w):
            return int(elem['latency'])

 
files = os.listdir('raw/')
w_list = set()
n_list = set()
data = []
for filename in files:
    arr = filename.split('_')
    if arr[0] == 'Baseline':
        w_list.add(int(arr[3]))
        n_list.add(int(arr[5]))
        client_file = open('raw/'+filename)
        client_output = client_file.readlines()
        latency = int(client_output[3])
        data.append({'scheme':'Baseline', 'w':int(arr[3]), 'n': int(arr[5]), 'latency':latency})
    elif arr[0] == 'Coeus':
        w_list.add(int(arr[3]))
        n_list.add(int(arr[5]))
        client_file = open('raw/'+filename)
        client_output = client_file.readlines()
        latency = int(client_output[3])
        data.append({'scheme':'Coeus', 'w':int(arr[3]), 'n': int(arr[5]), 'latency':latency})

n_list = sorted(n_list)
w_list = sorted(w_list)
os.system("touch latency-vs-machines.dat")
dat_file = open("latency-vs-machines.dat", "w")

for w in w_list:
    line = str(w)
    for scheme in ['Baseline', 'Coeus']:
        for n in n_list:
            latency = get_latency(data, scheme,w,n)
            line = line + '\t' + str(latency)
    dat_file.write(line)
dat_file.close()


cmd = "bash latency-vs-machines.sh "
#print(cmd)
os.system(cmd)
os.system("rm -f multi-machine-matrix-vector.dat")
os.system("rm -f plot.plt")