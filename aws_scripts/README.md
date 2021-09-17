To generate the graphs in the paper, first clone this repository into a local or cloud machine. Put the file <code>Coeus_artifact.pem</code> into <code>Coeus_artifact/aws_scripts/</code>. Please note that, the permission level of <code>Coeus_artifact.pem</code> must be <code>0400</code>. Please update the permission if it is not so. Then put the files <code>config</code> and <code>credentials</code> in your <code>~/.aws/</code> directory.

## Setup

The experiments use three Python packages: <code>numpy</code>, <code>boto3</code>, and <code>paramiko</code>. Please install these packages with your favourite package manager before starting the experiments. For example, using <code>pip</code> the following command will set up the packages:

    pip install numpy boto3 paramiko
    
## Running the experiments

There are three directories for different graphs in the paper. Figure 6 and 7 are in the same directory as they are generated from the same result. Figure 5 must be generated before Figure 6 and 7. This is because Figure 5, 6, 7 have some overlapping experiments and they are performed in Figure 5 and the results are copied into the other.

In each of the directories, two Python files need to be run. First, run <code>python run_experiments.py</code>. That will run all the necessary experiments and dump the results into a directory named <code>result</code>. In <code>result</code>, the outputs for each data point will be in a different subdirectory. In case it is necessary to re-run experiments for just a subset of the datapoints, delete the corresponding sub-directories associated with those data points in <code>result</code> and run <code>python run_experiments.py</code> again. This will just re-run the experiments for the missing data points. After completion, run <code>python gen_graph.py</code>. This will generate a graph similar to the paper in a pdf file.

## Troubleshoot

Each experiment will put progress logs in the standard output along with timestamps. Sometimes a running script may hang due to disruption in network, internal errors in AWS, etc. As a rule of thumb, if there is no progress in 5 minutes, the script may be considered to be non-responsive. If that occurs, first terminate the running Python script. Then run the python script <code>Coeus_artifact/aws_scripts/kill_all.py</code>. This will take a while and kill all stray processes in the instances. This step is necessary because otherwise stray processes may keep occupying resources such as network ports, memory etc. After the script <code>kill_all.py</code> finishes successfully, re-run the interrupted experiment. it will resume from the point of interruption.


