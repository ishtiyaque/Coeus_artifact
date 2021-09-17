To generate the graphs in the paper, first clone this repository into a local or cloud machine. Put the file <code>Coeus_artifact.pem</code> into <code>Coeus_artifact/aws_scripts/</code>. Please note that, the permission level of <code>Coeus_artifact.pem</code> must be <code>0400</code>. Please update the permission if it is not so.

There are three directories for different graphs in the paper. Figure 6 and 7 are in the same directory as they are generated from the same result. Figure 5 must be generated before Figure 6 and 7. This is because Figure 5, 6, 7 have some overlapping experiments and they are performed in Figure 5 and the results are copied into the other.

In each of the directories, two Python files need to be run. First, run <code>python run_experiments.py</code>. That will run all the necessary experiments and dump the results into a directory named <code>result</code>. In <code>result</code>, the outputs for each data point will be in a different subdirectory. In case it is necessary to re run experiments for just a subset of the datapoints, delete the corresponding sub-directories associated with those data points and run <code>python run_experiments.py</code> again. This will just re-run the experiments for the missing data points. After completion, run <code>python gen_graph.py</code>. This will generate a graph similar to the paper in a pdf file.




