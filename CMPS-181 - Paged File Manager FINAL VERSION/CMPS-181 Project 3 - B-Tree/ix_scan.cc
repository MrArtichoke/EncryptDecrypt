#include <iostream>
#include <cstring>
#include <algorithm>
#include <math.h>
#include "ix.h"

IndexManager* IndexManager::_index_manager = 0;

PagedFileManager *IndexManager::_pf_manager = NULL;

IndexManager* IndexManager::instance()
{
    if(!_index_manager)
        _index_manager = new IndexManager();

    return _index_manager;
}

IndexManager::IndexManager()
{
    levels = 0;
}

IndexManager::~IndexManager()
{
}

void IndexManager::print_leaf(void *page, const Attribute &attr, const unsigned flag) const
{
    unsigned free_space;
    unsigned offset;
    if(flag == 0){ // root page
        memcpy(&free_space, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));
    
        offset = sizeof(idir) + 2 * sizeof(unsigned);
    }else{ // leaf page
        memcpy(&free_space, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));
        
        offset = sizeof(ldir);
    }
    
    if(attr.type == TypeVarChar){
        void *last_char = malloc(attr.length + 1);
        unsigned char_size;
        
        // 0 = same, !0 = not same;
        unsigned is_same = 1; 
        // 0 = false, 1 = true
        unsigned is_start = 1;

        while(offset < free_space){
            /* get length of varchar */
            memcpy(&char_size, (char *)page + offset, sizeof(unsigned));
            offset += sizeof(unsigned);
            
            /* print varchar */
            void *varchar = malloc(char_size + 1);
            memcpy(varchar, (char *)page + offset, char_size);
            offset += char_size;
            ((char *)varchar)[char_size] = '\0';
            
            if(is_start != 1){
                is_same = memcmp(last_char, varchar, char_size + 1);
            }
            
            if(is_same != 0){
                if(is_start != 1){
                    cout << "\"" << ",";
                }
                cout << "\"" << (char *)varchar << ":[";
            }else{
                cout << ",";
            }
            
            
            RID rid;
            memcpy(&rid, (char *)page + offset, sizeof(RID));
            offset += sizeof(RID);
            
            cout << "(" << rid.pageNum << "," << rid.slotNum << ")";
            
            memcpy(last_char, varchar, char_size + 1);
            
            is_start = 0;
            free(varchar);
        }
        free(last_char);
    }else{
        unsigned key_data;
        unsigned last_key;
        float f_key_data;
        float f_last_key;
        
        // 0 = same, !0 = not same;
        unsigned is_same = 1; 
        // 0 = false, 1 = true
        unsigned is_start = 1;
        
        while(offset < free_space){
             /* get key */
            if(attr.type == TypeInt){
                memcpy(&key_data, (char *)page + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
            }else{
                memcpy(&f_key_data, (char *)page + offset, sizeof(float));
                offset += sizeof(float);
            }
            
            if(is_start != 1){
                if(attr.type == TypeInt){
                    is_same = memcmp(&last_key, &key_data, sizeof(unsigned));
                }else{
                    is_same = memcmp(&f_last_key, &f_key_data, sizeof(float));
                }
            }
            
            if(is_same != 0){
                if(is_start != 1){
                    cout << "\"" << ",";
                }
                if(attr.type == TypeInt){
                    cout << "\"" << key_data << ":[";
                }else{
                    cout << "\"" << f_key_data << ":[";
                }
            }else{
                cout << "\",";
            }
            
            RID rid;
            memcpy(&rid, (char *)page + offset, sizeof(RID));
            offset += sizeof(RID);
            
            cout << "(" << rid.pageNum << "," << rid.slotNum << ")";
            
            if(attr.type == TypeInt){
                last_key = key_data;
            }else{
                f_last_key = f_key_data;
            }
            
            is_start = 0;
        }    
    }
}

void set_idir(void * page, idir index_dir)
{
    // Setting the index directory.
    memcpy (page, &index_dir, sizeof(index_dir));
}

idir get_idir(void * page)
{
    // Getting the index directory.
    idir index_dir;
    memcpy (&index_dir, page, sizeof(idir));
    return index_dir;
}

ldir get_ldir(void * page)
{
    // Getting the leaf directory.
    ldir leaf_dir;
    memcpy (&leaf_dir, page, sizeof(ldir));
    return leaf_dir;
}

unsigned IndexManager::get_num_children(void * pageData) const
{
    idir index_dir = get_idir(pageData);
    return index_dir.nchildren;
}

unsigned get_data_entry_size(const Attribute &attr, const void *key) 
{
    unsigned size = 0;
    
    if(attr.type == TypeVarChar){
        unsigned len;
        memcpy(&len, key, 4);
        size += len;
    }
    
    size += 4;
    
    size += sizeof(RID);

    return size;
}

unsigned getRootPageFreeSpaceSize(void * page) 
{
    unsigned free_space;
    memcpy(&free_space, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));

    return 4096 - free_space;
}

unsigned IndexManager::getIndexPageFreeSpaceSize(void * page) 
{
    unsigned nchildren = get_num_children(page); 
    
    unsigned free_space = sizeof(idir) +  (2 * nchildren * sizeof(unsigned) - sizeof(unsigned));

    return 4096 - free_space;
}

unsigned getLeafPageFreeSpaceSize(void * page) 
{
    unsigned free_space;
    memcpy(&free_space, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));

    return 4096 - free_space;
}

