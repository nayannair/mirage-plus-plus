#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ctrcache.h"

////////////////////////////////////////////////////////////////////
// NOTE: ALL ACCESSES TO THE CACHE USE LINEADDR OF THE CACHELINE
////////////////////////////////////////////////////////////////////

Ctrcache *
ctrcache_new(uns sets, uns assocs)
{
  Ctrcache *c = (Ctrcache *) calloc (1, sizeof (Ctrcache));
  c->sets    = sets;
  c->assocs  = assocs;

  c->entries  = (Ctrcache_Entry *) calloc (sets * assocs, sizeof(Ctrcache_Entry));
  return c;
}

////////////////////////////////////////////////////////////////////
// the input addr field is the lineaddress = address/cache_line_size
// Returns HIT(1)  or MISS (0) based on if addr is in the cache  not.
////////////////////////////////////////////////////////////////////

Flag ctrcache_access (Ctrcache *c, Addr addr)
{
  Addr  tag  = addr; // full tags
  uns   set  = ctrcache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii;
    
  c->s_count++;
    
  for (ii=start; ii<end; ii++){
    Ctrcache_Entry *entry = &c->entries[ii];
    
    if(entry->valid && (entry->tag == tag))
      {
	entry->last_access  = c->s_count;
	return HIT;
      }
  }

  c->s_miss++;
  return MISS;
}

///////////////////////////////////////////////////////////////////////////////////
// The input addr field is the lineaddress = address/cache_line_size
// Returns dirty_evict_lineaddr in case of a dirty eviction, otherwise returns -1. 
///////////////////////////////////////////////////////////////////////////////////

Addr ctrcache_install (Ctrcache *c, Addr addr)
{
  Addr dirty_evict_lineaddr = -1;
  Addr  tag  = addr; // full tags
  uns   set  = ctrcache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii, victim;
  
  Flag update_lrubits=TRUE;
  
  Ctrcache_Entry *entry;

  for (ii=start; ii<end; ii++){
    entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag)){
      printf("Installed entry already with addr:%llx present in set:%u\n", addr, set);
      exit(-1);
    }
  }
  
  // find victim and install entry
  victim = ctrcache_find_victim(c, set);

  //Check if victim is dirty
  if(c->entries[victim].valid && c->entries[victim].dirty)
    dirty_evict_lineaddr = c->entries[victim].tag;

  entry = &c->entries[victim];

  if(entry->valid){
    c->s_evict++;
  }
  
  //put new information in
  entry->tag   = tag;
  entry->valid = TRUE;
  entry->dirty = FALSE;
  
  if(update_lrubits){
    entry->last_access  = c->s_count;   
  }

  return dirty_evict_lineaddr ;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

Flag    ctrcache_probe    (Ctrcache *c, Addr addr)
{
  Addr  tag  = addr; // full tags
  uns   set  = ctrcache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii;

  for (ii=start; ii<end; ii++){
    Ctrcache_Entry *entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag))
      {
	return TRUE;
      }
  }
  
  return FALSE;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

Flag    ctrcache_invalidate    (Ctrcache *c, Addr addr)
{
  Addr  tag  = addr; // full tags
  uns   set  = ctrcache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii;

  for (ii=start; ii<end; ii++){
    Ctrcache_Entry *entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag))
      {
	entry->valid = FALSE;
	return TRUE;
      }
  }
  
  return FALSE;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

Flag    ctrcache_mark_dirty    (Ctrcache *c, Addr addr)
{
  Addr  tag  = addr; // full tags
  uns   set  = ctrcache_get_index(c,addr);
  uns   start = set * c->assocs;
  uns   end   = start + c->assocs;
  uns   ii;

  for (ii=start; ii<end; ii++){
    Ctrcache_Entry *entry = &c->entries[ii];
    if(entry->valid && (entry->tag == tag))
      {
	entry->dirty = TRUE;
	return TRUE;
      }
  }
  
  return FALSE;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

uns ctrcache_find_victim (Ctrcache *c, uns set)
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

  return ctrcache_find_victim_lru(c, set);
}


////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

uns ctrcache_find_victim_lru (Ctrcache *c,  uns set)
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

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

uns ctrcache_get_index(Ctrcache *c, Addr addr){
  uns retval;
  retval=addr%c->sets;
  return retval;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

void  ctrcache_print_stats(Ctrcache *c, char *header){
  double missrate = 100.0 * (double)c->s_miss/(double)c->s_count;
  
  printf("\n%s_ACCESS       \t : %llu",  header,  c->s_count);
  printf("\n%s_MISS         \t : %llu",  header,  c->s_miss);
  printf("\n%s_MISSRATE     \t : %6.3f", header,  missrate);
  printf("\n");
}

