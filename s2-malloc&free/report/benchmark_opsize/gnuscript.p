set terminal png
set termoption enhanced

set output "all3_sizeops_zoom.png"

set title ""

set key left center

set xlabel "Antal operationer"
set yrange[0:500]
set ylabel "Medellängd av fria block"

plot "part5.dat" using 1:2 w linespoints title "Förbättrad", \
"part4.dat" using 1:2 w linespoints title "Med merge", \
"part3.dat" using 1:2 w linespoints title "Utan merge"
