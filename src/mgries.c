#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mgries.h"

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

MGries *mgries_new(uns num_entries, uns threshold, Addr bankID){
  MGries *m = (MGries *) calloc (1, sizeof (MGries));
  m->entries  = (MGries_Entry *) calloc (num_entries, sizeof(MGries_Entry));
  m->threshold = threshold;
  m->num_entries = num_entries;
  m->bankID = bankID;

    printf("Number of entries in bank %llu is %d\n",m->bankID,m->num_entries);  


  return m;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void    mgries_reset(MGries *m){
  uns ii;

  //------ update global stats ------
  m->s_num_reset++;
  m->s_glob_spill_count += m->spill_count;

  //----- reset the structures ------
  m->spill_count       = 0;
  
  for(ii=0; ii < m->num_entries; ii++){
    m->entries[ii].valid = FALSE;
    m->entries[ii].addr  = 0;
    m->entries[ii].count = 0;
  }

}

////////////////////////////////////////////////////////////////////
// The rowAddr field is the row to be updated in the tracker
// returns TRUE if mitigation must be issued for the row
////////////////////////////////////////////////////////////////////

Flag  mgries_access(MGries *m, Addr rowAddr){
  Flag retval = FALSE;
  m->s_num_access++;

  //---- TODO: Access the tracker and install entry (update stats) if needed

  //1. Check if rowAddr present in tracker and increase count if yes.
  //2. If not, check if spill count == lowest of count in tracker
  //3. If no, then increase spill count.
  //4. If yes, then install address in tracker with count = spill_count+1

  int presentTracker = 0;
  unsigned int lowestCount = m->threshold;
  int lowestCountindx = -1;

  for(unsigned int i=0; i< m->num_entries ; i++ )
  {
      if(m->entries[i].count < lowestCount)
      {
        lowestCount = m->entries[i].count;
        lowestCountindx = i;
      }

      if(m->entries[i].valid && m->entries[i].addr == rowAddr)
      {
        m->entries[i].count++;

        if(m->entries[i].count >= m->threshold)
        {
          retval = TRUE;
          m->entries[i].count = 0;
        }

        presentTracker = 1;
        break;
      }    
  }

  //If addr not in tracker
  if(presentTracker == 0)
  {
    Flag installed = FALSE;

    //Install if invalid entries
    for(unsigned int i=0; i< m->num_entries ; i++)
    {
        if(!m->entries[i].valid)
        {
          m->entries[i].addr = rowAddr;
          m->entries[i].count = 1;
          m->entries[i].valid = TRUE;
          installed = TRUE;
          m->s_num_install++;
          break;
        }
    }

    if(installed)
    {

    }
    else 
    {
      if(m->spill_count == lowestCount) //replace in tracker
      {
        if(lowestCountindx != -1)
        {
            m->entries[lowestCountindx].addr = rowAddr;
            m->entries[lowestCountindx].count = m->spill_count+1;
            m->s_num_install++;
        }
      }
      else                              //increase spill count
      {
        //printf("Entering spill count\n");
        m->spill_count++;
      }
    }
  }

  //---- TODO: Decide if mitigation is to be issued (retval)
  //If count >= threshold/2, then issue mitigation
  
  if(retval==TRUE){
    m->s_mitigations++;
    //Call mitigation
  }

  return retval;
}

////////////////////////////////////////////////////////////////////
// print stats for the tracker
// DO NOT CHANGE THIS FUNCTION
////////////////////////////////////////////////////////////////////

void    mgries_print_stats(MGries *m){
    char header[256];
    sprintf(header, "MGRIES-%llu",m->bankID);

    printf("\n%s_NUM_RESET      \t : %llu",    header, m->s_num_reset);
    printf("\n%s_GLOB_SPILL_CT  \t : %llu",    header, m->s_glob_spill_count);
    printf("\n%s_NUM_ACCESS     \t : %llu",    header, m->s_num_access);
    printf("\n%s_NUM_INSTALL    \t : %llu",    header, m->s_num_install);
    printf("\n%s_NUM_MITIGATE   \t : %llu",    header, m->s_mitigations);

    printf("\n"); 
}
