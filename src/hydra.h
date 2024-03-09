#ifndef HYDRA_H
#define HYDRA_H

#include "global_types.h"
#include "dram.h"
#include "externs.h"


#define CTR_SIZE 2

typedef struct GCT_ctr GCT_ctr;
typedef struct RCT_ctr RCT_ctr;
typedef struct rcc_cache rcc_cache;

//4 dram reads/writes --> 64bytes * 4 = 256bytes
// 256B/2B(size of RCT counter) --> 128 rows in a grou

//Structure for group counting
typedef struct GCTEntry{
    uns64 count;
} GCTEntry;

typedef struct GCT_ctr{
    uns num_ctrs;
    uns threshold_g;
    GCTEntry* entries;
} GCT_ctr;


typedef struct RCTEntry{
    uns64 count;
} RCTEntry;

//Structure for row counting
typedef struct RCT_ctr{
    uns num_ctrs;
    uns threshold_r;
    uns threshold_g;
    RCTEntry* entries;

    //---- Update below statistics  ----
    uns64         s_num_reads;  //-- how many times was the tracker called
    uns64         s_num_writes; //-- how many times did the tracker install rowIDs 
    uns64         s_mitigations; //-- how many times did the tracker issue mitigation

} RCT_ctr;

//RCC structures
typedef struct rcc_cache_entry{
    Flag valid;
    uns64 count;
    Addr tag;
    Flag    dirty;
    uns64   last_access;

} rcc_cache_entry;

//Cache for row counting
typedef struct rcc_cache{
  rcc_cache_entry* entries;
  uns sets;
  uns assocs;

  uns64 s_count; // number of accesses
  uns64 s_miss; // number of misses
  uns64 s_evict; // number of evictions
} rcc_cache;

GCT_ctr* gct_ctr_new(uns num_ctrs, uns threshold);
Flag gct_ctr_access(GCT_ctr* m, RCT_ctr* r, rcc_cache* c, DRAM* d, Addr rowAddr, uns64 in_cycle);
RCT_ctr* rct_ctr_new(uns num_ctrs, uns threshold);
Flag rct_ctr_access(RCT_ctr* r, rcc_cache* c, DRAM* d, Addr rowAddr, uns64 in_cycle);
void rct_ctr_initialize(RCT_ctr* m, Addr rowAddr);


Flag rcc_cache_access(rcc_cache* m, DRAM* d, Addr rowAddr, uns64 in_cycle);
rcc_cache *rcc_cache_new(uns sets, uns assocs);
Addr    rcc_cache_install       (rcc_cache *c, Addr rowAddr);
uns     rcc_cache_get_index     (rcc_cache *c, Addr rowAddr);
uns     rcc_cache_find_victim   (rcc_cache *c, uns set);
uns     rcc_cache_find_victim_lru   (rcc_cache *c, uns set);
void    rct_ctr_print_stats(RCT_ctr *m);
Flag    rcc_cache_mark_dirty    (rcc_cache *c, Addr addr);


//Flag    ctrcache_probe         (Ctrcache *c, Addr addr);
//Flag    ctrcache_invalidate    (Ctrcache *c, Addr addr);

#endif // HYDRA_H
