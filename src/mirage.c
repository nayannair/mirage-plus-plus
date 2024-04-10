#include "mirage.h"
#include "prince.h"
#include <vector>
//TODO
// Power of 2 choices --> random skew selection only works for 2 skews. Generalize for > 2
// implement write/read functionality and dirty writebacks

std::vector<PrinceHashTable*> PHT (NUM_SKEW);
//16384*1024*1024/LINESIZE
uint32_t tag_addr_space = 1 << 14; //40-bit tag 

uint32_t** hashTable; 

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

    //Shared TagStore
    c->SharedTagStore = (tagStore*) calloc(1,sizeof(tagStore));
    c->SharedTagStore->shared_assocs = SHARED_WAYS;
    c->SharedTagStore->sets = sets;
    c->SharedTagStore->skews = 1;
    uint64_t num_entries_in_sh_tagstr = sets*SHARED_WAYS;
    c->SharedTagStore->entries = (tagEntry*) calloc(num_entries_in_sh_tagstr,sizeof(tagEntry));
    printf("num shared tag %d\n",num_entries_in_sh_tagstr);

    for(uint64_t i=0; i<num_entries_in_sh_tagstr; i++)
    {
        c->SharedTagStore->entries[i].skewID =-1;
        c->SharedTagStore->entries[i].full_tag = 0;
        c->SharedTagStore->entries[i].fPtr = NULL;
        c->SharedTagStore->entries[i].valid = 0;
        c->SharedTagStore->entries[i].dirty = 0;
    }

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
    c->m_codi_relocs = 0;

    
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
                //printf("Line Found in Cache at entry! at SKEW: %lu, SET: %lu, Way: %lu\n",i,c->skew_set_index_arr[i],j);
                c->s_hits++;
                c->m_hits[i][c->skew_set_index_arr[i]]++;
                return HIT;
            }
        }

        //shared tag store access
        for(int j=0; j < c->SharedTagStore->shared_assocs;j++)
        {
            uns64 llc_index = (c->skew_set_index_arr[i]*SHARED_WAYS)+j;
            if((c->SharedTagStore->entries[llc_index].valid) && (incoming_tag == c->SharedTagStore->entries[llc_index].full_tag ) && (c->SharedTagStore->entries[llc_index].skewID==i))
            {
                //Line Found in Tag Store
                //printf("Line Found in Cache at entry! at SKEW: %lu, SET: %lu, Way: %lu\n",i,skew_set_index,j);
                c->s_hits++;
                c->m_hits[i][c->skew_set_index_arr[i]]++;
                return HIT;
            }
        }

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
    Flag tagSAE = 0;
    //Index and install using power of 2 choices and perform global eviction

    Addr skew_set_index;
    uns skew_select = skewSelect(c,addr, &tagSAE);
    //printf("Chose skew %d\n",skew_select);
    
    //Check if tagStore has SAE
    if(tagSAE)
    {
        //printf("SAE\n");
        //Invalidate from Tag Store and evict corresponding Data Store entry and replace with new line
        uns way_select = rand() % c->TagStore->total_assocs_per_skew;   //Can possibly implement some kind of replacement policy here
        //uns set_select = mirage_hash(skew_select,addr);
        uns set_select = c->skew_set_index_arr[skew_select]; //Only works for 2 skews. 

        //printf("Relocation\n");
        //Perform CoDi Relocation
        // Find CoDi victim
        uns skew_opt;
        Addr codi_victim_idx = codiVictim(c,skew_select,set_select,&skew_opt);
        // Do CoDi Relocation
        if(codi_victim_idx != -1)
        {
            //printf("CoDi Relocation\n");
            codiReloc(c,codi_victim_idx,skew_opt);
            c->m_codi_relocs++;
        }   
        else
        {
            //printf("Couldn't find CoDi victims!\n");
            c->m_sae++;
            uns64 llc_index = skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select;
            c->TagStore->entries[llc_index].full_tag = addr;
            c->TagStore->entries[llc_index].dirty = FALSE; 
            c->TagStore->entries[llc_index].valid = TRUE;
        }
    return;
    }
    //Only proceeds beyond this point if there are invalid tags present

    //printf("Has Valid Tags!\n");
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
    int installed_in_base = 0;
    //Tag Store
    for (int i =0 ; i < c->TagStore->total_assocs_per_skew ; i++)
    {
        uns64 llc_index = skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i;
        if(!c->TagStore->entries[llc_index].valid)
        {
            //printf("installing in Tag Store: %lu\n",skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i);

            c->TagStore->entries[llc_index].fPtr = &(c->DataStore->entries[evicted_line_index]);
            c->TagStore->entries[llc_index].valid = TRUE;
            c->TagStore->entries[llc_index].full_tag = addr;
            tagPtr = &(c->TagStore->entries[llc_index]);
            c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
            installed_in_base = 1;
            //printf("Skew %lu, set %lu has %lu installs\n",skew_select,skew_set_index,c->s_distribution[skew_select][skew_set_index]);
            break;
        }
    }

   //Install in shared tag
    if(!installed_in_base)
    {
        for (int i =0 ; i < c->SharedTagStore->shared_assocs ; i++)
        {
            uns64 llc_index = c->skew_set_index_arr[skew_select]*SHARED_WAYS+i;
            if(!c->SharedTagStore->entries[llc_index].valid)
            {
                //printf("installing in Tag Store: %lu\n",skew_select*SKEW_SIZE + skew_set_index*SET_SIZE+i);

                c->SharedTagStore->entries[llc_index].fPtr = &(c->DataStore->entries[evicted_line_index]);
                c->SharedTagStore->entries[llc_index].valid = TRUE;
                c->SharedTagStore->entries[llc_index].skewID = skew_select;
                c->SharedTagStore->entries[llc_index].full_tag = addr;
                tagPtr = &(c->SharedTagStore->entries[llc_index]);
                c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
                //printf("Skew %lu, set %lu has %lu installs\n",skew_select,skew_set_index,c->s_distribution[skew_select][skew_set_index]);
                break;
                
            }
        }
    }

    int ways_used = 0;
    uns64 set_index = (skew_select*SKEW_SIZE)+(c->skew_set_index_arr[skew_select]*SET_SIZE);
    for(int i=0; i< c->TagStore->total_assocs_per_skew; i++)
    {
        uns64 llc_index = set_index+i;
        if(c->TagStore->entries[llc_index].valid)
        {
            ways_used++;
        }
    }
    if (ways_used > c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]])
    {
        c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]] = ways_used;
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
    c->DataStore->entries[line_to_evict].rPtr->skewID = -1;
    c->DataStore->entries[line_to_evict].rPtr = NULL;

    return line_to_evict;
}


