#!/bin/sh
#if [[ $# != '0' ]]
#then
#        echo "usage: $0"
#        exit 1
#fi

GRAPHNAME=`basename $0 .sh`
EPS=$GRAPHNAME.eps
touch plot.plt

echo "
set terminal postscript eps 'Times-Roman,30' color size 7.2,3.8
set output '$GRAPHNAME.eps'
#set xrange [ 1024: 65536]
set xrange [ $1: $2]
set xtics nomirror font 'Times-Roman, 42' offset 0,-0.3
set logscale x 2
set format x '2^{%L}'
set nomxtics
set xlabel \"Width of submatrix at each worker\" font 'Times-Roman, 42' offset 0.0, -0.3
#set yrange [ 0.1 : 10]
set ylabel \"Wall-clock time (sec)\" font 'Times-Roman, 42' offset -1.0,0.0
set ytics 2 nomirror font 'Times-Roman, 42'
set yrange [0: $3]
set mytics
#set logscale y 10
set grid noxtics noytics
set border 3 lw 4
set key top left maxrows 3 spacing 1.1 samplen 2.5 font 'Times-Roman, 36'
set bmargin 3.5
set pointsize 2.0
#set size 1.1, 1
set style function linespoints
set style line 1 lc rgb '#006a4e' dt 1 lt 1 lw 11 pt 7 pi -1 ps 3.5
set style line 2 lc rgb '#006a4e' dt 1 lt 1 lw 7 pt 5 pi -1 ps 3.5
set style line 3 lc rgb '#006a4e' dt 1 lt 1 lw 9 pt 9 pi -1 ps 3.5
set style line 4 lc rgb 'black' dt 1 lt 1 lw 9 pt 6 pi -1 ps 3.5
set style line 5 lc rgb '#8d8d8d' dt 1 lt 1 lw 9 pt 4 pi -1 ps 3.5
set style line 6 lc rgb '#6495ed' dt 1 lt 1 lw 8 pt 8 pi -1 ps 3.5
set style line 7 lc rgb '#8d8d8d' dt 1 lt 1 lw 9 pt 8 pi -1 ps 4.5
plot \
'multi-machine-matrix-vector.dat' using 1:(\$2/1000000) with linespoints ls 4 title 'Input-dist.',\
'multi-machine-matrix-vector.dat' using 1:(\$3/1000000) with linespoints ls 5 title 'Compute',\
'multi-machine-matrix-vector.dat' using 1:(\$4/1000000) with linespoints ls 6 title 'Aggregation',\
'multi-machine-matrix-vector.dat' using 1:(\$5/1000000) with linespoints ls 1 title 'Total',\
" > plot.plt
gnuplot plot.plt
epspdf $EPS
rm $EPS
