#include "mirage.h"
#include "prince.h"
#include <vector>
//TODO
// Power of 2 choices --> random skew selection only works for 2 skews. Generalize for > 2
// implement write/read functionality and dirty writebacks

std::vector<PrinceHashTable*> PHT (NUM_SKEW);

MTRand *mtrand=new MTRand(42);

mirageCache *mirage_new(uns sets, uns base_assocs, uns skews )
{
    //mtrand->seed(10);

    mirageCache* c = (mirageCache*) calloc(1,sizeof(mirageCache));
    c->DataStore = (dataStore*) calloc(1,sizeof(dataStore));
    c->TagStore = (tagStore*) calloc(1,sizeof(tagStore));
    c->TagStore->base_assocs = base_assocs;
    c->TagStore->sets = sets;
    printf("Total Sets = %d\n",c->TagStore->sets);

    c->TagStore->extra_assocs = EXTRA_WAYS; //75% extra ways
    c->TagStore->total_assocs_per_skew = (base_assocs + c->TagStore->extra_assocs);
    printf("Total Assocs per skew = %d\n",c->TagStore->total_assocs_per_skew);
    c->TagStore->skews = skews;

    int num_entries_tag = skews*sets*(c->TagStore->total_assocs_per_skew);
    int num_entries_data = skews*sets*base_assocs;
    printf("num tags %d\n",num_entries_tag);
    printf("num data %d\n",num_entries_data);
    
    c->TagStore->entries = (tagEntry*) calloc(num_entries_tag,sizeof(tagEntry));        // How to index skews
    c->DataStore->entries = (dataEntry*) calloc(num_entries_data,sizeof(dataEntry));    // sum*assocs = lines
    c->DataStore->num_lines = skews*sets*base_assocs;

    //Instantiating Prince Hash Table
    for (uns64 i=0; i<NUM_SKEW; i++)
    {
        PHT[i] = (PrinceHashTable*)malloc(sizeof(PrinceHashTable));
        PHT[i]->entries = (uint64_t*)calloc(TABLE_SIZE,sizeof(uint64_t));
    }
    
    for(uns64 i=0; i < TABLE_SIZE; i++)
    {
        for (uns64 j=0; j<NUM_SKEW; j++)
        {
            PHT[j]->entries[i] = -1;
        }
    }  

    c->s_count = 0; // number of accesses
    c->s_miss  = 0; // number of misses
    c->s_hits  = 0; // number of hits
    c->s_evict = 0; // number of evictions from data store
    c->s_installs = 0; 
    c->m_sae = 0;
    c->m_gle = 0;

    
    for(int i=0; i < NUM_SKEW; i++)
    {
        c->skew_set_index_arr[i] = -1;
        //Initialize seed
        c->seed[i] = rand();
        
        for(int j=0; j < NUM_SETS; j++)
        {
            c->m_installs[i][j] = 0;
            c->m_hits[i][j] = 0;
            c->m_miss[i][j] = 0;
            c->m_access[i][j] = 0;
        }
    }

    for(int i =0; i<num_entries_data; i++)
    {
        c->DataStore->entries[i].rPtr = NULL;  
    }

    for(int i =0; i<num_entries_tag; i++)
    {
        c->TagStore->entries[i].fPtr = NULL;  
        c->TagStore->entries[i].valid = 0;
        c->TagStore->entries[i].dirty = 0;
        c->TagStore->entries[i].full_tag = 0;
    }

    c->DataStore->isFull = FALSE;

    return c;
}

