#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "externs.h"
#include "memsys.h"
#include "mcore.h"


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

MemSys *memsys_new(uns num_threads, uns64 rh_threshold){
    MemSys *m = (MemSys *) calloc (1, sizeof (MemSys));
    m->num_threads    = num_threads;
    m->rh_threshold   = rh_threshold;

    // init main memory DRAM
    m->mainmem = dram_new(MEM_SIZE_MB*1024*1024, MEM_CHANNELS, MEM_BANKS, MEM_PAGESIZE, 
			  MEM_T_ACT, MEM_T_CAS, MEM_T_RAS, MEM_T_RP, MEM_T_BURST);
    sprintf(m->mainmem->name, "MEM");
    m->lines_in_mainmem_rbuf = MEM_PAGESIZE/LINESIZE; // static

    printf("Number of lines in row = %d\n", MEM_PAGESIZE/LINESIZE);
    //-- TASK A --
    //-- TODO: Initialize Misra Gries Tracker

    //-- Think? How many trackers do you need? 
    //HINT: Look at mgries_new(..) func or memsys_print_stats func.    
    //int num_mg_trackers = m->mainmem->num_banks;
    //m->mgries_t = (MGries**) calloc(num_mg_trackers,sizeof(MGries*)); 

    //-- Think? What should be the threshold for mgries_new? 
    //int mg_threshold = m->rh_threshold/2;

    //int num_mg_entries = 64*1000000/(0.25*146);
    //printf("num_mg_entries = %d\n",num_mg_entries);
    //printf("mg_threshold = %d\n",mg_threshold);
    //num_mg_entries = num_mg_entries/mg_threshold;

    //for(int i=0; i<num_mg_trackers; i++){ 
    //  m->mgries_t[i] = mgries_new(num_mg_entries,mg_threshold,i);   
    //} 

    //-- TASK B --
    //-- TODO: Initialize CRA Counters in DRAM

    //-- Think? How many counters needed? What should be the threshold for cra_ctr_new()?
    /* m->cra_t = (CraCtr*)  calloc(1,sizeof(CraCtr)); */
    /* m->cra_t = cra_ctr_new(...);    */
    
    //GCT Table for Hydra
    int threshold = rh_threshold/2;
    int num_ctrs = ((MEM_SIZE_MB*1024*1024)/MEM_PAGESIZE)/128; //128 rows in one group
    m->gct_ctr_t = (GCT_ctr*) calloc (1, sizeof (GCT_ctr));
    m->gct_ctr_t = gct_ctr_new(num_ctrs,threshold);

    //RCT Table for Hydra
    int num_ctrs_r = (MEM_SIZE_MB*1024*1024)/MEM_PAGESIZE;
    m->rct_ctr_t = (RCT_ctr*) calloc (1, sizeof (RCT_ctr));
    m->rct_ctr_t = rct_ctr_new(num_ctrs_r,threshold);

    //RCC cache for Hydra //chefck this
    uns num_sets = 256;
    uns assocs = 16;
    m->rcc_cache_t = (rcc_cache*) calloc (1, sizeof (rcc_cache));
    m->rcc_cache_t = rcc_cache_new(num_sets,assocs);

    return m;
}


//////////////////////////////////////////////////////////////////////////
// NOTE: ACCESSES TO THE MEMORY USE LINEADDR OF THE CACHELINE ACCESSED
//////////////////////////////////////////////////////////////////////////

