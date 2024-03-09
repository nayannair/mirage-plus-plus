#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cra_ctr.h"

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

CraCtr *cra_ctr_new(uns num_ctrs, uns threshold){
  CraCtr *m = (CraCtr *) calloc (1, sizeof (CraCtr));
  m->counts  = (uns64 *) calloc (num_ctrs, sizeof(uns64));
  m->num_ctrs = num_ctrs;
  m->threshold = threshold;
  assert( ((num_ctrs / NUM_CTRS_PER_CL * 64) < 1024*1024*1024) \
    && "Counter Region cannot be more than 1GB\n");
  
  //---- TODO: B2 Initialize Counter Cache for in-DRAM counters
  /* m->ctr_cache = ctrcache_new(CTRCACHE_SIZE_KB*1024/LINESIZE/CTRCACHE_ASSOC,CTRCACHE_ASSOC);   */

  return m;
}

////////////////////////////////////////////////////////////////////
// The rowAddr field is the row to be updated in the tracker
// The *read_ctrval field is to be updated by the function, based on the ctr value read
// returns TRUE if mitigation must be issued for the row
////////////////////////////////////////////////////////////////////

Flag  cra_ctr_read(CraCtr *m, DRAM* d, Addr rowAddr, uns64 in_cycle, uns64* read_ctrval){
  Flag retval = FALSE;
  m->s_num_reads++;

  //---- TODO: B1 Calculate which cra_ctr is to be accessed
  
  //---- TODO: B2 Check if present in counter cache using cache_access() to avoid mem-access
  
  //---- TODO: B1 Issue memory access using dram_service()
  
  //---- TODO: B1 Read counter and decide if mitigation is to be issued (retval)

  //---- TODO: B2 Install if not present in counter cache using cache_install() and handle writebacks.
  
  if(retval==TRUE){
    m->s_mitigations++;
  }
  
  return retval;
}

void  cra_ctr_write(CraCtr *m, DRAM* d, Addr rowAddr, uns64 in_cycle, uns64 write_ctrval){
  m->s_num_writes++;

  //---- TODO: B1 Calculate which cra_ctr is to be accessed

  //---- TODO: B2 Check if present in counter cache using cache_access() to avoid mem-access, and mark dirty.
  
  //---- TODO: B1 Issue memory access using dram_service()
  
  //---- TODO: B1 Update counter value.

  //---- TODO: B2 Install if not present in counter cache using cache_install() and mark dirty, and handle writeback.
  
  return ;
}

////////////////////////////////////////////////////////////////////
// stats for tracker printed here
// DO NOT MODIFY THIS FUNCTION
////////////////////////////////////////////////////////////////////

void    cra_ctr_print_stats(CraCtr *m){
    char header[256];
    sprintf(header, "CRA");
    printf("\n%s_NUM_READS     \t : %llu",     header, m->s_num_reads);
    printf("\n%s_NUM_WRITES    \t : %llu",     header, m->s_num_writes);
    printf("\n%s_NUM_MITIGATE   \t : %llu",    header, m->s_mitigations);
    printf("\n"); 
}
