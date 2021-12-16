set terminal png
set termoption enhanced

set output "mutextest.png"

set title ""

set key left center

set xlabel "Antal iterationer"
set ylabel "Tid"

plot "mutex_test.txt" using 1:2 w linespoints title "pthread", \
"mutex_test.txt" using 1:3 w linespoints title "gthread"