unsigned prepare_entries(void *page, const Attribute &attr, const unsigned de_size, const void *key, const unsigned flag)
{   
    unsigned free_space_pointer;
    unsigned offset;
    if(flag == 0){
        memcpy(&free_space_pointer, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));
    
        offset = sizeof(idir) + 2 * sizeof(unsigned);
    }else{
        memcpy(&free_space_pointer, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));
    
        offset = sizeof(ldir);
    }
    
    if(attr.type == TypeVarChar){
        unsigned key_size;
        memcpy(&key_size, key, sizeof(unsigned));
        
        void *key_data = malloc(key_size);
        memcpy(key_data, (char *)key + sizeof(unsigned), key_size);
        
        while(offset < free_space_pointer){
            unsigned char_size;
            memcpy(&char_size, (char *)page + offset, sizeof(unsigned));
            offset += sizeof(unsigned);
        
            void *char_data = malloc(char_size);
            memcpy(char_data, (char *)page + offset, char_size);
            offset += char_size;
        
            unsigned max_size = max(key_size, char_size);
            int comp = memcmp(key_data, char_data, max_size);
            
            if(comp < 0){
                offset -= sizeof(unsigned);
                offset -= char_size;
                break;
            }
            
            offset += sizeof(RID);
            
            free(char_data);
        }
        free(key_data);
        
        if(offset < free_space_pointer){ // move data entries so new de can be placed in the middle
            memmove((char *)page + offset + de_size, (char *)page + offset, free_space_pointer - offset);
            
            return offset;    
        }else{ // place at end
            return offset;    
        }  
    }else{
        float f_key_data;
        unsigned key_data;
        if(attr.type == TypeInt){
            memcpy(&key_data, key, sizeof(unsigned));
        }else{
             memcpy(&f_key_data, key, sizeof(float));
        }
        
        while(offset < free_space_pointer){
            float f_num_data;
            unsigned num_data;
            if(attr.type == TypeInt){
                memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
            }else{
                memcpy(&f_num_data, (char *)page + offset, sizeof(float));
            }
            offset += sizeof(unsigned);
            
            if(attr.type == TypeInt){
                if(key_data < num_data){
                    offset -= sizeof(unsigned);
                    break;
                }
            }else{
                if(f_key_data < f_num_data){
                    offset -= sizeof(float);
                    break;
                }
            }
            
            offset += sizeof(RID);
        }
        
        if(offset < free_space_pointer){ // place in middle
            memmove((char *)page + offset + de_size, (char *)page + offset, free_space_pointer - offset);
            
            return offset;    
        }else{ // place at end
            return offset;    
        }  
    }
}

void set_leaf_entry(void *page, const Attribute &attr, const unsigned de_size, const void *key, const RID &rid)
{
    unsigned entry_offset = prepare_entries(page, attr, de_size, key, 1);
    
    /* get entry offset in page */
    unsigned free_space;
    // put free space pointer into entry_offset
    memcpy(&free_space, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));
    
    /* set entry into page */
    unsigned key_size = de_size - sizeof(RID);
    memcpy((char *)page + entry_offset, key, key_size);
    memcpy((char *)page + entry_offset + key_size, &rid, sizeof(RID));
    
    /* increment number of entries */
    unsigned num_entries;
    memcpy(&num_entries, (char *)page + 2 * sizeof(unsigned), sizeof(unsigned));
    num_entries++;
    memcpy((char *)page + 2 * sizeof(unsigned), &num_entries, sizeof(unsigned));
    
    /* write new free space offset */
    free_space += de_size;
    memcpy((char *)page + 3 * sizeof(unsigned), &free_space, sizeof(unsigned));
}

void set_root_entry(void *page, const Attribute &attr, const unsigned de_size, const void *key, const RID &rid)
{
    unsigned entry_offset = prepare_entries(page, attr, de_size, key, 0);
    
    /* get entry offset in page */
    unsigned free_space;
    // put free space pointer into entry_offset
    memcpy(&free_space, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));
    
    /* set entry into page */
    unsigned key_size = de_size - sizeof(RID);
    memcpy((char *)page + entry_offset, key, key_size);
    memcpy((char *)page + entry_offset + key_size, &rid, sizeof(RID));
    
    /* increment number of entries */
    unsigned num_entries;
    memcpy(&num_entries, (char *)page + sizeof(idir), sizeof(unsigned));
    num_entries++;
    memcpy((char *)page + sizeof(idir), &num_entries, sizeof(unsigned));
    
    /* write new free space offset */
    free_space += de_size;
    memcpy((char *)page + sizeof(idir) + sizeof(unsigned), &free_space, sizeof(unsigned));
}

unsigned prepare_tc_ptrs(void *page, const Attribute &attr, const unsigned de_size, const void *key)
{   
    unsigned nchildren;
    memcpy(&nchildren, (char *)page, sizeof(idir));
    
    unsigned free_space_pointer = sizeof(idir) + sizeof(unsigned);
    
    for(unsigned i = 1; i < nchildren; ++i){
        if(attr.type == TypeVarChar){
            unsigned char_len;
            memcpy(&char_len, (char *)page + free_space_pointer, sizeof(unsigned));
            free_space_pointer += char_len;
        }
        
        free_space_pointer += 2 * sizeof(unsigned);
    }
    
    unsigned offset = sizeof(idir) + sizeof(unsigned);
    
    if(attr.type == TypeVarChar){
        unsigned key_size;
        memcpy(&key_size, key, sizeof(unsigned));
        
        void *key_data = malloc(key_size);
        memcpy(key_data, (char *)key + sizeof(unsigned), key_size);
        
        while(offset < free_space_pointer){
            unsigned char_size;
            memcpy(&char_size, (char *)page + offset, sizeof(unsigned));
            offset += sizeof(unsigned);
        
            void *char_data = malloc(char_size);
            memcpy(char_data, (char *)page + offset, char_size);
            offset += char_size;
        
            unsigned max_size = max(key_size, char_size);
            int comp = memcmp(key_data, char_data, max_size);
            
            if(comp < 0){
                offset -= sizeof(unsigned);
                offset -= char_size;
                break;
            }
            offset += sizeof(unsigned);
            
            free(char_data);
        }
        free(key_data);
        
        if(offset < free_space_pointer){ // move data entries so new de can be placed in the middle
            memmove((char *)page + offset + de_size - sizeof(RID) + sizeof(unsigned), (char *)page + offset, free_space_pointer - offset);
            
            return offset;    
        }else{ // place at end
            return offset;    
        }  
    }else{
        float f_key_data;
        unsigned key_data;
        if(attr.type == TypeInt){
            memcpy(&key_data, key, sizeof(unsigned));
        }else{
             memcpy(&f_key_data, key, sizeof(float));
        }
        
        while(offset < free_space_pointer){
            float f_num_data;
            unsigned num_data;
            if(attr.type == TypeInt){
                memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
            }else{
                memcpy(&f_num_data, (char *)page + offset, sizeof(float));
            }
            offset += sizeof(unsigned);
            
            if(attr.type == TypeInt){
                if(key_data < num_data){
                    offset -= sizeof(unsigned);
                    break;
                }
            }else{
                if(f_key_data < f_num_data){
                    offset -= sizeof(float);
                    break;
                }
            }
            
            offset += sizeof(unsigned);
        }
        
        if(offset < free_space_pointer){ // place in middle
            memmove((char *)page + offset + de_size - sizeof(RID) + sizeof(unsigned), (char *)page + offset, free_space_pointer - offset);
            
            return offset;    
        }else{ // place at end
            return offset;    
        }  
    }
}

