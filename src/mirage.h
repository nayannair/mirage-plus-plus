#ifndef MIRAGE_H
#define MIRAGE_H

#include "mtrand.h"
#include "global_types.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "externs.h"
#include <vector>

//#define SKEW_SIZE total_assocs_per_skew*sets
//#define SET_SIZE total_assocs_per_skew


typedef struct dataEntry;

typedef struct tagEntry
{
    //Full Tag - 40 bits
    uns64 full_tag;  
    //Forward Ptr
    dataEntry* fPtr;
    //valid
    uns valid;
    // Dirty 
    uns dirty;

} tagEntry;

typedef struct dataEntry
{
    //Data
    uns Data;
    //Backward Ptr
    tagEntry* rPtr;
 
} dataEntry;

typedef struct tagStore{
  //Tag Store
  uns sets;
  uns base_assocs;
  uns skews;
  uns extra_assocs;
  uns total_assocs_per_skew;

  //Entry
  tagEntry* entries;
} tagStore;

typedef struct dataStore{
  //Data Store
  uns num_lines;
  Flag isFull;
  //Entry
  dataEntry* entries;
} dataStore;

typedef struct {
    uint64_t* entries;
} PrinceHashTable;

//dynamic tag pool
typedef struct dynTagEntry{
  uns64 setID;
  int skewID;
  tagEntry tag_entry;
} dynTagEntry;

typedef struct mirageCache{
  
  tagStore* TagStore;
  dataStore* DataStore;

  //Hash table for PRINCE cipher for all lineaddr in mem
  Addr* princeHashTable0;
  Addr* princeHashTable1;

  Addr skew_set_index_arr[NUM_SKEW];

  uns64 s_count; // number of accesses
  uns64 s_miss; // number of misses
  uns64 s_hits; // number of hits
  uns64 s_evict; // number of evictions from data store
  uns64 s_installs; // number of installs5

  //Stats for MIRAGE on a skew-set granularity
  uns64 m_installs[NUM_SKEW][NUM_SETS];
  uns64 m_hits[NUM_SKEW][NUM_SETS];
  uns64 m_miss[NUM_SKEW][NUM_SETS];
  uns64 m_access[NUM_SKEW][NUM_SETS];
  uns64 m_gle;
  uns64 m_sae;
  uns64 max_ways_used[NUM_SKEW][NUM_SETS];

  //seed for hash function per skew
  uns64 seed[NUM_SKEW];

  //dynamic tag pool
  std::vector<dynTagEntry> dynTagPool;
 
} mirageCache;


// Create new Mirage LLC
mirageCache *mirage_new(uns sets, uns base_assocs, uns skews );
// Access Mirage LLC
Flag mirage_access (mirageCache *c, Addr addr);
// Hash function 
Addr mirage_hash(uns seed, Addr addr, int skew);
// Cache install
void mirage_install (mirageCache *c, Addr addr);
// Select skew 
uns skewSelect(mirageCache *c, Addr addr, Flag* tagSAE);
//Global eviction
uns64 mirageGLE(mirageCache *c);

uint32_t allocateTag(mirageCache* c, int skewID, uns64 setID);


void mirage_print_stats(mirageCache *c, char *header);





#endif //MIRAGE_H