// Select skew to install based on power of 2 choices
uns skewSelect(mirageCache *c, Addr addr, Flag* tagSAE)
{
    uns max_invalid_tags = 0;
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
        uns invalid_tags = 0;

        //printf("checking entry %lu\n",i*SKEW_SIZE+skew_set_index*SET_SIZE);

        //Check for invalid tags
        for(int j =0; j < c->TagStore->total_assocs_per_skew ; j++)
        {
            uns64 llc_index = i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j;
            //printf("llc_index_skew_select =%llu\n",llc_index);
            if(!(c->TagStore->entries[llc_index].valid))
            {
                invalid_tags++;
            }
        }

        for(int j =0; j < c->SharedTagStore->shared_assocs ; j++)
        {
            uns64 llc_index = c->skew_set_index_arr[i]*SHARED_WAYS+j;
            if(!(c->SharedTagStore->entries[llc_index].valid))
            {
                invalid_tags++;
            }
        }

        if(invalid_tags > max_invalid_tags)
        {
            max_invalid_tags = invalid_tags;
            skew_select = i;
        }
        //To randomize skew selection if same num of invalid tags in two skews
        // Only works for 2 skews. Generalize for > 2
        if((invalid_tags >= equal_count))
        {
            if (invalid_tags > equal_count)
            {
                equals.clear();
            }
            equals.push_back(i);
            equal_count = invalid_tags;
            //skew_select = mtrand->randInt(equals.size() - 1) % (equals.size());
            //assert (skew_select < equals.size());
            //printf("equals.size() = %d\n",equals.size());
            skew_select_equals = equals[mtrand->randInt(equals.size() - 1) % (equals.size())];
            //printf("skew_select_equals = %d\n",skew_select_equals);

        }

        //printf("Skew %d -> %u invalid tags\n", i, invalid_tags);
        //printf("Max invalid tags -> %u\n", max_invalid_tags);
    }
    
    //assert (max_invalid_tags != 0);

    if(max_invalid_tags == 0)
    {
        //printf("No Invalid Tags! Perform SAE!\n");
        *tagSAE = TRUE;
        //c->m_sae++;
        //Select skew on random
        uns skew_rand = rand() % c->TagStore->skews;
        //*skew_set_index = c->skew_set_index_arr[skew_rand];
        return ( skew_rand );
    }
    else
    {
        *tagSAE = FALSE;
    }

    //*skew_set_index = c->skew_set_index_arr[skew_select];
    if (max_invalid_tags > equal_count)
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