// Addr is line addr = addr / CACHE_LINESIZE
Flag mirage_access (mirageCache *c, Addr addr)
{
    c->s_count++;
    // Access skews in parallel
    for(int i =0 ; i<c->TagStore->skews; i++)
    {
        
        c->skew_set_index_arr[i] = mirage_hash(c->seed[i],addr,i);
        //assert (skew_set_index == c->princeHashTable0[addr]);
        //assert (skew_set_index == c->princeHashTable1[addr]);

        Addr incoming_tag = addr; //Full 40-bit tag

        //printf("checking entry: SKEW %lu, SET Index: %lu\n",i,c->skew_set_index_arr[i]);
        c->m_access[i][c->skew_set_index_arr[i]]++;

        for(int j =0; j< c->TagStore->total_assocs_per_skew; j++)
        {
            //printf("Comparing LineAddr:%llu with Full-Tag %llu\n",addr,c->TagStore->entries[i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j].full_tag);
            uns64 llc_index = (i*SKEW_SIZE)+(c->skew_set_index_arr[i]*SET_SIZE)+j;
            if((c->TagStore->entries[llc_index].valid) && (incoming_tag == c->TagStore->entries[llc_index].full_tag ))
            {
                //Line Found in Tag Store
                //printf("Line Found in Cache at entry! at SKEW: %lu, SET: %lu, Way: %lu\n",i,skew_set_index,j);
                c->s_hits++;
                c->m_hits[i][c->skew_set_index_arr[i]]++;
                return HIT;
            }
        }

        //If not in base ways, check in allocated tag pool.
        //Extra pool has allocation for the set
        if (c->dynTagPool.find(c->skew_set_index_arr[i]) != c->dynTagPool.end())
        {
            for(uint32_t w=0; w < c->dynTagPool[c->skew_set_index_arr[i]].size(); w++)
            {
                if ((c->dynTagPool[c->skew_set_index_arr[i]][w].skewID == i) && (c->dynTagPool[c->skew_set_index_arr[i]][w].tag_entry.valid) && (incoming_tag == c->dynTagPool[c->skew_set_index_arr[i]][w].tag_entry.full_tag) )
                {
                    c->s_hits++;
                    c->m_hits[i][c->skew_set_index_arr[i]]++;
                    return HIT;
                }
            }
        }
        
        /*
        for(uint32_t w=0; w < c->dynTagPool.size(); w++)
        {
            if(c->dynTagPool[w].skewID == i && c->dynTagPool[w].setID == c->skew_set_index_arr[i])
            {
                if(c->dynTagPool[w].tag_entry.valid && incoming_tag == c->dynTagPool[w].tag_entry.full_tag)
                {
                    c->s_hits++;
                    c->m_hits[i][c->skew_set_index_arr[i]]++;
                    return HIT;
                }
            }
        }
        */

        c->m_miss[i][c->skew_set_index_arr[i]]++;

    }
    //printf("Line is a miss in LLC!\n");
    c->s_miss++;
    return MISS;
}

