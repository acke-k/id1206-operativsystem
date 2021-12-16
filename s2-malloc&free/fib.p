set terminal png
set termoption enhanced

set output "fib.png"
set title ""

set key left center

set xlabel "freelist length"

set ylabel "antal operationer"

plot "sem2.dat" u 2:1 w linespoints title "sem2"

