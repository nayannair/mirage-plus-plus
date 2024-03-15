 /*************************************************************************
 * File         : sim.c
 * Description  : Fast DRAM Rowhammer Mitigation Simulator
 *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "global_types.h"
#include "mcore.h"
#include "memsys.h"
#include "params.h"



/***************************************************************************
 * Globals 
 **************************************************************************/

MemSys      *memsys; 
OS          *os;
mirageCache *LLC;
MCore       *mcore[MAX_THREADS];



/***************************************************************************************
 * Functions
 ***************************************************************************************/

void print_dots()
{
  uns ii;
  uns LINE_INTERVAL = 50 *  DOT_INTERVAL;

  last_printdot_cycle = cycle;

  if(TRACE_LIMIT){
      if(memsys->s_totaccess >= TRACE_LIMIT){
	  for(ii=0; ii< NUM_THREADS; ii++){
	      MCore *c=mcore[ii];
	      c->done_inst_count  = c->inst_num;
	      c->done_cycle_count = c->cycle;
	      c->done_miss_count  = c->miss_count;
	      c->done = 1;
	  }
      }
  }
  
  if(!PRINT_DOTS){
      return;
  }

  if (cycle % LINE_INTERVAL ==0){
	printf("\n%4llu M\t", cycle/1000000);

	printf("(INST_M: ");
	for(ii=0; ii<NUM_THREADS; ii++){
	  uns my_inst = (uns)(mcore[ii]->inst_num/1000000);
	  printf("%5u,", my_inst);
	}
	printf(") ");
	fflush(stdout);
    }
    else{
	printf(".");
	fflush(stdout);
    }
    
}


/***************************************************************************************
 * Main
 ***************************************************************************************/
int main(int argc, char** argv)
{
  int   ii;
  Flag  all_cores_done=0;

  srand( time(NULL) );

  if (argc < 2) {
    die_usage();
  }
  
  read_params(argc, argv);
  
  //--------------------------------------------------------------------
  // -- Allocate the nest and cores
  //--------------------------------------------------------------------
  uns num_os_pages = (1024*(MEM_SIZE_MB-MEM_RSRV_MB))/(OS_PAGESIZE/1024);
  os = os_new(num_os_pages, num_threads);

  uns l3sets = (L3_SIZE_KB*1024)/(L3_ASSOC*LINESIZE);

  memsys = memsys_new(NUM_THREADS, RH_THRESHOLD_ACT);
  
  //LLC = mcache_new(l3sets, L3_ASSOC, L3_REPL);

  printf("Init Mirage LLC\n");
  // Instantiate Mirage LLC
  LLC = mirage_new(NUM_SETS,NUM_ASSOCS,NUM_SKEW);

  for(ii=0; ii<num_threads; ii++){
    mcore[ii] = mcore_new_m( memsys, os, LLC, addr_trace_filename[ii], ii);
  }
  
  srand(RAND_SEED);
  print_dots();

  //--------------------------------------------------------------------
  // -- Iterate through the traces by cycling all cores till done
  //--------------------------------------------------------------------
  
  while( ! (all_cores_done) ){
    all_cores_done=1;

    for(ii=0; ii<num_threads; ii++){

      mcore_cycle(mcore[ii]);
      all_cores_done &= mcore[ii]->done;

    }
    

    if (cycle - last_printdot_cycle >= DOT_INTERVAL){
      print_dots();
    }
  
    cycle+= CLOCK_INC_FACTOR;
  }
    
  //--------------------------------------------------------------------
  // -- Print statistics
  //--------------------------------------------------------------------
  
  printf("\n\n");
   
  printf("------------------------------------\n");
  printf("------  DRAM.RH.SIM.V1.1  ----------\n");
  printf("------------------------------------\n");

  for(ii=0; ii<num_threads; ii++){
    mcore_print_stats(mcore[ii]);
  }

  //mcache_print_stats(LLC, (char*) "L3CACHE");
  mirage_print_stats(LLC, (char*) "MIRAGE_LLC");
  memsys_print_stats(memsys);

  os_print_stats(os);

  
  uns64 sum_done_cycles=0;
  for(ii=0; ii<num_threads; ii++){
    sum_done_cycles+=(uns64)(mcore[ii]->done_cycle_count);
  }
  uns64 avg_done_cycles=(uns64)(sum_done_cycles)/(uns64)(num_threads);

  printf("\nSYS_CYCLES      \t : %llu", cycle);
  printf("\nAVG_CORE_CYCLES \t : %llu", avg_done_cycles);

  printf("\n\n\n");

  return 0;
}






