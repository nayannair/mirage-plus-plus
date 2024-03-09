#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "global_types.h"
#include "hydra.h"

GCT_ctr* gct_ctr_new(uns num_ctrs, uns threshold){
  GCT_ctr *m = (GCT_ctr *) calloc (1, sizeof (GCT_ctr));
  m->entries  = (GCTEntry*) calloc (num_ctrs, sizeof(uns64));

  m->num_ctrs = num_ctrs;
  m->threshold_g = 0.8*threshold;

  printf("Number of cntrs in GCT: %d\n", num_ctrs);
  printf("Threshold of GCT: %d\n", m->threshold_g);

  for(unsigned int i=0; i<num_ctrs; i++)
  {
    m->entries[i].count = 0;
  }
  return m;
}

//Increment the group count
//log2(number of ctrs in GCT) = 14bits.
Flag gct_ctr_access(GCT_ctr* m, RCT_ctr* r, rcc_cache* c, DRAM* d, Addr rowAddr, uns64 in_cycle){

    Flag retval = FALSE;
    Addr index = rowAddr & 0x00003FFF; //Extract 14 LSB bits

    //printf("Row Addr: %llu, Index: %llu\n",rowAddr,index);

    if(m->entries[index].count < m->threshold_g)
    {
        //printf("Entered GCT entry\n");
        m->entries[index].count++;
        //printf("Index %llu -> count: %d\n",index,m->entries[index].count);

        //If GCT count is threshold_g, initialize RCT entries
        if(m->entries[index].count == m->threshold_g)
        {
           
            //Issue 4 reads/writes to DRAM
            Addr lineAddr;
            Addr offset = 16106127360; //15GB offset
            //lineAddr = rowAddr*LINESIZE; //Check this
            index = rowAddr & 0x00003FFF;  //All rows with same GCT index will map to this row Addr
            Addr top_bits = rowAddr >> 14;
            lineAddr = (offset+ index*2*256 + top_bits) / LINESIZE;
            //lineAddr = (offset)/LINESIZE + (index >> 5); //Check this

            lineAddr = rowAddr * (MEM_PAGESIZE/LINESIZE);
            DRAM_ReqType type=DRAM_REQ_RD;
            double num_lineburst=1.0; // one cache line
            ACTinfo *act_info;
            
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            r->s_num_reads +=4;

            type= DRAM_REQ_WB;
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
            r->s_num_writes +=4;

            rct_ctr_initialize(r,rowAddr);
            rcc_cache_install(c,rowAddr);
            rcc_cache_mark_dirty(c,rowAddr);
        }
    }
    else
    {
        //Access RCT
        if (rct_ctr_access(r,c,d,rowAddr,in_cycle))
        {
          retval = TRUE;
        }
    }

    return retval;
}

RCT_ctr* rct_ctr_new(uns num_ctrs, uns threshold){
  RCT_ctr *m = (RCT_ctr *) calloc (1, sizeof (RCT_ctr));
  m->entries  = (RCTEntry*) calloc (num_ctrs, sizeof(uns64));
  m->num_ctrs = num_ctrs;
  m->threshold_r = threshold;
  m->threshold_g = 0.8*threshold;

  m->s_mitigations = 0;
  m->s_num_reads = 0;
  m->s_num_writes = 0;

  for(int i=0; i<num_ctrs; i++)
  {
    m->entries[i].count = 0;
  }
  return m;
}

void rct_ctr_initialize(RCT_ctr* m, Addr rowAddr)
{
    Addr key = rowAddr & 0x00003FFF; 

    for(Addr rowAddrs = 0; rowAddrs < ((MEM_SIZE_MB*1024*1024)/MEM_PAGESIZE) ; rowAddrs++)
    {
        if( (rowAddrs & 0x00003FFF) == key)
        {
                //printf("Row Addr %llu is set in RCT\n",rowAddrs);
                m->entries[rowAddrs].count = m->threshold_g;
        }
    }
}

Flag rct_ctr_access(RCT_ctr* r, rcc_cache* c, DRAM* d, Addr rowAddr, uns64 in_cycle)
{
    Flag retval = FALSE;
    //Check for line in RCC first

    //Check for mitigations
    if(r->entries[rowAddr].count ==  r->threshold_r -1)
    {
        //printf("Mitigation issued for rowAddr %llu\n",rowAddr);
        retval = TRUE;
        r->s_mitigations++;
        r->entries[rowAddr].count = 0;
    }
    
    {
        //If Hit
      if (rcc_cache_access(c,d,rowAddr,in_cycle) )
      { 
        r->entries[rowAddr].count++;
        //printf("Hit in cache! Row Addr: %llu, count: %d\n",rowAddr,r->entries[rowAddr].count);
      }
      else      //If Miss fetch from DRAM.
      {
        //printf("Miss in cache. Fecthing from DRAM\n");
        Addr lineAddr;
        Addr offset = 16106127360; //15GB offset
        //lineAddr = (offset + rowOffset)/(4*LINESIZE); //Check this
        //lineAddr = (offset + rowAddr*CTR_SIZE)/LINESIZE; //Check this 
        
        Addr index = rowAddr & 0x00003FFF;  //All rows with same GCT index will map to this row Addr
        Addr top_bits = rowAddr >> 14;
        lineAddr = (offset+ index*2*256 + top_bits) / LINESIZE;
        //lineAddr = (offset)/LINESIZE + (index>>5); //Check this

        DRAM_ReqType type=DRAM_REQ_RD;
        double num_lineburst=1.0; // one cache line
        ACTinfo *act_info;
        
        dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
        r->s_num_reads++;


        Addr evictedAddr = rcc_cache_install(c,rowAddr);
        rcc_cache_mark_dirty(c,rowAddr);

        //Increment counter
        r->entries[rowAddr].count++;

        //lineAddr = (offset + evictedAddr*CTR_SIZE)/LINESIZE; //Check this
        index = evictedAddr & 0x00003FFF;  //All rows with same GCT index will map to this row Addr
        //lineAddr = (offset)/LINESIZE + (index >> 5); //Check this
        top_bits = evictedAddr >> 14;
        lineAddr = (offset+ index*2*256 + top_bits) / LINESIZE;
        
        type=DRAM_REQ_WB;
        
        if(evictedAddr != -1)
        {
          //printf("Line evicted from cache\n");
          dram_service(d,lineAddr,type,num_lineburst,in_cycle,act_info);
          r->s_num_writes++;
          r->s_num_reads++;
        }
      }
    }
    return retval;
}