void set_tc_ptr(void *page, const Attribute &attr, const unsigned de_size, const void *key, const unsigned leaf_ptr)
{
    unsigned offset = prepare_tc_ptrs(page, attr, de_size, key);
    
    /* set tc_ptr into page */
    unsigned key_size = de_size - sizeof(RID);
    memcpy((char *)page + offset, key, key_size);
    memcpy((char *)page + offset + key_size, &leaf_ptr, sizeof(unsigned));
    
    /* increment number of children */
    unsigned nchildren;
    memcpy(&nchildren, page, sizeof(idir));
    nchildren++;
    memcpy(page, &nchildren, sizeof(idir));
}

void IndexManager::initialize_leaves_from_root(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, 
                                 const void *key, const RID &rid, void *page)
{
    /* set up second leaf page */
    void *second_page = calloc(PAGE_SIZE, 1);
    if (second_page == NULL){
        return;
    }
    
    unsigned free_space_pointer;
    memcpy(&free_space_pointer, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));
    
    unsigned num_entries;
    memcpy(&num_entries, (char *)page + sizeof(idir), sizeof(unsigned));
    
    unsigned half_entries = (unsigned)ceil((((float)num_entries) / 2.0));
    unsigned entry_count = 0;
    
    unsigned offset = sizeof(idir) + 2 * sizeof(unsigned);
    
    while(entry_count < half_entries){
        if(attr.type == TypeVarChar){
            unsigned char_len;
            memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
            offset += char_len;
        }
        
        offset += sizeof(unsigned);
        offset += sizeof(RID);
        
        entry_count++;
    }
    
    /* set up root page */
    void *root_page = calloc(PAGE_SIZE, 1);
    if (root_page == NULL){
        return;
    }
    
    /* set up root page data */
    unsigned is_left = 0; // 0 = false, 1 = true
    unsigned l_child_ptr = 1;
    unsigned r_child_ptr = 2;
    memcpy((char *)root_page + sizeof(idir), &l_child_ptr, sizeof(unsigned));
    if(attr.type == TypeVarChar){
        unsigned char_len;
        memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
        
        void *char_data = malloc(char_len);
        memcpy(char_data, (char *)page + offset + sizeof(unsigned), char_len);
        
        unsigned key_len;
        memcpy(&key_len, key, sizeof(unsigned));
        
        void *key_data = malloc(key_len);
        memcpy(key_data, (char *)key + sizeof(unsigned), key_len);
        
        memcpy((char *)root_page + sizeof(idir) + sizeof(unsigned), (char *)page + offset, sizeof(unsigned));
        memcpy((char *)root_page + sizeof(idir) + 2 * sizeof(unsigned), (char *)page + offset + sizeof(unsigned), char_len);
        
        memcpy((char *)root_page + sizeof(idir) + sizeof(unsigned) + char_len, &r_child_ptr, sizeof(unsigned));
        
        /* compare traffic cop value to new key */
        unsigned max_size = max(key_len, char_len);
        int comp = memcmp(key_data, char_data, max_size);
        if(comp < 0){
            is_left = 1;
        }
        
        free(key_data);
        free(char_data);
    }else{    
        memcpy((char *)root_page + sizeof(idir) + sizeof(unsigned), (char *)page + offset, sizeof(unsigned));   
        
        memcpy((char *)root_page + sizeof(idir) + 2 * sizeof(unsigned), &r_child_ptr, sizeof(unsigned));
        
        /* compare traffic cop value to new key */
        float f_num_data;
        float f_key_data;
        unsigned num_data;
        unsigned key_data;
        if(attr.type == TypeInt){
            memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
            memcpy(&key_data, key, sizeof(unsigned));
        }else{
            memcpy(&f_num_data, (char *)page + offset, sizeof(float));
            memcpy(&f_key_data, key, sizeof(float));
        }
            
        if(attr.type == TypeInt){
            if(key_data < num_data){
                is_left = 1;
            }
        }else{
            if(f_key_data < f_num_data){
                is_left = 1;
            }
        }
    }
    
    /*  set up directory of second leaf page */
    ldir scnd_leaf_dir;
    scnd_leaf_dir.lptr = 1;
    scnd_leaf_dir.rptr = 0;
    scnd_leaf_dir.nentries = num_entries - half_entries;
    scnd_leaf_dir.free_space_offset = sizeof(ldir) + (free_space_pointer - offset);
    
    memcpy(second_page, &scnd_leaf_dir, sizeof(ldir));
    /* copy actual data from first leaf page into second leaf page */
    memcpy((char *)second_page + sizeof(ldir), (char *)page + offset, free_space_pointer - offset);
    
    /* set up first leaf page by moving data to accomodate new directory */
    memmove((char *)page + sizeof(ldir), (char *)page + sizeof(idir) + 2 * sizeof(unsigned), offset - (sizeof(idir) + 2 * sizeof(unsigned)));
    
    /* set up directory of first leaf page */
    ldir leaf_dir;
    leaf_dir.lptr = 0;
    leaf_dir.rptr = 2;
    leaf_dir.nentries = entry_count;
    leaf_dir.free_space_offset = sizeof(ldir) + (offset - (sizeof(idir) + 2 * sizeof(unsigned)));
    memcpy(page, &leaf_dir, sizeof(ldir));
    
    /* set up directory of root page */
    idir root_dir;
    root_dir.nchildren = 2;
    memcpy(root_page, &root_dir, sizeof(idir));
    
    /* insert new element */
    if(is_left != 0){ // insert into left leaf
        set_root_entry(page, attr, de_size, key, rid);
    }else{ // insert into right leaf
        set_root_entry(second_page, attr, de_size, key, rid);
    }
    
    /* write pages to file */
    if ((*ixfileHandle.get_handle()).appendPage(page)){
        free(second_page);
        free(root_page);
        return;
    }
    if ((*ixfileHandle.get_handle()).appendPage(second_page)){
        free(second_page);
        free(root_page);
        return;
    }
    if ((*ixfileHandle.get_handle()).writePage(0, root_page)){
        free(second_page);
        free(root_page);
        return;
    }
    
    levels += 1;
    
    free(root_page);
    free(second_page);
}

