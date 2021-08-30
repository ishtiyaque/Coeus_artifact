# Coeus

Coeus is a system for oblivious document ranking and retrieval. From high level, Coeus allows a user to search for documents in a public repository that are relevant to a private set of key words. To the core of Coeus' techniques is a secure matrix vector product protocol which significantly improves over the state of the art. This repository contains the source code and a step by step instructions to reproduce the secure matrix vector product protocol in Coeus and compare them with other baselines.

## Setup

### AWS

(Estimated time: 10 minutes)

We have installed the environment required for running Coeus in an AWS AMI and made it public. Following are the details of the publicly available AMI:


    AWS region: US East (Ohio)us-east-2
    AMI Name: Coeus_artifact
    AMI ID: ami-004d83515c02322aa
    Source: 235748323098/Coeus_artifact

Launch an AWS instance with this AMI. An instance with bigger memory is preferred, since the experiments are memory hungry (We used <code>c5.12xlarge</code> for our experiments, which has a memory of 96GB). 

Then, clone this repository:

    git clone https://github.com/ishtiyaque/Coeus_artifact

### Local (Tested on Ubuntu 18.04)

(Estimated time: 30 minutes)

First, clone this repository:

    git clone https://github.com/ishtiyaque/Coeus_artifact
    
Run the following command in the cloned directory:

    ./env_setup.sh

## Matrix-vector product in a single machine

### Build

(Estimated time: 5 minutes)

Run the following commands in the cloned directory:

    cd matmult
    cmake .
    make
    
This will generate three executable files in the <code>matmut/bin</code> directory, namely, <code>baseline</code>, <code>opt1</code>, and <code>opt2</code> corresponding to the Halevi Shoup baseline algorithm, Coeus with only the first optimization, and Coeus with both the first and second optimizations as discussed in the paper.

### Run

(Estimated time: 4 hours)

Each executable can be run by passing two arguments: number of row blocks and column blocks in the matrix. Each row block contains 8192 row elements and each column block contains 8192 column elements. The vector dimension is determined accordingly. For example, to run the most optimized version with 1 row block and 1 column block run the following command:

    bin/opt2 -r 1 -c 1
    
This will output the total time required as well as a breakdown of that time.

The script <code>run.sh</code> can be used to run all the executables for different number of vertically stacked column blocks, as shown in Figure 8. <code>run.sh</code> takes an argument <code>max_blocks</code> and runs each executable from 1 to <code>max_blocks</code> number of column blocks with a geometric progression of 2.

For example, to reproduce the exact experiments in Figure 8 of the paper, run the following command:

    bash ./run.sh 64
 
This may take a few hours depending on the configuration of the machine used. After finishing, the results will be available in some text files in the <code>results</code> directory. Please note that, experiments with higher number of blocks may fail if sufficient memory is not available. Considering a smaller value for <code>max_blocks</code> may be a way around for that.

Finally, figure 8 can be reproduced by the follwoing command:

    python3 gen_graph.py <max_blocks>
    
The figure will be available in a file named <code>single-machine-matrix-vector.pdf</code> in the <code>matmult</code> directory.

## Matrix-vector product over a cluster of machines

Coeus follows a master-worker architecture to support large-scale matrix vector product. The master receives the query and necessary keys from the client and distributes the computation task among a set of worker machines. In this part, we will describe how to perform this distributed matrix-vector product computation and regenerate the graph in Figure 9. The experiments involve three types of processes: a client process, a master process, and a number of worker processes. Ideally, each process should run on a separate machine. However, it is possible to run multiple processes on the same machine, the results might not look as expected in that case though.

### Build
First, clone this repo in each of the client, master, and worker machines.

To compile master code, run:

    cd step1/server/master
    cmake .
    make

To compile worker code, run:

    cd step1/server/worker
    cmake .
    make
    
To compile client code, run:

    cd step1/client
    cmake .
    make


### Run

Before running the experiments, it is required to synchronize the clocks of all the machines with an NTP server. This is required because some of the performance metrics are measured across machines. The drift that may occur after synchronization is negligible. One way to synchronize the clocks is simply to turn off the ntp synchronization and then turn it on. In each machine, run:

    sudo timedatectl set-ntp off
    sudo timedatectl set-ntp on
    
Then, in the master and worker machines, open the file <code>step1/common/worker_ip.txt</code> and overwrite it with a list of ip addresses of the worker machines, each in a separate line. Then, run each process in the order mentioned below. The explanation of the options associated with each command are also included afterwards.

Run master:

    python run_master.py -n <num_docs> -b <log2(submatrix_width)> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run client

    python run_client.py -n <num_docs> -b <log2(submatrix_width)> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run each worker:

    python run_worker.py -n <num_docs> -b <submatrix_width> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -b log2 of the submatrix width in each worker. (See Figure 9 in the paper)
    -p id address of the master machine
    -f number of total features. Must be a multiple of 8192, otherwise will be padded up.
    -c ip address of the client machine
    -w number of worker processes (multiple worker processes can be deployed in a single machine, though not recommended). The number of worker processes must be a multiple of (number of features / submatrix width) i.e. (f / 2^b)
    -t number of threads used by each worker process. If a single worker runs on a machine, the recommneded value is the number of cores in the machine. Otherwise, should be adjusted accordingly
    -i id of the worker where 0 <= i < number of worker processes. Id must be unique for each worker.

The experiment needs to be repeated for different submatrix widths. Each process will produce output files in its <code>result/</code> subdirectory.

### Generate graph (Figure 9)

After finishing all the experiments, copy all the output files (generated by master, workers, and client) into <code>graphs/multi-machine-matrix-vector/raw</code>

To generate a graph like Figure 9, run:

    cd graphs/multi-machine-matrix-vector
    python gen_graph.py <num_worker> <min_b> <max_b>

The graph will be available in a file named <code>multi-machine-matrix-vector.pdf</code>
