#include "mirage.h"
#include "prince.h"
//TODO
// Power of 2 choices --> random skew selection only works for 2 skews. Generalize for > 2
// implement write/read functionality and dirty writebacks

#define SKEW_SIZE 16384*14
#define SET_SIZE 14

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

    /*
    c->princeHashTable0 = (Addr*) calloc(NUM_LINES_IN_MEM,sizeof(Addr));
    c->princeHashTable1 = (Addr*) calloc(NUM_LINES_IN_MEM,sizeof(Addr));

    //Initilize hash table
    
    for(int j =0; j < MEM_SIZE_MB*1024*1024/LINESIZE; j++)
    {
        c->princeHashTable0[j] = calcPRINCE64(j, rand()) % NUM_SETS;
    }

    for(int j =0; j < MEM_SIZE_MB*1024*1024/LINESIZE; j++)
    {
        c->princeHashTable1[j] = calcPRINCE64(j, rand()) % NUM_SETS;
    }*/
    

    return c;
}

// Addr is line addr = addr / CACHE_LINESIZE
Flag mirage_access (mirageCache *c, Addr addr)
{
    c->s_count++;
    // Access skews in parallel
    for(int i =0 ; i<c->TagStore->skews; i++)
    {
        
        c->skew_set_index_arr[i] = mirage_hash(c->seed[i],addr);
        //assert (skew_set_index == c->princeHashTable0[addr]);
        //assert (skew_set_index == c->princeHashTable1[addr]);


        Addr incoming_tag = addr; //Full 40-bit tag

        //printf("checking entry: SKEW %lu, SET Index: %lu\n",i,c->skew_set_index_arr[i]);
        c->m_access[i][c->skew_set_index_arr[i]]++;

        for(int j =0; j< c->TagStore->total_assocs_per_skew; j++)
        {
            //printf("Comparing LineAddr:%llu with Full-Tag %llu\n",addr,c->TagStore->entries[i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j].full_tag);
            if(c->TagStore->entries[i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j].valid && incoming_tag == c->TagStore->entries[i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j].full_tag )
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
    Flag tagSAE;
    //Index and install using power of 2 choices and perform global eviction

    Addr skew_set_index;
    uns skew_select = skewSelect(c,addr, &tagSAE);
    
    //Check if tagStore has SAE
    if(tagSAE)
    {
        //Invalidate from Tag Store and evict corresponding Data Store entry and replace with new line
        uns way_select = rand() % c->TagStore->total_assocs_per_skew;   //Can possibly implement some kind of replacement policy here
        //uns set_select = mirage_hash(skew_select,addr);
        uns set_select = skew_select ? c->skew_set_index_arr[1] : c->skew_set_index_arr[0]; //Only works for 2 skews. 

        //Writeback if dirty
        //if (c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].dirty)
        //{
        //    printf("Writeback!\n");
        //}

        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].full_tag = addr;
        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].dirty = FALSE; 
        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].valid = TRUE;
        //c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].fPtr->Data = 0;   //Use incoming data if needed
        return;
    }
    //Only proceeds beyond this point if there are invalid tags present

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
        if(!c->TagStore->entries[skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i].valid)
        {
            //printf("installing in Tag Store: %lu\n",skew_select*SKEW_SIZE + skew_set_index*SET_SIZE+i);

            c->TagStore->entries[skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i].fPtr = &(c->DataStore->entries[evicted_line_index]);
            c->TagStore->entries[skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i].valid = TRUE;
            c->TagStore->entries[skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i].full_tag = addr;
            tagPtr = &(c->TagStore->entries[skew_select*SKEW_SIZE + c->skew_set_index_arr[skew_select]*SET_SIZE+i]);
            c->m_installs[skew_select][c->skew_set_index_arr[skew_select]]++;
            //printf("Skew %lu, set %lu has %lu installs\n",skew_select,skew_set_index,c->s_distribution[skew_select][skew_set_index]);
            break;
        }
    }

    int ways_used = 0;
    for(int i=0; i< c->TagStore->total_assocs_per_skew; i++)
    {
        if(c->TagStore->entries[skew_select*SKEW_SIZE+c->skew_set_index_arr[skew_select]*SET_SIZE+i].valid)
        {
            ways_used++;
        }
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
uns mirageGLE(mirageCache *c)
{
    c->s_evict++;
    c->m_gle++;
    //printf("Random Global Eviction\n");
    //Choose a line from data store randomly for eviction
    uns64 line_to_evict = mtrand->randInt(c->DataStore->num_lines - 1) % (c->DataStore->num_lines);//rand() % c->DataStore->num_lines;

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
    uns max_invalid_tags = 0;
    uns skew_select;
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
            if(!(c->TagStore->entries[i*SKEW_SIZE+c->skew_set_index_arr[i]*SET_SIZE+j].valid))
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
        else if(invalid_tags == max_invalid_tags)
        {
            //skew_select = rand() % 2;
            skew_select = mtrand->randInt(1) % 2;
        }
    }
    
    //assert (max_invalid_tags != 0);

    if(max_invalid_tags == 0)
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
    }

    //*skew_set_index = c->skew_set_index_arr[skew_select];
    return skew_select;
}

// Return hashed addr
Addr mirage_hash(uns seed, Addr addr)
{
    //return addr % NUM_SETS;
    //PRINCE Cipher with skew number as the seed
    Addr hashed_addr = calcPRINCE64(addr, seed) % NUM_SETS;
    return hashed_addr;
    
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
