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
set terminal postscript eps 'Times-Roman,30' color size 7,3.8
set output '$GRAPHNAME.eps'
set xrange [ 18 : 110]
#set xtics 20 nomirror font 'Times-Roman, 40' offset 0,-0.1
set xtics (\"32\" 32, \"48\" 48, \"64\" 64, \"80\" 80, \"96\" 96) font 'Times-Roman, 40'
set nomxtics
set xlabel \"Number of machines for the query-scorer\" font 'Times-Roman, 40'
set yrange [ 0.1 : 10000]
set ylabel \"Query-scoring latency (sec)\" font 'Times-Roman, 40' offset -1.0,0.0
set ytics nomirror font 'Times-Roman, 40'
set nomytics
set xtics nomirror
set logscale y 10
#set grid noxtics noytics
set grid ytics lw -1
#set grid mytics lw -1
set border 3 lw 4
set key top right maxrows 3 samplen 2.5 font 'Times-Roman, 30'
set bmargin 3.5
set pointsize 2.0
#set size 1.1, 1
set style function linespoints
set style line 1 lc rgb '#006a4e' dt 1 lt 1 lw 10 pt 7 pi -1 ps 2.5
set style line 2 lc rgb '#006a4e' dt 1 lt 1 lw 6 pt 5 pi -1 ps 2.5
set style line 3 lc rgb '#006a4e' dt 1 lt 1 lw 8 pt 9 pi -1 ps 3.5
set style line 4 lc rgb '#8d8d8d' dt 1 lt 1 lw 8 pt 6 pi -1 ps 2.5
set style line 5 lc rgb '#8d8d8d' dt 1 lt 1 lw 8 pt 4 pi -1 ps 2.5
set style line 6 lc rgb '#8d8d8d' dt 1 lt 1 lw 7 pt 8 pi -1 ps 2.5
set style line 7 lc rgb '#8d8d8d' dt 1 lt 1 lw 8 pt 8 pi -1 ps 3.5
plot \
'latency-vs-machines.dat' using 1:(\$2/1000000) with linespoints ls 4 title 'Base. (n=300K)',\
'latency-vs-machines.dat' using 1:(\$3/1000000) with linespoints ls 5 title 'Base. (n=1.2M)',\
'latency-vs-machines.dat' using 1:(\$4/1000000) with linespoints ls 6 title 'Base. (n=5M)',\
'latency-vs-machines.dat' using 1:(\$5/1000000) with linespoints ls 1 title 'Coeus (n=300K)',\
'latency-vs-machines.dat' using 1:(\$6/1000000) with linespoints ls 2 title 'Coeus (n=1.2M)',\
'latency-vs-machines.dat' using 1:(\$7/1000000) with linespoints ls 3 title 'Coeus   (n=5M)',\
" > plot.plt
gnuplot plot.plt
epspdf $EPS
rm $EPS