// Cache Install
void mirage_install (mirageCache *c, Addr addr)
{
    c->s_installs++;
    Flag tagSAE;
    //Index and install using power of 2 choices and perform global eviction

    Addr skew_set_index;
    uns skew_select = skewSelect(c,addr, &tagSAE);
    
    //Always proceeds - no SAE

    uint64_t evicted_line_index; 

    Flag isFull = TRUE;
    //Find unused data store entry
    for(uint64_t i =0; i < c->DataStore->num_lines ; i++)
    {
        if(c->DataStore->entries[i].rPtr == NULL)
        {
            //printf("Found invalid data entry!\n");
            evicted_line_index = i;
            isFull = FALSE;
            break;
        }
    }

    if(isFull)
    {
        evicted_line_index = mirageGLE(c);
    }

    //Install Line
    tagEntry* tagPtr = NULL;
    bool found_in_base = false;
    //Tag Store
    for (int i =0 ; i < c->TagStore->total_assocs_per_skew ; i++)
    {
        uns64 llc_index = skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i;
        if(!c->TagStore->entries[llc_index].valid)
        {
            //printf("installing in Tag Store: %lu\n",skew_select*SKEW_SIZE + skew_set_index*SET_SIZE+i);

            c->TagStore->entries[llc_index].fPtr = &(c->DataStore->entries[evicted_line_index]);
            c->TagStore->entries[llc_index].valid = TRUE;
            c->TagStore->entries[llc_index].full_tag = addr;
            tagPtr = &(c->TagStore->entries[llc_index]);
            c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
            //printf("Skew %lu, set %lu has %lu installs\n",skew_select,skew_set_index,c->s_distribution[skew_select][skew_set_index]);
            found_in_base = true;
            break;
        }
    }

    //If not in main tagstore, evict from dynamic pool
    if (!found_in_base)
    {
        dynTagEntry tag_entry;
        tag_entry.skewID = skew_select;
        tag_entry.tag_entry.full_tag = addr;
        tag_entry.tag_entry.fPtr = &(c->DataStore->entries[evicted_line_index]);
        tag_entry.tag_entry.valid = TRUE;
        tag_entry.tag_entry.dirty = FALSE;

        std::vector<dynTagEntry> tag_entry_v;
        tag_entry_v.push_back(tag_entry);

        //if setID is found
        if (c->dynTagPool.find(c->skew_set_index_arr[skew_select]) != c->dynTagPool.end())
        {
            //invalid tag found
            bool invalid_tag_found = false;
            for(uint32_t w=0; w < c->dynTagPool[c->skew_set_index_arr[skew_select]].size(); w++)
            {
                if ((c->dynTagPool[c->skew_set_index_arr[skew_select]][w].skewID == skew_select) && (!c->dynTagPool[c->skew_set_index_arr[skew_select]][w].tag_entry.valid))
                {
                    invalid_tag_found = true;
                    c->dynTagPool[c->skew_set_index_arr[skew_select]][w].tag_entry = tag_entry.tag_entry;
                    
                    tagPtr = &(c->dynTagPool[c->skew_set_index_arr[skew_select]][w].tag_entry);
                    c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
                    break;
                }
            }
            if (!invalid_tag_found)
            {
                c->dynTagPool[c->skew_set_index_arr[skew_select]].push_back(tag_entry);
                int index = c->dynTagPool[c->skew_set_index_arr[skew_select]].size() - 1;

                tagPtr = &(c->dynTagPool[c->skew_set_index_arr[skew_select]][index].tag_entry);
                c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
            }
        }
        //set ID is not found, insert
        else
        {
            //c->dynTagPool[c->skew_set_index_arr[skew_select]] = std::vector<dynTagEntry>{tag_entry};
            c->dynTagPool[c->skew_set_index_arr[skew_select]] = tag_entry_v;
            //Umapsptr->node_index->emplace(5, 10);
            tagPtr = &(c->dynTagPool[c->skew_set_index_arr[skew_select]][0].tag_entry);  
            c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
        }
    }

    int ways_used = 0;
    int extra_pool_valid = 0;
    int extra_pool = 0;

    uns64 set_index = (skew_select*SKEW_SIZE)+(c->skew_set_index_arr[skew_select]*SET_SIZE);
    //base ways check
    for(int i=0; i< c->TagStore->total_assocs_per_skew; i++)
    {
        uns64 llc_index = set_index+i;
        if(c->TagStore->entries[llc_index].valid)
        {
            ways_used++;
        }
    }
    //Pool check
    //if setID is found
    if (c->dynTagPool.find(c->skew_set_index_arr[skew_select]) != c->dynTagPool.end())
    {
        for(uint32_t w=0; w < c->dynTagPool[c->skew_set_index_arr[skew_select]].size(); w++)
        {
            if (c->dynTagPool[c->skew_set_index_arr[skew_select]][w].skewID == skew_select)
            {

                if (c->dynTagPool[c->skew_set_index_arr[skew_select]][w].tag_entry.valid)
                    extra_pool_valid++;
                extra_pool++;
            }
        }
    }


    //TODO :: fix this for dyn-tag-alloc
    if (ways_used > c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]])
    {
        c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]] = ways_used;
    }

    if (extra_pool_valid > c->max_extra_pool_valid_used[skew_select][c->skew_set_index_arr[skew_select]])
    {
        c->max_extra_pool_valid_used[skew_select][c->skew_set_index_arr[skew_select]] = extra_pool_valid;
    }
    
    if (extra_pool > c->max_total_extra_pool_used[skew_select][c->skew_set_index_arr[skew_select]])
    {
        c->max_total_extra_pool_used[skew_select][c->skew_set_index_arr[skew_select]] = extra_pool;
    }

    //printf("Ways used for skew %lu, set %lu: %lu\n",skew_select,skew_set_index,ways_used);

    //Data Store
    if(tagPtr)
    {
        //printf("installing in Data Store: %lu\n",evicted_line_index);
        c->DataStore->entries[evicted_line_index].Data = 0; //Shouldn't matter
        c->DataStore->entries[evicted_line_index].rPtr = tagPtr;
        return;
    }
}


