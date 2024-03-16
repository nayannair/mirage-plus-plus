RM        := /bin/rm -rf
SIM       := ./sim_6
CC        := g++
CFLAGS    := -lz -O2 -lm  -W -Wall -Wno-deprecated -Wno-unknown-warning-option -Wno-self-assign -Wno-unused-parameter -Wno-unused-but-set-variable
DFLAGS    := -pg -g
PFLAGS    := -pg




all:  folders traces
	${CC} ${CFLAGS} dram.c  mgries.c cra_ctr.c ctrcache.c scoreboard.c mcore.c os.c  mcache.c sim.c memsys.c  hash_lib.c malloc_lib.c hydra.c mirage.c prince.c -o ${SIM}

folders: 
	mkdir -p ../RESULTS

traces: 
	if test -d ../TRACES; then echo "Traces exist" ; else echo "Downloading traces..." ; wget -O traces.tar.gz "https://www.dropbox.com/s/77pgznatvbr9hw9/traces.tar.gz?dl=1" ; mv traces.tar.gz ../. ;  cd ../ ; tar -zxvf traces.tar.gz ; rm -rf traces.tar.gz; fi

clean: 
	$(RM) ${SIM} *.o
