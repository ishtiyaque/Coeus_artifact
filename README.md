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
  
## Build

(Estimated time: 5 minutes)

Run the following commands in the cloned directory:

    cd matmult
    cmake .
    make
    
This will generate three executable files in the <code>matmut/bin</code> directory, namely, <code>baseline</code>, <code>opt1</code>, and <code>opt2</code> corresponding to the Halevi Shoup baseline algorithm, Coeus with only the first optimization, and Coeus with both the first and second optimizations as discussed in the paper.

## Run

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
    
The figure will be available in the <code>results</code> directory as <code>matmult.png</code>.
