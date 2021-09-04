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

## Matrix-vector product in a single machine (Figure 8)

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

## Matrix-vector product over a cluster of machines (Figure 9)

Coeus follows a master-worker architecture to support large-scale matrix vector product. The master receives the query and necessary keys from the client and distributes the computation task among a set of worker machines. In this part, we will describe how to perform this distributed matrix-vector product computation and regenerate the graph in Figure 9. The experiments involve three types of processes: a client process, a master process, and a number of worker processes. Ideally, each process should run on a separate machine. However, it is possible to run multiple processes on the same machine, the results might not look as expected in that case though.

### Build
First, clone this repo in each of the client, master, and worker machines. Then, build the relevant part in each machine.

To build master code, run:

    cd step1/server/master
    cmake .
    make

To build worker code, run:

    cd step1/server/worker
    cmake .
    make
    
To build client code, run:

    cd step1/client
    cmake .
    make


### Run

Before running the experiments, it is required to synchronize the clocks of all the machines with an NTP server. This is required because some of the performance metrics are measured across machines. The drift that may occur after synchronization is negligible. One way to synchronize the clocks is simply to turn off the ntp synchronization and then turn it on. In each machine, run:

    sudo timedatectl set-ntp off
    sudo timedatectl set-ntp on
    
Then, in the master and worker machines, open the file <code>step1/common/worker_ip.txt</code> and overwrite it with a list of ip addresses of the worker machines, each in a separate line. Then, run each process in the order mentioned below. The explanation of the options associated with each command are also included afterwards.

Run master:

    python run_master_matmult.py -n <num_docs> -b <log2(submatrix_width)> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run client

    python run_client_matmult.py -n <num_docs> -b <log2(submatrix_width)> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run each worker:

    python run_worker_matmult.py -n <num_docs> -b <log2(submatrix_width)> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>
    
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

## Comparison of query scoring step in Coeus (Figure 5)

The query scoring step in Coeus is essentially a large scale matrix-vector product. In the paper, Figure 5 shows a comparison of this step with an unoptimised baseline. To reproduce the results, first determine three different numbers of documents (as in Fig. 5). Then both Coeus' optimized protocol and the unoptimized baseline should be run for all three document library sizes, with a fixed number of features. A similar master, worker, client environment needs to be set up.

### Build

Following are the commands for building the code for both Coeus and the baseline baseline.
To compile master code, run:

    pushd baseline/server/master
    cmake .
    make
	popd
	pushd step1/server/master
    cmake .
    make
	popd


To compile worker code, run:

    pushd baseline/server/worker
    cmake .
    make
	popd
	pushd step1/server/worker
	cmake .
	make
	popd
    
To compile client code, run:

    pushd baseline/client
    cmake .
    make
	popd
	pushd step1/client
	cmake .
	make
	popd

### Run

Before running the experiments, it is required to synchronize the clocks of all the machines with an NTP server. This is required because some of the performance metrics are measured across machines. The drift that may occur after synchronization is negligible. One way to synchronize the clocks is simply to turn off the ntp synchronization and then turn it on. In each machine, run:

    sudo timedatectl set-ntp off
    sudo timedatectl set-ntp on
    
Then, in the master and worker machines, replace the files <code>step1/common/worker_ip.txt</code> and <code>baseline/common/worker_ip.txt</code> with a list of ip addresses of the worker machines, each in a separate line. Then, run Coeus and baseline experiments independently as outlined below.run each process in the order mentioned below. The explanation of the options associated with each command are also included afterwards.

#### Coeus

Run master:

	cd step1/server/master
	python run_master_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run client

	cd step1/client
	python run_client_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run each worker:

	cd step1/server/worker
	python run_worker_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -p id address of the master machine
    -f number of total features. Must be a multiple of 8192, otherwise will be padded up.
    -c ip address of the client machine
    -w number of worker processes (multiple worker processes can be deployed in a single machine, though not recommended).
    -t number of threads used by each worker process. If a single worker runs on a machine, the recommneded value is the number of cores in the machine. Otherwise, should be adjusted accordingly
    -i id of the worker where 0 <= i < number of worker processes. Id must be unique for each worker.

The experiment needs to be repeated for different document library sizes. Each process will produce output files in its <code>result/</code> subdirectory.

#### Baseline

Run master:

	cd baseline/server/master
    python run_master_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run client

	cd baseline/client
    python run_client_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run each worker:

	cd baseline/server/worker
    python run_worker_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -p id address of the master machine
    -f number of total features. Must be a multiple of 8192, otherwise will be padded up.
    -c ip address of the client machine
    -w number of worker processes (multiple worker processes can be deployed in a single machine, though not recommended).
    -t number of threads used by each worker process. If a single worker runs on a machine, the recommneded value is the number of cores in the machine. Otherwise, should be adjusted accordingly
    -i id of the worker where 0 <= i < number of worker processes. Id must be unique for each worker.