void IndexManager::split_index(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, const void *key, const unsigned leaf_ptr)
{
    /* set up second index page */
    void *second_page = calloc(PAGE_SIZE, 1);
    if (second_page == NULL){
        return;
    }
    
    /* get page to be split */
    void *page = calloc(PAGE_SIZE, 1);
    if (page == NULL){
        free(second_page);
        return;
    }
    
    unsigned index_ptr = 0;
    if(path.size() != 0){
        index_ptr = path.back();
        path.pop_back();
    }
    
    if ((*ixfileHandle.get_handle()).readPage(index_ptr, page)){
        free(page);
        free(second_page);
        return;
    }
    
    /* figure out where new traffic cop goes */
    unsigned nchildren;
    memcpy(&nchildren, (char *)page, sizeof(idir));
    
    unsigned half_entries = (unsigned)ceil((((float)nchildren) / 2.0));
    unsigned entry_count = 0;
    
    unsigned offset = sizeof(idir) + sizeof(unsigned);
    
    unsigned found = 0; // 0 = not found, !0 = found
    
    unsigned char_len;
    while(entry_count < half_entries){
        if(found == 0){
            if(attr.type == TypeVarChar){
                //unsigned char_len;
                memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
            
                void *char_data = malloc(char_len);
                memcpy(char_data, (char *)page + offset + sizeof(unsigned), char_len);
            
                unsigned key_len;
                memcpy(&key_len, key, sizeof(unsigned));
            
                void *key_data = malloc(key_len);
                memcpy(key_data, (char *)key + sizeof(unsigned), key_len);
            
                unsigned max_size = max(key_len, char_len);
                int comp = memcmp(key_data, char_data, max_size);
                if(comp < 0){
                    entry_count++;
                    found = offset;
                }
            
                offset += char_len;
            }else if(attr.type == TypeInt){
                unsigned num_data;
                unsigned key_data;
                memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
                memcpy(&key_data, key, sizeof(unsigned));
            
                if(key_data < num_data){
                    entry_count++;
                    found = offset;
                }
            }else{
                float num_data;
                float key_data;
                memcpy(&num_data, (char *)page + offset, sizeof(float));
                memcpy(&key_data, key, sizeof(float));
            
                if(key_data < num_data){
                    entry_count++;
                    found = offset;
                }
            }
        }
        
        offset += 2 * sizeof(unsigned);
        
        entry_count++;
    }
    
    /* set up directory of second index page */
    idir scnd_idir;
    scnd_idir.nchildren = nchildren - entry_count;
    set_idir(second_page, scnd_idir);
    
    /* copy actual data from first index page into second index page */
    unsigned free_space_pointer = offset;
    for(unsigned i = entry_count; i <= scnd_idir.nchildren; ++i){
        if(attr.type == TypeVarChar){
            unsigned xchar_len;
            memcpy(&xchar_len, (char *)page + free_space_pointer, sizeof(unsigned));
            free_space_pointer += xchar_len;
        }
        
        free_space_pointer += 2 * sizeof(unsigned);
    }
    
    offset -= sizeof(unsigned);
    memcpy((char *)second_page + sizeof(idir), (char *)page + offset, free_space_pointer - offset);
    
    /* set up directory of first index page */
    idir index_dir;
    
    /* insert new traffic cop into correct index */
    if(found < offset && found != 0){ // first page
        index_dir.nchildren = entry_count - 2;
        set_tc_ptr(page, attr, de_size, key, leaf_ptr);
    }else{ // second page
        set_tc_ptr(second_page, attr, de_size, key, leaf_ptr);
    }
    
    index_dir.nchildren = entry_count - 1;
    set_idir(page, index_dir);
    
    /* check to see if parent is full, and recurse if so.  recursion ends when
       either the parent is not full or the root needs to split */
    
    /* check whether or not current index is root */
    if(index_ptr == 0){ // the root was split
        levels++;
        
        unsigned first_page_index = (*ixfileHandle.get_handle()).getNumberOfPages();
        
        /* append first new index page */
        if ((*ixfileHandle.get_handle()).appendPage(page)){
            free(page);
            free(second_page);
            return;
        }
        
        unsigned second_page_index = (*ixfileHandle.get_handle()).getNumberOfPages();
        
        /* append second new index page */
        if ((*ixfileHandle.get_handle()).appendPage(second_page)){
            free(page);
            free(second_page);
            return;
        }
        
        /* set up new root page */
        void *root_page = calloc(PAGE_SIZE, 1);
        if (root_page == NULL){
            return;
        }
        
        /* set up root directory */
        idir root_idir;
        root_idir.nchildren = 2;
        set_idir(root_page, root_idir);
        
        /* set up data in root */
        memcpy((char *)root_page + sizeof(idir), &first_page_index, sizeof(unsigned));
        // push up middle traffic cop from split
        if(attr.type == TypeVarChar){
            offset -= char_len;
            memcpy((char *)root_page + sizeof(idir) + sizeof(unsigned), &char_len, sizeof(unsigned));
            
            memcpy((char *)root_page + sizeof(idir) + 2 * sizeof(unsigned), (char *)page + offset, char_len);
            memcpy((char *)root_page + sizeof(idir) + 2 * sizeof(unsigned) + char_len, &second_page_index, sizeof(unsigned));
        }else{
            memcpy((char *)root_page + sizeof(idir) + sizeof(unsigned), (char *)page + offset - sizeof(unsigned), sizeof(unsigned));
            memcpy((char *)root_page + sizeof(idir) + 2 * sizeof(unsigned), &second_page_index, sizeof(unsigned));
        }
        
        /* write to root page */
        if ((*ixfileHandle.get_handle()).writePage(0, root_page)){
            free(page);
            free(second_page);
            free(root_page);
            return;
        }
        
        free(root_page);
    }else{ // non root index was split
        /* get parent page */
        void *parent_page = calloc(PAGE_SIZE, 1);
        if (parent_page == NULL){
            free(page);
            free(second_page);
            return;
        }
    
        unsigned parent_ptr = 0;
        if(path.size() != 0){
            parent_ptr = path.back();
        }
    
        if ((*ixfileHandle.get_handle()).readPage(parent_ptr, parent_page)){
            free(page);
            free(second_page);
            return;
        }
        
        /* check if parent index has enough space for the new index */
        void *traffic_cop;
        unsigned tc_size;
        if(attr.type == TypeVarChar){
            traffic_cop = malloc(char_len);
            offset -= char_len;
            offset -= sizeof(unsigned);
            memcpy(traffic_cop, (char *)page + offset, char_len + sizeof(unsigned));
            
            tc_size = get_data_entry_size(attr, traffic_cop);
        }else{
            traffic_cop = malloc(sizeof(unsigned));
            memcpy(traffic_cop, (char *)page + offset - sizeof(unsigned), sizeof(unsigned));
        
            tc_size = get_data_entry_size(attr, traffic_cop);
        }
        
        if (getIndexPageFreeSpaceSize(parent_page) >= tc_size - sizeof(RID) + sizeof(unsigned)){ // parent has enough space
            unsigned second_page_index = (*ixfileHandle.get_handle()).getNumberOfPages();
        
            /* append second index page */
            if ((*ixfileHandle.get_handle()).appendPage(second_page)){
                free(second_page);
                return;
            }
            
            /* insert new traffic cop and ptr into parent */
            set_tc_ptr(parent_page, attr, tc_size, traffic_cop, second_page_index);
            
            /* write to first index page */
            if ((*ixfileHandle.get_handle()).writePage(index_ptr, page)){
                free(page);
                free(second_page);
                return;
            }
        
            if ((*ixfileHandle.get_handle()).writePage(parent_ptr, parent_page)){
                free(page);
                free(parent_page);
                free(second_page);
                return;
            }
            
            
        }else{ // no space in parent
            unsigned second_page_index = (*ixfileHandle.get_handle()).getNumberOfPages();
        
            /* append second index page */
            if ((*ixfileHandle.get_handle()).appendPage(second_page)){
                free(second_page);
                return;
            }
            
            /* write to first index page */
            if ((*ixfileHandle.get_handle()).writePage(index_ptr, page)){
                free(page);
                free(second_page);
                return;
            }
            
            split_index(ixfileHandle, attr, tc_size, traffic_cop, second_page_index);
        }
        free(parent_page);
    }
    
    free(page);
    free(second_page);
}