//Global Eviction from Data Store
uns64 mirageGLE(mirageCache *c)
{
    c->s_evict++;
    c->m_gle++;
    //printf("Random Global Eviction\n");
    //Choose a line from data store randomly for eviction
    uns64 line_to_evict = mtrand->randInt(c->DataStore->num_lines - 1) % (c->DataStore->num_lines);//rand() % c->DataStore->num_lines;
    //uns64 line_to_evict = mtrand->randInt(c->DataStore->num_lines - 1);
    //assert (line_to_evict < c->DataStore->num_lines);
    //Writeback if dirty
    if (c->DataStore->entries[line_to_evict].rPtr->dirty)
    {
        //printf("Writeback!\n");
    }

    //Invalidate Tag Store entry
    //Evict
    c->DataStore->entries[line_to_evict].rPtr->full_tag = 0;
    c->DataStore->entries[line_to_evict].rPtr->valid = FALSE;
    c->DataStore->entries[line_to_evict].rPtr->fPtr = NULL;
    c->DataStore->entries[line_to_evict].rPtr->dirty = 0;
    c->DataStore->entries[line_to_evict].rPtr = NULL;

    return line_to_evict;
}


// Select skew to install based on power of 2 choices
uns skewSelect(mirageCache *c, Addr addr, Flag* tagSAE)
{
    //Arbitrary upper threshold for max tags possibly used by a set
    uns min_tags_used = 1000;
    uns equal_count = 0;
    
    uns skew_select;
    uns skew_select_equals;
    
    std::vector<int> equals;
    //Addr skew_set_index_loc[NUM_SKEW];

    for(int i=0; i< c->TagStore->skews; i++)
    {
        //Calculate skew index
        //skew_set_index_loc[i] = mirage_hash(i,addr);
        //Addr skew_set_index = 5;
        uns used_tags = 0;

        //printf("checking entry %lu\n",i*SKEW_SIZE+skew_set_index*SET_SIZE);

        //Check for valid tags in base
        for(int j =0; j < c->TagStore->total_assocs_per_skew ; j++)
        {
            uns64 llc_index = (i*SKEW_SIZE) + (c->skew_set_index_arr[i]*SET_SIZE) + j;
            if((c->TagStore->entries[llc_index].valid))
            {
                used_tags++;
            }
        }

        //Entry found in the extra pool
        if (c->dynTagPool.find(c->skew_set_index_arr[i]) != c->dynTagPool.end())
        {
            for(uint32_t w=0; w < c->dynTagPool[c->skew_set_index_arr[i]].size(); w++)
            {
                if ((c->dynTagPool[c->skew_set_index_arr[skew_select]][w].skewID == i) && (c->dynTagPool[c->skew_set_index_arr[skew_select]][w].tag_entry.valid))
                    used_tags++;
            }
        }

        if(used_tags < min_tags_used)
        {
            min_tags_used = used_tags;
            skew_select = i;
        }
        //To randomize skew selection if same num of invalid tags in two skews
        // Only works for 2 skews. Generalize for > 2
        else if((used_tags <= equal_count))
        {
            if (used_tags < equal_count)
            {
                equals.clear();
            }
            equals.push_back(i);
            equal_count = used_tags;
            //skew_select = mtrand->randInt(equals.size() - 1) % (equals.size());
            //assert (skew_select < equals.size());
            skew_select_equals = equals[mtrand->randInt(equals.size() - 1) % (equals.size())];
        }
    }
    
    //assert (max_invalid_tags != 0);
    //SAE NEVER
    /*if(max_invalid_tags == 0)
    {
        //printf("No Invalid Tags! Perform SAE!\n");
        *tagSAE = TRUE;
        c->m_sae++;
        //Select skew on random
        uns skew_rand = rand() % c->TagStore->skews;
        //*skew_set_index = c->skew_set_index_arr[skew_rand];
        return ( skew_rand );
    }
    else
    {
        *tagSAE = FALSE;
    }*/

    *tagSAE = FALSE;

    //*skew_set_index = c->skew_set_index_arr[skew_select];
    if (min_tags_used < equal_count)
        return skew_select;
    else
        return skew_select_equals;
}