//Perform relocation
void codiReloc(mirageCache* c, Addr codi_victim_idx, uns skew_select)
{

    //mirage_install(c,codi_victim_idx);   
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
    //Tag Store
    for (int i =0 ; i < c->TagStore->total_assocs_per_skew ; i++)
    {
        uns64 llc_index = skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i;
        if(!c->TagStore->entries[llc_index].valid)
        {
            //printf("installing in Tag Store: %lu\n",skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i);

            c->TagStore->entries[llc_index].fPtr = &(c->DataStore->entries[evicted_line_index]);
            c->TagStore->entries[llc_index].valid = TRUE;
            c->TagStore->entries[llc_index].full_tag = codi_victim_idx;
            tagPtr = &(c->TagStore->entries[llc_index]);
            c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
            //printf("Skew %lu, set %lu has %lu installs\n",skew_select,skew_set_index,c->s_distribution[skew_select][skew_set_index]);
            break;
        }
    }

    int ways_used = 0;
    uns64 set_index = (skew_select*SKEW_SIZE)+(c->skew_set_index_arr[skew_select]*SET_SIZE);
    for(int i=0; i< c->TagStore->total_assocs_per_skew; i++)
    {
        uns64 llc_index = set_index+i;
        if(c->TagStore->entries[llc_index].valid)
        {
            ways_used++;
        }
    }
    if (ways_used > c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]])
    {
        c->max_ways_used[skew_select][c->skew_set_index_arr[skew_select]] = ways_used;
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

//Find victim to relocate (Only 1 level deep for now)
Addr codiVictim(mirageCache* c, uns skew_select, uns set_select, uns* skew_opt)
{   
    Addr target = -1;
    //Search for alternate skew-set location for each way
    int found_invalid_tags = 0;

    for(uint32_t i=0; i < c->TagStore->total_assocs_per_skew; i++)
    {   
        //printf("Way %d being checked...\n",i);
        uns64 llc_index = skew_select*SKEW_SIZE+set_select*SET_SIZE+i;
        //printf("reloc_llc_index =%llu\n",llc_index);

        Addr addr_req = c->TagStore->entries[llc_index].full_tag;

        for(int s=0; s < NUM_SKEW; s++) 
        {
           if(s!=skew_select)
           {
                Addr reloc_addr  = mirage_hash(c->seed[s],addr_req,s);
                //printf("Checking tags for set=%llu\n",reloc_addr);
                
                if(checkInvalidTags(c,s,reloc_addr))
                {
                    target = addr_req;
                    found_invalid_tags = 1;
                    *skew_opt = s;
                    break;
                }
           }
        }
        if(found_invalid_tags)
        {
            break;
        }
    }
    return target;
}

int checkInvalidTags(mirageCache* c, uns skew, Addr reloc_addr)
{
    uint32_t invalid_tags = 0;
    for(uint32_t i=0; i < c->TagStore->total_assocs_per_skew; i++)
    {
        if(!(c->TagStore->entries[skew*SKEW_SIZE+reloc_addr*SET_SIZE+i].valid))
        {
            //printf("Set=%d, Invalid Tag at %d\n",reloc_addr,i);
            invalid_tags++;
        }
        else
        {
            //printf("Valid Entry with Tag=%llu\n",c->TagStore->entries[skew*SKEW_SIZE+reloc_addr*SET_SIZE+i].full_tag);
        }
    }

    if(invalid_tags == 0)
        return 0;
    else
        return 1;
}

// Print Stats
void mirage_print_stats(mirageCache *c, char *header)
{
  printf("\n%s_ACCESS       \t : %llu",  header,  c->s_count);
  printf("\n%s_MISS         \t : %llu",  header,  c->s_miss);
  printf("\n%s_HITS         \t : %llu",  header,  c->s_hits);
  printf("\n%s_INSTALLS     \t : %llu",  header,  c->s_installs);
  printf("\n%s_EVICTS       \t : %llu",  header,  c->s_evict);
  printf("\n%s_SAE_EVICTS   \t : %llu",  header,  c->m_sae);
  printf("\n%s_CODI_RELOCS   \t : %llu",  header,  c->m_codi_relocs);

  printf("\n---------------------SKEW-SET-DISTRIBUTION----------------------\n");
  printf("\n---------------------     WAYS USED       ----------------------\n");

  printf("\t\tSKEW 0 \t\tSKEW 1\n");
  for(uint64_t i=0; i< NUM_SETS; i++)
  {
    printf("Set %lu:\t",i); 
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        uint64_t ways_used = 0;
        uint64_t shared_ways_used = 0;
        for(uint64_t k=0; k < c->TagStore->total_assocs_per_skew; k++)
        {
            if(c->TagStore->entries[j*SKEW_SIZE+i*SET_SIZE+k].valid)
            {
                ways_used++;
            }
        }
        
        printf("%lu \t\t",ways_used);

        for(uint64_t k=0; k < c->SharedTagStore->shared_assocs; k++)
        {
            if(c->SharedTagStore->entries[i*SHARED_WAYS+k].valid && c->SharedTagStore->entries[i*SHARED_WAYS+k].skewID == j)
            {
                shared_ways_used++;
            }

        //printf("%lu \t\t",shared_ways_used);
        }  
        printf("%lu \t\t",shared_ways_used);
    }

    
    //Max ways used
    for(uint64_t j=0 ; j < NUM_SKEW; j++)
    {
        printf("%lu \t\t",c->max_ways_used[j][i]);
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
