# shitty  GCC links against other hackrf.so..how to force to link against different one? Piece of shit!
#gcc [options] [source files] [object files] [-Ldir] -llibname [-o outfile]

#rm hackrf_transfer
#gcc hackrf_transfer.c -I ../src/  ../build/src/libhackrf.so.0.5.0 -o hackrf_transfer
#gcc file1.c -Llibs -lmylib -o outfile
rm hackrf_transfer 1>& /dev/null
EXPORT="export LD_LIBRARY_PATH=/home/mike/folder/projects/hackrf-2017.02.1_btle/host/libhackrf/build/src"
#gcc  -Wall -I ../src hackrf_transfer.c ../build/src/libhackrf.so.0.5.0 -o hackrf_transfer
$EXPORT && gcc  -Wall -I ../src hackrf_transfer.c -L/home/mike/folder/projects/hackrf-2017.02.1_btle/host/libhackrf/build/src -lhackrf -o hackrf_transfer
ldd hackrf_transfer
