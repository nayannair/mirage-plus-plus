#include "mirage.h"
#include "prince.h"
//TODO
// implement hash functions -- PRINCE64
// define SKEW_SIZE and SET_SIZE globally

#define SKEW_SIZE 16384*14
#define SET_SIZE 14

mirageCache *mirageCache_new(uns sets, uns assocs, uns skews )
{
    mirageCache* c = (mirageCache*) calloc(1,sizeof(mirageCache));
    c->DataStore = (dataStore*) calloc(1,sizeof(dataStore));
    c->TagStore = (tagStore*) calloc(1,sizeof(tagStore));
    c->TagStore->assocs = assocs;
    c->TagStore->sets = sets;
    c->TagStore->extra_assocs = EXTRA_WAYS*assocs; //75% extra ways
    c->TagStore->total_assocs_per_skew = (assocs + EXTRA_WAYS*assocs);
    c->TagStore->skews = skews;

    int num_entries_tag = skews*sets*(c->TagStore->total_assocs_per_skew);
    int num_entries_data = skews*sets*assocs;
    printf("num tags %d\n",num_entries_tag);
    printf("num data %d\n",num_entries_data);
    
    c->TagStore->entries = (tagEntry*) calloc(num_entries_tag,sizeof(tagEntry));        // How to index skews
    c->DataStore->entries = (dataEntry*) calloc(num_entries_data,sizeof(dataEntry));    // sum*assocs = lines
    c->DataStore->num_lines = skews*sets*assocs;

    
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
Flag mirageCache_access (mirageCache *c, Addr addr)
{
    // Access skews in parallel
    for(int i =0 ; i<c->TagStore->skews; i++)
    {
        
        Addr skew_set_index = mirage_hash(i,addr);
        Addr incoming_tag = addr; //Full 40-bit tag

        //printf("checking entry number: %lu\n",i*SKEW_SIZE+skew_set_index*SET_SIZE);
        

        for(int j =0; j< c->TagStore->total_assocs_per_skew; j++)
        {
            if(c->TagStore->entries[i*SKEW_SIZE+skew_set_index*SET_SIZE+j].valid && incoming_tag == c->TagStore->entries[i*SKEW_SIZE+skew_set_index*SET_SIZE+j].full_tag)
            {
                //Line Found in Tag Store
                return TRUE;
            }
        }
        
    }
    return FALSE;
}

// Cache Install
void mirageCache_install (mirageCache *c, Addr addr)
{
    Flag tagSAE;
    //Index and install using power of 2 choices and perform global eviction
    uns skew_select = skewSelect(c,addr, &tagSAE);
    
    //Check if tagStore has SAE
    if(tagSAE)
    {
        //Invalidate from Tag Store and evict corresponding Data Store entry and replace with new line
        uns way_select = rand() % c->TagStore->total_assocs_per_skew;   //Can possibly implement some kind of replacement policy here
        uns set_select = mirage_hash(skew_select,addr);

        //Writeback if dirty
        if (c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].dirty)
        {
            printf("Writeback!\n");
        }

        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].full_tag = addr;
        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].dirty = FALSE; 
        c->TagStore->entries[skew_select*SKEW_SIZE + set_select*SET_SIZE + way_select].fPtr->Data = 0;   //Use incoming data if needed
        return;
    }
    //Only proceeds beyond this point if there are invalid tags present
    
    
    Addr skew_set_index = mirage_hash(skew_select, addr);

    uns evicted_line_index; 

    //If data store full -- perform global eviction
    if(c->DataStore->isFull)
    {
        evicted_line_index = mirageGLE(c);
    }
    else
    {
        //Find unused data store entry
        for(int i =0; i < c->DataStore->num_lines ; i++)
        {
            if(c->DataStore->entries[i].rPtr == NULL)
            {
                evicted_line_index = i;
                break;
            }
        }

        //Check if data store is full
        if(evicted_line_index == c->DataStore->num_lines-1)
        {
            c->DataStore->isFull = TRUE;
        }

    }

    //Install Line
    tagEntry* tagPtr = NULL;
    //Tag Store
    for (int i =0 ; i < c->TagStore->total_assocs_per_skew ; i++)
    {
        if(!c->TagStore->entries[skew_select*SKEW_SIZE + skew_set_index*SET_SIZE].valid)
        {
            c->TagStore->entries[skew_select*SKEW_SIZE + skew_set_index*SET_SIZE].fPtr = &(c->DataStore->entries[evicted_line_index]);
            c->TagStore->entries[skew_select*SKEW_SIZE + skew_set_index*SET_SIZE].valid = TRUE;
            c->TagStore->entries[skew_select*SKEW_SIZE + skew_set_index*SET_SIZE].full_tag = addr;
            tagPtr = &(c->TagStore->entries[skew_select*SKEW_SIZE + skew_set_index*SET_SIZE]);
            break;
        }
    }
    //Data Store
    {
        c->DataStore->entries[evicted_line_index].Data = 0; //Shouldn't matter
        c->DataStore->entries[evicted_line_index].rPtr = tagPtr;
    }
}


//Global Eviction from Data Store
uns mirageGLE(mirageCache *c)
{
    srand( time(NULL) );
    //Choose a line from data store randomly for eviction
    uns line_to_evict = rand() % (c->DataStore->num_lines);

    //Writeback if dirty
    if (c->DataStore->entries[line_to_evict].rPtr->dirty)
    {
        printf("Writeback!\n");
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

    for(int i=0; i< c->TagStore->skews; i++)
    {
        //Calculate skew index
        Addr skew_set_index = mirage_hash(i,addr);
        //Addr skew_set_index = 5;
        uns invalid_tags = 0;

        //printf("checking entry %lu\n",i*SKEW_SIZE+skew_set_index*SET_SIZE);

        //Check for invalid tags
        for(int j =0; j < c->TagStore->total_assocs_per_skew ; j++)
        {
            if(!c->TagStore->entries[i*SKEW_SIZE+skew_set_index*SET_SIZE+j].valid)
            {
                invalid_tags++;
            }
        }

        if(invalid_tags > max_invalid_tags)
        {
            max_invalid_tags = invalid_tags;
            skew_select = i;
        }
    }
    
    
    if(max_invalid_tags == 0)
    {
        printf("No Invalid Tags! Perform SAE!\n");
        *tagSAE = TRUE;
        //Select skew on random
        return ( rand() % c->TagStore->skews );
    }
    else
    {
        *tagSAE = FALSE;
    }

    return skew_select;
}

// Return hashed addr
Addr mirage_hash(uns skew, Addr addr)
{
    //return addr % NUM_SETS;
    //PRINCE Cipher with skew number as the seed
    Addr hashed_addr = calcPRINCE64(addr,skew) % NUM_SETS;
    return hashed_addr;
    /*
    switch(skew)
    {
        case 0: return addr; 
                break;
        case 1: return addr;
                break;
        default: return addr;
    }
    */
}