uns64 memsys_access(MemSys *m, Addr lineaddr,  uns tid, uns64 in_cycle){
  uns64 total_delay=0;
 uns64 memdelay=0;

  memdelay=memsys_dram_access(m, lineaddr, in_cycle, &(m->dram_acc_info));
  total_delay=memdelay;

  /*******Edit ***********/
  //Access the M-Gries Tracker --- Check this

  //Debug
  //printf("Tracker Access Info: Cycle: %llu, Bank ID - %llu\n", in_cycle,m->dram_acc_info.bankID);
  
  //if(m->dram_acc_info.isACT && (mgries_access(m->mgries_t[m->dram_acc_info.bankID],m->dram_acc_info.rowID)))
  //{
  //  memsys_rh_mitigate(m,m->dram_acc_info.rowID,in_cycle);
  //}
  /*******Edit ***********/


  Addr rowAddr = lineaddr >> 7;
  //Access GCT
  if(m->dram_acc_info.isACT && gct_ctr_access(m->gct_ctr_t,m->rct_ctr_t,m->rcc_cache_t,m->mainmem,rowAddr,in_cycle))
  {
      memsys_rh_mitigate(m,rowAddr,in_cycle);
  }

  // stat collection happens below this line
  m->s_totaccess++;
  m->s_totdelaysum+=total_delay;

  // return delay
  return total_delay;
}



////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

void memsys_print_stats(MemSys *m)
{
    char header[256];
    sprintf(header, "MSYS");

    uns64 avg_delay=0;
    if(m->s_totaccess){
      avg_delay=m->s_totdelaysum/m->s_totaccess;
    }

    dram_print_stats(m->mainmem);

    printf("\n%s_TOT_ACCESS      \t : %llu",    header, m->s_totaccess);
    printf("\n%s_AVG_DELAY       \t : %llu",    header, avg_delay);
    printf("\n%s_RH_TOT_MITIGATE \t : %llu",    header, m->s_tot_mitigate);
    printf("\n");
    
    //Printing other stats
    long long unsigned total_reset =0;
    long long unsigned total_installs =0;
    long long unsigned total_g_spills =0;

    if(m->mgries_t)
      for(uns i=0; i< m->mainmem->num_banks; i++){
        total_reset += m->mgries_t[i]->s_num_reset;
        total_installs += m->mgries_t[i]->s_num_install;
        total_g_spills += m->mgries_t[i]->s_glob_spill_count;
      }

    //printf("\n%s_TOT_INSTALLS     \t : %llu",    header, total_installs);
    //printf("\n%s_NUM_RESETS       \t : %llu",    header, total_reset);
    //printf("\n%s_NUM_GLOBAL_SPILL_CNT      \t : %llu",    header, total_g_spills);


    if(m->mgries_t)
      for(uns i=0; i< m->mainmem->num_banks; i++){
	    mgries_print_stats(m->mgries_t[i]);
      }

    if(m->cra_t)
      cra_ctr_print_stats(m->cra_t);

    if(m->rct_ctr_t)
      rct_ctr_print_stats(m->rct_ctr_t);
}


//////////////////////////////////////////////////////////////////////////
// NOTE: ACCESSES TO THE MEMORY USE LINEADDR OF THE CACHELINE ACCESSED
//-- for input ACTinfo* act_info: if the pointer is not NULL, then *act_info is updated, else not 
////////////////////////////////////////////////////////////////////
uns64  memsys_dram_access(MemSys *m, Addr lineaddr, uns64 in_cycle, ACTinfo *act_info){
  DRAM_ReqType type=DRAM_REQ_RD;
  double burst_size=1.0; // one cache line
  uns64 delay=0;

  delay += dram_service(m->mainmem, lineaddr, type, burst_size, in_cycle, act_info);

  return delay;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void  memsys_rh_mitigate(MemSys *m, Addr rowID, uns64 in_cycle){

  DRAM_ReqType type=DRAM_REQ_RD;
  double burst_size=1.0; // one cache line
  uns64 delay=0;

  //-- neighbors --
  Addr row_prev_lineaddr, row_next_lineaddr ; 
  dram_get_neighbor_lineaddr(m->mainmem, rowID, &row_prev_lineaddr, &row_next_lineaddr);

  //-- Activate neighbors --
  if(row_prev_lineaddr != (Addr)-1){
    delay += dram_service(m->mainmem, row_prev_lineaddr, type, burst_size, in_cycle, NULL);
  }
  if(row_next_lineaddr != (Addr)-1){
    delay += dram_service(m->mainmem, row_next_lineaddr, type, burst_size, in_cycle, NULL);
  }

  //Update statistic
  m->s_tot_mitigate++;
  return ;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