rcc_cache* rcc_cache_new(uns sets, uns assocs)
{
  rcc_cache *c = (rcc_cache *) calloc (1, sizeof (rcc_cache));
  c->sets    = sets;
  c->assocs  = assocs;

  c->entries  = (rcc_cache_entry*) calloc (sets * assocs, sizeof(rcc_cache_entry));

  for(int i =0; i < sets*assocs; i++)
  {
    c->entries->count =0;
    c->entries->valid = FALSE;
    c->entries->tag = 0;
    c->entries->dirty = FALSE;
  }

  return c;
}

// 2 mil rows --> 21 bits.
// 21 - 9 bits (512 sets) = 12bits (tag)
// 512*16*2B = 16kB 
//Using full tag here, shouldn't make a difference

Flag rcc_cache_access(rcc_cache* m, DRAM* d, Addr rowAddr, uns64 in_cycle)
{
  Addr  tag  = rowAddr; // after removing index bits
  uns   set  = rcc_cache_get_index(m,rowAddr);
  uns   start = set * m->assocs;
  uns   end   = start + m->assocs;
  uns   ii;
    
  m->s_count++;
    
  for (ii=start; ii<end; ii++){
    rcc_cache_entry *entry = &m->entries[ii];
    
    if(entry->valid && (entry->tag == tag))
      {
        entry->last_access  = m->s_count;
        return HIT;
      }
  }
  m->s_miss++;
  return MISS;
}

uns rcc_cache_get_index(rcc_cache *c, Addr addr){
  uns retval;
  retval=addr%c->sets;
  return retval;
}

Addr rcc_cache_install (rcc_cache *c, Addr rowAddr)
{
  Addr dirty_evict_lineaddr = -1;
  Addr  tag  = rowAddr; // full tags
  uns   set  = rcc_cache_get_index(c,rowAddr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii, victim;
  
  Flag update_lrubits=TRUE;
  
  rcc_cache_entry *entry;

  for (ii=start; ii<end; ii++){
    entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag)){
      printf("Installed entry already with addr:%llx present in set:%u\n", rowAddr, set);
      exit(-1);
    }
  }
  
  // find victim and install entry
  victim = rcc_cache_find_victim(c, set);

  //Check if victim is dirty
  if(c->entries[victim].valid && c->entries[victim].dirty) //Data is always dirty
    dirty_evict_lineaddr = c->entries[victim].tag;

  entry = &c->entries[victim];

  if(entry->valid){
    c->s_evict++;
  }
  
  //put new information in
  entry->tag   = tag;
  entry->valid = TRUE;
  entry->dirty = FALSE;  //Data is always dirty
  
  if(update_lrubits){
    entry->last_access  = c->s_count;   
  }

  return dirty_evict_lineaddr ;
}

uns rcc_cache_find_victim (rcc_cache *c, uns set)
{
  int ii;
  int start = set   * c->assocs;    
  int end   = start + c->assocs;    

  //search for invalid first
  for (ii = start; ii < end; ii++){
    if(!c->entries[ii].valid){
      return ii;
    }
  }

  return rcc_cache_find_victim_lru(c, set);
}

uns rcc_cache_find_victim_lru (rcc_cache *c,  uns set)
{
  uns start = set   * c->assocs;    
  uns end   = start + c->assocs;    
  uns lowest=start;
  uns ii;


  for (ii = start; ii < end; ii++){
    if (c->entries[ii].last_access < c->entries[lowest].last_access){
      lowest = ii;
    }
  }

  return lowest;
}

void    rct_ctr_print_stats(RCT_ctr *m){
    char header[256];
    sprintf(header, "HYDRA");
    printf("\n%s_NUM_READS     \t : %llu",     header, m->s_num_reads);
    printf("\n%s_NUM_WRITES    \t : %llu",     header, m->s_num_writes);
    printf("\n%s_NUM_MITIGATE   \t : %llu",    header, m->s_mitigations);
    printf("\n"); 
}


Flag    rcc_cache_mark_dirty    (rcc_cache *c, Addr addr)
{
  Addr  tag  = addr; // full tags
  uns   set  = rcc_cache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii;

  for (ii=start; ii<end; ii++){
    rcc_cache_entry *entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag))
      {
	entry->dirty = TRUE;
	return TRUE;
      }
  }
  
  return FALSE;
}