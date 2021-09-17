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
set terminal postscript eps enhanced colour size 8, 4 font 'Times-Roman,45'
set output '$GRAPHNAME.eps'
set boxwidth 0.5
set xtics nomirror font 'Times-Roman, 50'
set ytics nomirror font 'Times-Roman, 50'
set ylabel \"Latency (seconds)\" font 'Times-Roman,55' offset -0.7,0.0
set xlabel \"Number of documents (n)\" font 'Times-Roman,55'
set key at 1.5,5.1 font 'Times-Roman, 47' spacing 1.05
set border 3 lw 4
set yrange [0:5]
set style data histograms
set bmargin 3.5
set style histogram rowstack
set style line 1 lc rgb '#006a4e' dt 1 lt 1 lw 9 pt 7 pi -1 ps 2.5
set style line 2 lc rgb '#006a4e' dt 1 lt 1 lw 5 pt 5 pi -1 ps 2.5
set style line 3 lc rgb '#006a4e' dt 1 lt 1 lw 7 pt 9 pi -1 ps 2.5
set style line 4 lc rgb '#8d8d8d' dt 1 lt 1 lw 7 pt 6 pi -1 ps 2.5
set style line 5 lc rgb '#8d8d8d' dt 1 lt 1 lw 7 pt 4 pi -1 ps 2.5
set style line 6 lc rgb '#8d8d8d' dt 1 lt 1 lw 6 pt 8 pi -1 ps 2.5
set style line 7 lc rgb '#8d8d8d' dt 1 lt 1 lw 7 pt 8 pi -1 ps 3.5
set style fill pattern border -1
#set style fill noborder

plot\
'latency-vs-rounds.dat' using (\$2/1000000):xtic(1) fs pattern 0 lc rgb '#006a4e' lw 5 t 'Metadata-retrieval',\
     '' using (\$3/1000000) fs solid 0.7 lc rgb '#006a4e' lw 4 t 'Document-retrieval',\
    '' using 0:((\$2+\$3+\$4)/1000000 + 0.3):(sprintf(\"%3.2f\", (\$2+\$3+\$4)/1000000)) with labels notitle, '' using (\$4/1000000) fs pattern 5 lc rgb '#6495ed' lw -1 lt 2 t 'Query-scoring'
" > plot.plt
gnuplot plot.plt
epspdf $EPS
rm $EPS
