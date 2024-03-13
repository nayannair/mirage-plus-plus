#ifndef MIRAGE_H
#define MIRAGE_H

#include "global_types.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

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

typedef struct mirageCache{
  
  tagStore* TagStore;
  dataStore* DataStore;

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

} mirageCache;


// Create new Mirage LLC
mirageCache *mirageCache_new(uns sets, uns base_assocs, uns skews );
// Access Mirage LLC
Flag mirageCache_access (mirageCache *c, Addr addr);
// Hash function 
Addr mirage_hash(uns skew, Addr addr);
// Cache install
void mirageCache_install (mirageCache *c, Addr addr);
// Select skew 
uns skewSelect(mirageCache *c, Addr addr, Flag* tagSAE, Addr* skew_set_index);
//Global eviction
uns mirageGLE(mirageCache *c);

void mirage_print_stats(mirageCache *c, char *header);





#endif //MIRAGE_H