void IndexManager::split_leaf(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, 
                                 const void *key, const RID &rid, const unsigned parent_ptr, const unsigned ind_ptr, void *parent_page, void *page)
{
    /* set up second leaf page */
    void *second_page = calloc(PAGE_SIZE, 1);
    if (second_page == NULL){
        return;
    }
    
    unsigned free_space_pointer;
    memcpy(&free_space_pointer, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));
    
    unsigned num_entries;
    memcpy(&num_entries, (char *)page + 2 * sizeof(unsigned), sizeof(unsigned));
    
    unsigned half_entries = (unsigned)ceil((((float)num_entries) / 2.0));
    unsigned entry_count = 0;
    
    unsigned offset = sizeof(ldir);
    
    while(entry_count < half_entries){
        if(attr.type == TypeVarChar){
            unsigned char_len;
            memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
            offset += char_len;
        }
        
        offset += sizeof(unsigned);
        offset += sizeof(RID);
        
        entry_count++;
    }
    
    /* get directory of first leaf page */
    ldir leaf_dir = get_ldir(page);
    
    /*  set up directory of second leaf page */
    ldir scnd_leaf_dir;
    scnd_leaf_dir.lptr = ind_ptr;
    scnd_leaf_dir.rptr = leaf_dir.rptr;
    scnd_leaf_dir.nentries = num_entries - half_entries;
    scnd_leaf_dir.free_space_offset = sizeof(ldir) + (free_space_pointer - offset);
    memcpy(second_page, &scnd_leaf_dir, sizeof(ldir));
    
    /* copy actual data from first leaf page into second leaf page */
    memcpy((char *)second_page + sizeof(ldir), (char *)page + offset, free_space_pointer - offset);
    
    /* set up directory of first leaf page */
    leaf_dir.rptr = (*ixfileHandle.get_handle()).getNumberOfPages();
    leaf_dir.nentries = entry_count;
    leaf_dir.free_space_offset = sizeof(ldir) + (offset - sizeof(ldir));
    memcpy(page, &leaf_dir, sizeof(ldir));
    
    /* figure out which leaf to put new data entry into */
    unsigned is_left = 0; // 0 = false, 1 = true
    if(attr.type == TypeVarChar){
        unsigned char_len;
        memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
        
        void *char_data = malloc(char_len);
        memcpy(char_data, (char *)page + offset + sizeof(unsigned), char_len);
        
        unsigned key_len;
        memcpy(&key_len, key, sizeof(unsigned));
        
        void *key_data = malloc(key_len);
        memcpy(key_data, (char *)key + sizeof(unsigned), key_len);

        /* compare traffic cop value to new key */
        unsigned max_size = max(key_len, char_len);
        int comp = memcmp(key_data, char_data, max_size);
        if(comp < 0){
            is_left = 1;
        }
        
        free(key_data);
        free(char_data);
    }else{    
        /* compare traffic cop value to new key */
        float f_num_data;
        float f_key_data;
        unsigned num_data;
        unsigned key_data;
        if(attr.type == TypeInt){
            memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
            memcpy(&key_data, key, sizeof(unsigned));
        }else{
            memcpy(&f_num_data, (char *)page + offset, sizeof(float));
            memcpy(&f_key_data, key, sizeof(float));
        }
            
        if(attr.type == TypeInt){
            if(key_data < num_data){
                is_left = 1;
            }
        }else{
            if(f_key_data < f_num_data){
                is_left = 1;
            }
        }
    }
    
    /* insert new element */
    if(is_left != 0){ // insert into left leaf
        set_leaf_entry(page, attr, de_size, key, rid);
    }else{ // insert into right leaf
        set_leaf_entry(second_page, attr, de_size, key, rid);
    }
    
    /* check if parent index has enough space for the new leaf */
    void *traffic_cop;
    unsigned tc_size;
    if(attr.type == TypeVarChar){
        unsigned char_len;
        memcpy(&char_len, (char *)second_page + sizeof(ldir), sizeof(unsigned));
            
        traffic_cop = malloc(char_len);
        memcpy(traffic_cop, (char *)second_page + sizeof(ldir) + sizeof(unsigned), char_len);
            
        tc_size = get_data_entry_size(attr, traffic_cop);
    }else{
        traffic_cop = malloc(sizeof(unsigned));
        memcpy(traffic_cop, (char *)second_page + sizeof(ldir), sizeof(unsigned));
        
        tc_size = get_data_entry_size(attr, traffic_cop);
    }
        
    if (getIndexPageFreeSpaceSize(parent_page) >= tc_size - sizeof(RID) + sizeof(unsigned)){ // parent has enough space
        /* insert new traffic cop and ptr into parent */
        set_tc_ptr(parent_page, attr, tc_size, traffic_cop, leaf_dir.rptr);
        
        if ((*ixfileHandle.get_handle()).writePage(parent_ptr, parent_page)){
            free(second_page);
            return;
        }
    }else{ // parent does not have enough space
        // split parent
        split_index(ixfileHandle, attr, tc_size, traffic_cop, leaf_dir.rptr);
    }
    
    /* write pages to file */
    if ((*ixfileHandle.get_handle()).writePage(ind_ptr, page)){
        free(second_page);
        return;
    }
    if ((*ixfileHandle.get_handle()).appendPage(second_page)){
        free(second_page);
        return;
    }
    free(second_page);
}

