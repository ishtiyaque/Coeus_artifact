#!/bin/bash

if [ $# != 1 ]; then
	echo "usage ./run.sh <max_blocks>"
	exit
fi

mkdir -p results

for program in  baseline opt1 opt2
do
	mkdir -p results/$program
	num_block=1
	for (( ;$num_block <= $1; num_block=$[ $num_block * 2] ))
	do
		bin/$program -r $num_block -c 1 > results/$program/r_$num_block.txt
		echo finished $program r = $num_block
	done

done