The experiment needs to be repeated for different document library sizes. Each process will produce output files in its <code>result/</code> subdirectory.

### Generate graph (Figure 5)

After completion of the experiments, in the client machine, copy the files in <code>step1/client/result/</code> and <code>baseline/client/result/</code> into <code>graphs/latency-vs-machines/raw/</code>. To generate a graph like Figure 5, run:

    cd graphs/latency-vs-machines
    python gen_graph.py 

The graph will be available in a file named <code>latency-vs-machines.pdf</code>

## End-to-end performance of Coeus (Figure 6 and 7)

The end-to-end pipeline of Coeus consists of 3 sequential steps. Figure 6 in the paper shows a latency breakdown of these three different steps for three different document library sizes. Figure 7 shows the client CPU and network costs for the same experiments. In order to reproduce these results, one first needs to pick three different document sizes and then execute each of step1, step2, and step3 of Coeus. All of the three steps require a similar master, worker, and client setup.

### Step1

#### Build

To build master code, run:

	cd step1/server/master
	cmake .
	make

To build worker code, run:

	cd step1/server/worker
	cmake .
	make
    
To build client code, run:

	cd step1/client
	cmake .
	make

#### Run

First, in the master and worker machines, replace the files <code>step1/common/worker_ip.txt</code> with a list of ip addresses of the worker machines, each in a separate line. 

Run master:

	cd step1/server/master
	python run_master_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run client

	cd step1/client
	python run_client_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers>
    
Run each worker:

	cd step1/server/worker
	python run_worker_latency.py -n <num_docs> -p <master_ip> -f <num_features> -c <client_ip> -w <num_workers> -t <num_thread_per_worker> -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -p id address of the master machine
    -f number of total features. Must be a multiple of 8192, otherwise will be padded up.
    -c ip address of the client machine
    -w number of worker processes (multiple worker processes can be deployed in a single machine, though not recommended).
    -t number of threads used by each worker process. If a single worker runs on a machine, the recommneded value is the number of cores in the machine. Otherwise, should be adjusted accordingly
    -i id of the worker where 0 <= i < number of worker processes. Id must be unique for each worker.

The experiment needs to be repeated for different document library sizes. Each process will produce output files in its <code>result/</code> subdirectory.


### Step2

#### Build

To build master code, run:

	cd step2/master
	cmake .
	make

To build worker code, run:

	cd step2/worker
	cmake .
	make
    
To build client code, run:

	cd step2/client
	cmake .
	make

#### Run

Step2 should be run with a fixed number of 24 worker processes. Multiple workers may run on a single machine. For our experiments, we used 6 worker machines to run the 24 processes. In the master and worker machines, the file <code>step2/common/worker_ip.txt</code> should be replaced with the 24 worker ip addresses (possibly with repetitions), each in a separate line.

Run master:

	cd step2/master
	python run_master_latency.py -n <num_docs> -p <master_ip> 
    
Run client

	cd step2/client
	python run_client_latency.py -n <num_docs> -p <master_ip>  -c <client_ip>
    
Run each worker:

	cd step2/worker
	python run_worker_latency.py -n <num_docs> -c <client_ip>  -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -p id address of the master machine
    -c ip address of the client machine
    -i id of the worker where 0 <= i < 24. Id must be unique for each worker.

The experiment needs to be repeated for different document library sizes. Each process will produce output files in its <code>result/</code> subdirectory.

### Step3

#### Build

To build master code, run:

	cd step3/master
	cmake .
	make

To build worker code, run:

	cd step3/worker
	cmake .
	make
    
To build client code, run:

	cd step3/client
	cmake .
	make

#### Run

Step3 should be run with a fixed number of 38 worker processes. Multiple workers may run on a single machine, though not recommended. In the master and worker machines, the file <code>step3/common/worker_ip.txt</code> should be replaced with the 38 worker ip addresses (possibly with repetitions), each in a separate line.

Run master:

	cd step3/master
	python run_master_latency.py -n <num_docs> -p <master_ip> 
    
Run client

	cd step3/client
	python run_client_latency.py -n <num_docs> -p <master_ip>  -c <client_ip>
    
Run each worker:

	cd step2/worker
	python run_worker_latency.py -n <num_docs> -c <client_ip>  -i <worker_id>
    
The options are explained below:

    -n Number of documents
    -p id address of the master machine
    -c ip address of the client machine
    -i id of the worker where 0 <= i < 38. Id must be unique for each worker.
    
The experiment needs to be repeated for different document library sizes. Each process will produce output files in its <code>result/</code> subdirectory.

### Generate graphs (Figure 6 and 7)