void IndexManager::print_index(IXFileHandle &ixfileHandle, void * pageData, const unsigned nchildren, const Attribute &attribute, unsigned next_level) const
{
    cout << "\"keys\":[";
    /* print traffic cops */
    unsigned offset = 2 * sizeof(unsigned);
        
    for(unsigned i = 1; i < nchildren; ++i){
        cout << "\"";
        switch(attribute.type){
            case TypeInt:
                unsigned num_data;
                memcpy(&num_data, (char *)pageData + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
                cout << num_data;
                break;
            case TypeReal:
                float float_data;
                memcpy(&float_data, (char *)pageData + offset, sizeof(float));
                offset += sizeof(float);
                cout << float_data;
                break;
            case TypeVarChar:
                unsigned char_len;
                memcpy(&char_len, (char *)pageData + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
                    
                void * char_data = malloc(char_len);
                memcpy(char_data, (char *)pageData + offset, char_len);
                offset += char_len;
                cout << (char *)char_data << '\0';
                free(char_data);
                break;
        }
        offset += sizeof(unsigned);
        cout << "\"";
        if(i != nchildren - 1){
            cout << ",";
        }
    }
    cout << "]," << endl;
        
    cout << "\"children\":[" << endl;
    offset = sizeof(unsigned);
    for(unsigned i = 0; i < nchildren; ++i){
        unsigned ind_ptr;
        memcpy(&ind_ptr, (char *)pageData + offset, sizeof(unsigned));
        offset += sizeof(unsigned);
        if(i != nchildren - 1){
            if(attribute.type == TypeVarChar){
                unsigned char_len;
                memcpy(&char_len, (char *)pageData + offset, sizeof(unsigned));
                
                void * char_data = malloc(char_len);
                memcpy(char_data, (char *)pageData + offset + sizeof(unsigned), char_len);
                offset += char_len;
                free(char_data);
            }
            offset += sizeof(unsigned);
        }
            
        /* get page */
        void *page = malloc(PAGE_SIZE);
        if (page == NULL)
            return;
    
        if ((*ixfileHandle.get_handle()).readPage(ind_ptr, page)){
            free(page);
            return;
        }
            
        if(next_level != levels){ // print index
            unsigned new_nchildren = get_num_children(page);
            
            print_index(ixfileHandle, page, new_nchildren, attribute, next_level + 1);
        }else{ // print leaf
            print_leaf(page, attribute, 1);
        }
            
        free(page);
    }
    cout << "]" << endl;
}

void IndexManager::find_leaf(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid, void *page, const unsigned nchildren, const unsigned parent_ptr, const unsigned next_level)
{
     unsigned offset = 2 * sizeof(unsigned);
        
    for(unsigned i = 1; i < nchildren; ++i){
        switch(attribute.type){
            case TypeInt:
                unsigned num_data;
                memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
                
                // compare
                unsigned key_data;
                memcpy(&key_data, key, sizeof(unsigned));
                if(key_data < num_data){
                    unsigned ind_ptr;
                    memcpy(&ind_ptr, (char *)page + offset - 2 * sizeof(unsigned), sizeof(unsigned));
                    
                    /* get page */
                    void *new_page = malloc(PAGE_SIZE);
                    if (new_page == NULL)
                        return;
    
                    if ((*ixfileHandle.get_handle()).readPage(ind_ptr, new_page)){
                        free(new_page);
                        return;
                    }
                    
                    if(next_level == levels){
                        // insert into leaf (split if doesnt fit)
                        unsigned deSize = get_data_entry_size(attribute, key);
                        if (getLeafPageFreeSpaceSize(new_page) >= deSize){ // leaf has enough space
                            set_leaf_entry(new_page, attribute, deSize, key, rid);
            
                            /* write page to file */
                            if ((*ixfileHandle.get_handle()).writePage(ind_ptr, new_page)){
                                free(new_page);
                                return;
                            }
                        }else{ // leaf needs to split
                            split_leaf(ixfileHandle, attribute, deSize, key, rid, parent_ptr, ind_ptr, page, new_page);
                        }
                    }else{
                        path.push_back(ind_ptr);
                        
                        // call find_leaf on the ptr
                        unsigned new_nchildren = get_num_children(new_page);
                        
                        find_leaf(ixfileHandle, attribute, key, rid, new_page, new_nchildren, ind_ptr, next_level + 1);
                    }
                    free(new_page);
                    return;
                }
                offset += sizeof(unsigned);
                break;
            case TypeReal:
                float float_data;
                memcpy(&float_data, (char *)page + offset, sizeof(float));
                offset += sizeof(float);
                // compare
                float f_key_data;
                memcpy(&f_key_data, key, sizeof(float));
                if(f_key_data < float_data){
                    unsigned ind_ptr;
                    memcpy(&ind_ptr, (char *)page + offset - sizeof(unsigned) - sizeof(float), sizeof(unsigned));
                    
                    /* get page */
                    void *new_page = malloc(PAGE_SIZE);
                    if (new_page == NULL)
                        return;
    
                    if ((*ixfileHandle.get_handle()).readPage(ind_ptr, new_page)){
                        free(new_page);
                        return;
                    }
                    
                    if(next_level == levels){
                        // insert into leaf (split if doesnt fit)
                        unsigned deSize = get_data_entry_size(attribute, key);
                        if (getLeafPageFreeSpaceSize(new_page) >= deSize){ // leaf has enough space
                            set_leaf_entry(new_page, attribute, deSize, key, rid);
            
                            /* write page to file */
                            if ((*ixfileHandle.get_handle()).writePage(ind_ptr, new_page)){
                                free(new_page);
                                return;
                            }
                        }else{ // leaf needs to split
                            split_leaf(ixfileHandle, attribute, deSize, key, rid, parent_ptr, ind_ptr, page, new_page);
                        }
                    }else{
                        path.push_back(ind_ptr);
                        
                        // call find_leaf on the ptr
                        unsigned new_nchildren = get_num_children(new_page);
                        
                        find_leaf(ixfileHandle, attribute, key, rid, new_page, new_nchildren, ind_ptr, next_level + 1);
                    }
                    free(new_page);
                    return;
                }
                offset += sizeof(unsigned);
                break;
            case TypeVarChar:
                unsigned char_len;
                memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
                    
                void * char_data = malloc(char_len);
                memcpy(char_data, (char *)page + offset, char_len);
                offset += char_len;
                // compare
                unsigned key_len;
                memcpy(&key_len, key, sizeof(unsigned));
        
                void *key_char_data = malloc(key_len);
                memcpy(key_char_data, (char *)key + sizeof(unsigned), key_len);

                /* compare traffic cop value to new key */
                unsigned max_size = max(key_len, char_len);
                int comp = memcmp(key_char_data, char_data, max_size);
                if(comp < 0){
                    unsigned ind_ptr;
                    memcpy(&ind_ptr, (char *)page + offset - 2 * sizeof(unsigned) - char_len, sizeof(unsigned));
                    
                    /* get page */
                    void *new_page = malloc(PAGE_SIZE);
                    if (new_page == NULL)
                        return;
    
                    if ((*ixfileHandle.get_handle()).readPage(ind_ptr, new_page)){
                        free(new_page);
                        return;
                    }
                    
                    if(next_level == levels){
                        // insert into leaf (split if doesnt fit)
                        unsigned deSize = get_data_entry_size(attribute, key);
                        if (getLeafPageFreeSpaceSize(new_page) >= deSize){ // leaf has enough space
                            set_leaf_entry(new_page, attribute, deSize, key, rid);
            
                            /* write page to file */
                            if ((*ixfileHandle.get_handle()).writePage(ind_ptr, new_page)){
                                free(new_page);
                                return;
                            }
                        }else{ // leaf needs to split
                            split_leaf(ixfileHandle, attribute, deSize, key, rid, parent_ptr, ind_ptr, page, new_page);
                        }
                    }else{
                        path.push_back(ind_ptr);
                        
                        // call find_leaf on the ptr
                        unsigned new_nchildren = get_num_children(new_page);
                        
                        find_leaf(ixfileHandle, attribute, key, rid, new_page, new_nchildren, ind_ptr, next_level + 1);
                    }
                    free(new_page);
                    return;
                }
                offset += sizeof(unsigned);
                
                free(key_char_data);
                free(char_data);
                break;
        }
        offset += sizeof(unsigned);
    }
}

RC search_and_delete(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid, const unsigned current_level, const unsigned current_ptr)
{
    /* check if this is a leaf */
    if(current_level == levels){ // this is a leaf page
        // search leaf page and delete if found
        return 0;
    }
    
    /* search and recurse until a leaf is found */
    /* get index page */
    void *page = malloc(PAGE_SIZE);
    if (page == NULL)
        return 1;
    
    if ((*ixfileHandle.get_handle()).readPage(current_ptr, page)){
        free(page);
        return 1;
    }
    
    /* check how many children the root page has */
    unsigned nchildren = get_num_children(page);
    
    unsigned offset = sizeof(idir) + sizeof(unsigned);
    
    for(unsigned i = 0; i < nchildren - 2; ++i){
        switch(attr.type){
            case TypeInt:
                unsigned key_data;
                memcpy(&key_data, key, sizeof(unsigned));
                
                unsigned num_data;
                memcpy(&num_data, (char *)page + offset, sizeof(unsigned));
                
                if(key_data < num_data){
                    unsigned index_ptr;
                    memcpy(&index_ptr, (char *)page + offset - sizeof(unsigned), sizeof(unsigned));
                    
                    search_and_delete(ixfileHandle, attribute, key, rid, current_level + 1, index_ptr);
                    return 0;
                }
                offset += 2 * sizeof(unsigned);
                break;
            case TypeReal:
                float f_key_data;
                memcpy(&f_key_data, key, sizeof(float));
                
                float f_num_data;
                memcpy(&f_num_data, (char *)page + offset, sizeof(float));
                
                if(f_key_data < f_num_data){
                    unsigned index_ptr;
                    memcpy(&index_ptr, (char *)page + offset - sizeof(unsigned), sizeof(unsigned));
                    
                    search_and_delete(ixfileHandle, attribute, key, rid, current_level + 1, index_ptr);
                    return 0;
                }
                offset += sizeof(float);
                offset += sizeof(unsigned);
                break;
            case TypeVarChar:
                unsigned char_len;
                memcpy(&char_len, (char *)page + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
                    
                void * char_data = malloc(char_len);
                memcpy(char_data, (char *)page + offset, char_len);
                offset += char_len;
                
                unsigned key_len;
                memcpy(&key_len, key, sizeof(unsigned));
        
                void *key_char_data = malloc(key_len);
                memcpy(key_char_data, (char *)key + sizeof(unsigned), key_len);

                unsigned max_size = max(key_len, char_len);
                int comp = memcmp(key_char_data, char_data, max_size);
                if(comp < 0){
                    unsigned index_ptr;
                    memcpy(&index_ptr, (char *)page + offset - sizeof(unsigned) - char_len, sizeof(unsigned));
                    
                    search_and_delete(ixfileHandle, attribute, key, rid, current_level + 1, index_ptr);
                    return 0;
                }    
                offset += sizeof(unsigned);
                break;
        }
    }
    
    /* search in last pointer */
    unsigned index_ptr;
    memcpy(&index_ptr, (char *)page + offset - sizeof(unsigned), sizeof(unsigned));
                    
    search_and_delete(ixfileHandle, attribute, key, rid, current_level + 1, index_ptr);
    
    free(page);
    return 0;
}

//------------------------------------------------------------------------
RC IndexManager::createFile(const string &fileName)
{
    if (_pf_manager->createFile(fileName))
        return 1;
    
    void * firstPageData = calloc(PAGE_SIZE, 1);
    if (firstPageData == NULL){
        return 1;
    }
    
    // set the root directory
    idir index_dir;
    index_dir.nchildren = 0;
    set_idir(firstPageData, index_dir);
    // because the root is the only page, give it #entries and free space offset fields
    unsigned num_entries = 0;
    memcpy ((char *)firstPageData + sizeof(idir), &num_entries, sizeof(unsigned));
    unsigned free_space_offset = sizeof(idir) + 2 * sizeof(unsigned);
    memcpy ((char *)firstPageData + sizeof(idir) + sizeof(unsigned), &free_space_offset, sizeof(unsigned));

    // Adds the first record based page.
    FileHandle handle;
    if (_pf_manager->openFile(fileName.c_str(), handle))
        return 1;
    if (handle.appendPage(firstPageData))
        return 1;
    _pf_manager->closeFile(handle);

    free(firstPageData);
    return 0;
}

RC IndexManager::destroyFile(const string &fileName)
{
    return _pf_manager->destroyFile(fileName);
}

RC IndexManager::openFile(const string &fileName, IXFileHandle &ixfileHandle)
{   
    if(ixfileHandle.get_handle() != NULL){
        return 2;
    }
    
    FileHandle *fh = new FileHandle;
    
    if (_pf_manager->openFile(fileName.c_str(), *fh)){
        delete fh;
        return 2;
    }
    
    ixfileHandle.set_handle(fh);
    
    return 0;
}

RC IndexManager::closeFile(IXFileHandle &ixfileHandle)
{
    if(ixfileHandle.get_handle() == NULL){
        return 3;
    }
    
    if(_pf_manager->closeFile(*ixfileHandle.get_handle()))
        return 3;
    
    delete ixfileHandle.get_handle();
    ixfileHandle.set_handle(NULL);
    
    return 0;
}

RC IndexManager::insertEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
    /* get root page */
    void *pageData = malloc(PAGE_SIZE);
    if (pageData == NULL)
        return 1;
    
    if ((*ixfileHandle.get_handle()).readPage(0, pageData)){
        free(pageData);
        return 1;
    }
    
    /* check how many children the root page has */
    unsigned nchildren = get_num_children(pageData);
    
    if(nchildren != 0){ // the root contains index node pointers
        /* find the correct leaf to place the data entry in */
        find_leaf(ixfileHandle, attribute, key, rid, pageData, nchildren, 0, 1);
        
    }else{ // the root contains data entries
        /* check if the root has enough space for the inserted entry */
        unsigned deSize = get_data_entry_size(attribute, key);
        if (getRootPageFreeSpaceSize(pageData) >= deSize){ // root has enough space
            /* set data entry into page */
            set_root_entry(pageData, attribute, deSize, key, rid);
            
            /* write page to file */
            if ((*ixfileHandle.get_handle()).writePage(0, pageData)){
                free(pageData);
                return 1;
            }
        }else{ // must create new leaf pages and make root an index page
            initialize_leaves_from_root(ixfileHandle, attribute, deSize, key, rid, pageData);
        }
    }
    
    free(pageData);
    return 0;
}

RC IndexManager::deleteEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid)
{
    return search_and_delete(ixfileHandle, attribute, key, rid, 0, 0);
}


RC IndexManager::scan(IXFileHandle &ixfileHandle,
        const Attribute &attribute,
        const void      *lowKey,
        const void      *highKey,
        bool			lowKeyInclusive,
        bool        	highKeyInclusive,
        IX_ScanIterator &ix_ScanIterator)
{
    return -1;
}

void IndexManager::printBtree(IXFileHandle &ixfileHandle, const Attribute &attribute) const 
{   
    /* get root page */
    void *pageData = malloc(PAGE_SIZE);
    if (pageData == NULL)
        return;
    
    if ((*ixfileHandle.get_handle()).readPage(0, pageData)){
        free(pageData);
        return;
    }
    
    /* check how many children the root page has */
    unsigned nchildren = get_num_children(pageData);
        
    cout << "{" << endl;
    
    if(nchildren != 0){ // root is an index
        unsigned next_level = 1;
        print_index(ixfileHandle, pageData, nchildren, attribute, next_level);
    }else{ // root is a leaf
        cout << "{\"keys\": [";
        print_leaf(pageData, attribute, 0); 
        cout << "]\"]}" << endl;
    }
    
    cout << "}" << endl;
    
    free(pageData);
}
//------------------------------------------------------------------------

void get_leaf_entry(void *page, const Attribute &attr, const unsigned flag, int entryNum, RID &rid, void *key )
{
    unsigned free_space;
    unsigned offset;
    if(flag == 0){ // root page
        memcpy(&free_space, (char *)page + sizeof(idir) + sizeof(unsigned), sizeof(unsigned));
    
        offset = sizeof(idir) + 2 * sizeof(unsigned);
    }else{ // leaf page
        memcpy(&free_space, (char *)page + 3 * sizeof(unsigned), sizeof(unsigned));
        
        offset = sizeof(ldir);
    }
    
    if(attr.type == TypeVarChar){
        void *last_char = malloc(attr.length + 1);
        unsigned char_size;
        
	int counter = 0;
        while(offset < free_space){
            /* get length of varchar */
            memcpy(&char_size, (char *)page + offset, sizeof(unsigned));
            offset += sizeof(unsigned);
            
            /* get key */
            key = malloc(char_size + 1);
            memcpy(key, (char *)page + offset, char_size);
            offset += char_size;
            ((char *)key)[char_size] = '\0';
            
            
            memcpy(&rid, (char *)page + offset, sizeof(RID));
            offset += sizeof(RID);
            
            memcpy(last_char, key, char_size + 1);
            if(entryNum == counter){
		    return;
	    }
	    counter++;



            free(key);
        }
        free(last_char);
    }else{
	    /*
        unsigned key_data;
        unsigned last_key;
        float f_key_data;
        float f_last_key;
	*/
        
	int counter = 0;
        while(offset < free_space){
             /* get key */
            if(attr.type == TypeInt){
                memcpy(key, (char *)page + offset, sizeof(unsigned));
                offset += sizeof(unsigned);
            }else{
                memcpy(key, (char *)page + offset, sizeof(float));
                offset += sizeof(float);
            }
            /*
            if(is_start != 1){
                if(attr.type == TypeInt){
                    is_same = memcmp(&last_key, &key_data, sizeof(unsigned));
                }else{
                    is_same = memcmp(&f_last_key, &f_key_data, sizeof(float));
                }
            }
	    */
            
            memcpy(&rid, (char *)page + offset, sizeof(RID));
            offset += sizeof(RID);
            
            if(counter == entryNum){
		    return;
	    }
	    counter++;
	    /*
            if(attr.type == TypeInt){
                last_key = key_data;
            }else{
                f_last_key = f_key_data;
            }
            
            is_start = 0;
	    */
        }    
    }
}

IX_ScanIterator::IX_ScanIterator()
{
    currentPageNum = 0;
	lastPageNum = 0;
	startFlag = 0;
	endOfFile = false;
}

IX_ScanIterator::~IX_ScanIterator()
{
    currentPageNum = 0;
	lastPageNum = 0;
}

RC IX_ScanIterator::getNextEntry(RID &rid, void *key)
{
	if(endOfFile){
		return IX_EOF;
	}

	 void *startPage = malloc(PAGE_SIZE);
	 memset(startPage, 0, PAGE_SIZE);
	 if((ixfileHandle->get_handle())->readPage(currentPageNum, startPage)){
		 free(startPage);
		  return -1;
	 }

	 void *endPage = malloc(PAGE_SIZE);
	 memset(endPage, 0, PAGE_SIZE);
	 if((ixfileHandle->get_handle())->readPage(lastPageNum, endPage)){
		 free(endPage);
		 return -1;
	 }

	 ldir leaf_dir = get_ldir(startPage);

    return -1;
}

RC IX_ScanIterator::close()
{
    return -1;
}


IXFileHandle::IXFileHandle()
{
    ixReadPageCounter = 0;
    ixWritePageCounter = 0;
    ixAppendPageCounter = 0;
    handle = NULL;
}

IXFileHandle::~IXFileHandle()
{
}

RC IXFileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = handle->readPageCounter;
    writePageCount = handle->writePageCounter;
    appendPageCount = handle->appendPageCounter;
    return 0;
}

void IXFileHandle::set_handle(FileHandle *fh)
{
    handle = fh;
}

FileHandle *IXFileHandle::get_handle()
{
    return handle;
}
