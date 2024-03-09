
#####################################################################################################
## Run one benchmark
#####################################################################################################

mkdir -p ../RESULTS/BASELINE.4C.8MB; ./sim ../TRACES/bwaves_06.mat.gz ../TRACES/bwaves_06.mat.gz ../TRACES/bwaves_06.mat.gz ../TRACES/bwaves_06.mat.gz > ../RESULTS/BASELINE.4C.8MB/bwaves_06.res ; 

#####################################################################################################
## Run all benchmarks
#####################################################################################################


suite="lab1"
fw=7
#fw    - number of simulations to run in parallel
#sutie - set of benchmarks to simulate (defined in bench_common.pl

perl runall.pl --w $suite --f $fw  --d "../RESULTS/BASELINE.4C.8MB" ;