// Return hashed addr
Addr mirage_hash(uns seed, Addr addr, int skew)
{
    //return addr % NUM_SETS;
    //PRINCE Cipher with random number as the seed
   Addr hashed_addr;
   Addr l_addr_bits = addr & 0xFFFFFFFFFF;
   //printf("Addr accessed in Hash Table! Addr: %llu\n",addr);

   PrinceHashTable* PHT_c = PHT[skew];

   if(PHT_c->entries[l_addr_bits] != -1)
   {
	//printf("Addr Exists in Hash Table! Addr: %llu\n",addr);
	hashed_addr = PHT_c->entries[l_addr_bits];
	//printf("Addr Exists in Hash Table! Addr: %llu\n",addr);
   }
   else
   {
    //printf("Addr Inserted in Hash Table! Addr: %llu\n",addr);
	PHT_c->entries[l_addr_bits] = calcPRINCE64(addr, seed) % NUM_SETS;
	hashed_addr = PHT_c->entries[l_addr_bits];
    //printf("Addr Inserted in Hash Table! Addr: %llu\n",addr);
   }
   //hashed_addr = calcPRINCE64(addr, seed) % NUM_SETS;
   return hashed_addr;
}

// Allocate tags from the pool
/*
uint32_t allocateTag(mirageCache* c, int skewID, uns64 setID)
{
    //Allocate a tagEntry from the dynamic tag pool
    dynTagEntry newEntry;
    newEntry.setID = setID;
    newEntry.skewID = skewID;
    c->dynTagPool.push_back(newEntry);
    uint32_t index = c->dynTagPool.size();
    return index;
}
*/

// Print Stats
void mirage_print_stats(mirageCache *c, char *header)
{
  printf("\n%s_ACCESS       \t : %llu",  header,  c->s_count);
  printf("\n%s_MISS         \t : %llu",  header,  c->s_miss);
  printf("\n%s_HITS         \t : %llu",  header,  c->s_hits);
  printf("\n%s_INSTALLS     \t : %llu",  header,  c->s_installs);
  printf("\n%s_EVICTS       \t : %llu",  header,  c->s_evict);
  printf("\n%s_SAE_EVICTS   \t : %llu",  header,  c->m_sae);

  printf("\n%s_DYNAMIC_TAG_POOL_SIZE \t : %llu",  header, c->dynTagPool.size());

  printf("\n---------------------SKEW-SET-DISTRIBUTION----------------------\n");
  printf("\n---------------------     WAYS USED       ----------------------\n");

  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        uint64_t ways_used = 0;
        for(uint64_t k=0; k < c->TagStore->total_assocs_per_skew; k++)
        {
            if(c->TagStore->entries[j*SKEW_SIZE+i*SET_SIZE+k].valid)
            {
                ways_used++;
            }
        }
        printf("%lu \t\t",ways_used);
    }
    //Max ways used
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->max_ways_used[j][i]);
    }

    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->max_extra_pool_valid_used[j][i]);
    }
    
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->max_total_extra_pool_used[j][i]);
    }

    printf("\n");
  }

  printf("\n---------------------     INSTALLS        ----------------------\n");

  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->m_installs[j][i]);
    }
    printf("\n");
  }

  printf("\n---------------------     HITS        ----------------------\n");
  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->m_hits[j][i]);
    }
    printf("\n");
  }

  printf("\n---------------------     MISS        ----------------------\n");
  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->m_miss[j][i]);
    }
    printf("\n");
  }

  printf("\n---------------------     ACCESSES        ----------------------\n");
  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->m_access[j][i]);
    }
    printf("\n");
  }

  printf("\n");
}
