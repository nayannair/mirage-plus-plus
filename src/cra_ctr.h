#ifndef CRA_CTR_H
#define CRA_CTR_H

#include "global_types.h"
#include "ctrcache.h"
#include "dram.h"

typedef struct CraCtr CraCtr;
#define NUM_CTRS_PER_CL (32) 

///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


struct CraCtr{
  uns64         *counts;
  uns           num_ctrs;
  uns           threshold; //-- after which mitigation is to be issued 
  Ctrcache      *ctr_cache;//-- to cache in-DRAM CRA counters.

  //---- Update below statistics in cra_ctr_read() and cra_ctr_write() ----
  uns64         s_num_reads;  //-- how many times was the tracker called
  uns64         s_num_writes; //-- how many times did the tracker install rowIDs 
  uns64         s_mitigations; //-- how many times did the tracker issue mitigation

};

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

CraCtr *cra_ctr_new(uns num_ctrs,  uns threshold);
Flag    cra_ctr_read(CraCtr *m, DRAM* d, Addr rowAddr, uns64 in_cycle, uns64* read_ctrval);
void    cra_ctr_write(CraCtr *m, DRAM* d, Addr rowAddr, uns64 in_cycle, uns64 write_ctrval);
void    cra_ctr_print_stats(CraCtr *m);

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

#endif // CRA_CTR_H
