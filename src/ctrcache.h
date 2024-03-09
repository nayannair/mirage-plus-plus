#ifndef CTRCACHE_H
#define CTRCACHE_H

#include "global_types.h"

////////////////////////////////////////////////////////////////////
// NOTE: ALL ACCESSES TO THE CACHE USE LINEADDR OF THE CACHELINE
////////////////////////////////////////////////////////////////////

typedef struct Ctrcache_Entry Ctrcache_Entry;
typedef struct Ctrcache Ctrcache;

//Size of CRA counter-cache
#define       CTRCACHE_SIZE_KB (32)
//Associativity of CRA counter-cache
#define       CTRCACHE_ASSOC   (8)

///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

struct Ctrcache_Entry {
    Flag    valid;
    Flag    dirty;
    Addr    tag;
    uns64   last_access;
};


struct Ctrcache{
  uns sets;
  uns assocs;
  Ctrcache_Entry *entries;

  uns64 s_count; // number of accesses
  uns64 s_miss; // number of misses
  uns64 s_evict; // number of evictions
};

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

Ctrcache *ctrcache_new(uns sets, uns assocs);
Flag    ctrcache_access        (Ctrcache *c, Addr addr);
Addr    ctrcache_install       (Ctrcache *c, Addr addr);
Flag    ctrcache_probe         (Ctrcache *c, Addr addr);
Flag    ctrcache_invalidate    (Ctrcache *c, Addr addr);
Flag    ctrcache_mark_dirty    (Ctrcache *c, Addr addr);
uns     ctrcache_get_index     (Ctrcache *c, Addr addr);

uns     ctrcache_find_victim   (Ctrcache *c, uns set);
uns     ctrcache_find_victim_lru   (Ctrcache *c, uns set);

void    ctrcache_print_stats(Ctrcache *c, char *header);
///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

#endif // CTRCACHE_H
