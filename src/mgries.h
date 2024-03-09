#ifndef MGRIES_H
#define MGRIES_H

#include "global_types.h"


typedef struct MGries_Entry MGries_Entry;
typedef struct MGries MGries;

///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

struct MGries_Entry {
    Flag    valid;
    Addr    addr;
    uns     count;
};


struct MGries{
  MGries_Entry *entries;
  uns           num_entries;
  uns           threshold;
  Addr          bankID;
  uns           spill_count;

  uns64         s_num_reset;        //-- how many times was the tracker reset
  uns64         s_glob_spill_count; //-- what is the total spill_count over time

  //---- Update below statistics in mgries_access() ----
  uns64         s_num_access;  //-- how many times was the tracker called
  uns64         s_num_install; //-- how many times did the tracker install rowIDs 
  uns64         s_mitigations; //-- how many times did the tracker issue mitigation

};

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

MGries *mgries_new(uns entries, uns threshold, Addr bankID);
Flag    mgries_access(MGries *m, Addr addr);
void    mgries_reset(MGries *m);

void    mgries_print_stats(MGries *m);

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////

#endif // MGRIES_H
