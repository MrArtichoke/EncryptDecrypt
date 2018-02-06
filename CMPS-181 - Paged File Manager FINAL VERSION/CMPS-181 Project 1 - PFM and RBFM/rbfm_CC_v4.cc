#include <iostream>
#include <math.h>
#include <string.h>
#include <iomanip>
#include "rbfm.h"

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
    RC rc;
    PagedFileManager *pfm;
    
    pfm = PagedFileManager::instance();
    
    rc = pfm->createFile(fileName);
    return rc;
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    RC rc;
    PagedFileManager *pfm;
    
    pfm = PagedFileManager::instance();
    
    rc = pfm->destroyFile(fileName);
    return rc;
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    RC rc;
    PagedFileManager *pfm;
    
    pfm = PagedFileManager::instance();
    
    rc = pfm->openFile(fileName, fileHandle);
    return rc;
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    RC rc;
    PagedFileManager *pfm;
    
    pfm = PagedFileManager::instance();
    
    rc = pfm->closeFile(fileHandle);
    return rc;
}

// code for byte[] -> float found here:
// stackoverflow.com/questions/3991478/building-a-32bit-float-out-of-its-4-composite-bytes-c
float bytes_to_float(const void *data, const unsigned index){
    float real_obj;
    char real_bytes[] = {((char*)data)[index], 
                                 ((char*)data)[index + 1], 
                                 ((char*)data)[index + 2], 
                                 ((char*)data)[index + 3],};
    memcpy(&real_obj, &real_bytes, sizeof(real_obj));
    return real_obj;
}

unsigned bytes_to_int(const void *data, const unsigned index){    
    void *int_data;
    unsigned int_obj;
    
    int_data = malloc(4);
    memcpy((char*)int_data, (char *)data + index, 4);
    int_obj = *(unsigned*)((void*)int_data);
    free(int_data);
    
    return int_obj;
}

RC insertrecord_insert_into_page(FileHandle &fileHandle, RID &rid, const void *vrecord, const unsigned record_length, const unsigned num_records, const unsigned free_space_pointer, const unsigned page_num){
    unsigned retval;
    unsigned new_free_space_pointer, new_num_records, new_slot;
    unsigned seek_offset;
    void *new_slot_directory, *record_metadata;
    
    // write record to page
    seek_offset = (page_num - 1) * 4096 + free_space_pointer;
    retval = fseek(fileHandle.handled_file, seek_offset, SEEK_SET);
    if(retval != 0){
        return -1;
    }
    
    retval = fwrite(vrecord, record_length, 1, fileHandle.handled_file);
    if(retval != 1){
        return -1;
    }
    
    // set up new slot directory
    new_slot_directory = malloc(8);
    new_num_records = num_records + 1;
    new_free_space_pointer = free_space_pointer + record_length;
    memcpy((char *)new_slot_directory, &new_num_records, sizeof(int));
    memcpy((char *)new_slot_directory + 4, &new_free_space_pointer, sizeof(int));

    // write number of slots and free space pointer
    seek_offset = page_num * 4096 - 8;
    retval = fseek(fileHandle.handled_file, seek_offset, SEEK_SET);
    if(retval != 0){
        return -1;
    }
        
    retval = fwrite(new_slot_directory, 8, 1, fileHandle.handled_file);
    if(retval != 1){
        return -1;
    }
    free(new_slot_directory);
    
    // add new record metadata to slot directory
    record_metadata = malloc(8);
    memcpy((char *)record_metadata, &free_space_pointer, sizeof(int));
    memcpy((char *)record_metadata + 4, &record_length, sizeof(int));
    
    new_slot = 8 + (8 * new_num_records);
    seek_offset = page_num * 4096 - new_slot;
    retval = fseek(fileHandle.handled_file, seek_offset, SEEK_SET);
    if(retval != 0){
        return -1;
    }
    
    retval = fwrite(record_metadata, 8, 1, fileHandle.handled_file);
    if(retval != 1){
        return -1;
    }
    free(record_metadata);
    
    // update rid
    rid.pageNum = page_num;
    rid.slotNum = new_num_records;
        
    return 0;
}

RC insertrecord_choose_page(FileHandle &fileHandle, RID &rid, const void *vrecord, const unsigned record_length){
    unsigned num_records, free_space_pointer;
    unsigned retval, free_space, num_pages, current;
    void *slot_directory;
    RC rc;
    
    num_pages = fileHandle.getNumberOfPages();
    
    if(num_pages != 0){ // attempt to insert record into last page
        // check if there is space in last page
        
        // get number of slots and free space pointer
        retval = fseek(fileHandle.handled_file, num_pages * 4096 - 8, SEEK_SET);
        if(retval != 0){
           return -1;
        }
        
        slot_directory = malloc(8);
        retval = fread(slot_directory, 8, 1, fileHandle.handled_file);
        
        num_records = bytes_to_int(slot_directory, 0);
        free_space_pointer = bytes_to_int(slot_directory, 4);
        
        free_space = (4096 - (8 + 8 * (num_records + 1))) - free_space_pointer;
        
        if((int)free_space >= (int)record_length){ // last page has enough space for record
            rc = insertrecord_insert_into_page(fileHandle, rid, vrecord, record_length, num_records, free_space_pointer, num_pages);
        }else{ // last page has no space for record
            for(current = 1; current <= num_pages; ++current){
                retval = fseek(fileHandle.handled_file, 4096 * current - 8, SEEK_SET);
                if(retval != 0){
                   return -1;
                }
                
                retval = fread(slot_directory, 8, 1, fileHandle.handled_file);
        
                num_records = bytes_to_int(slot_directory, 0);
                free_space_pointer = bytes_to_int(slot_directory, 4);
        
                free_space = (4096 - (8 + 8 * (num_records + 1))) - free_space_pointer;
                
                if((int)free_space >= (int)record_length){
                    rc = insertrecord_insert_into_page(fileHandle, rid, vrecord, record_length, num_records, free_space_pointer, current);
                    free(slot_directory);
                    return rc;
                }
            }
            // if no page has space, append a new page
            fileHandle.appendPage(vrecord);
            rc = insertrecord_insert_into_page(fileHandle, rid, vrecord, record_length, 0, 0, num_pages + 1);
        }
        free(slot_directory);
    }else{ // append a page
        fileHandle.appendPage(vrecord);
        rc = insertrecord_insert_into_page(fileHandle, rid, vrecord, record_length, 0, 0, 1);
    }
    return rc;
}

unsigned insertrecord_get_record_length(const vector<Attribute> &recordDescriptor, const void *data, int null_length){
    unsigned j, k;
    size_t i;
    unsigned record_length, bytes_seen, int_obj;
    
    record_length = 4; // first four bytes are number of fields
    
    bytes_seen = null_length;
    record_length += bytes_seen; // bytes of null indicator
    
    record_length += recordDescriptor.size() * 4; // 4 bytes for each pointer to field offset
    
    j = 0;
    k = 0;
    for(i = 0; i < recordDescriptor.size(); ++i){
        if(k > 7){
            j++;
            k = 0;
        }
        if((((char*)data)[j] & (1 << (7 - k))) == 1){ // null field
            continue;
        }
        k++;
        
        
        if(recordDescriptor[i].type == 2){ // char
            int_obj = bytes_to_int(data, bytes_seen);
            bytes_seen += 4;
            record_length += int_obj; // bytes for char field values
            bytes_seen += int_obj;
        }else{
            bytes_seen += 4;
            record_length += recordDescriptor[i].length; // bytes for int or real field values
        }
    }
    return record_length;
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    size_t i;
    unsigned bytes_seen, record_length, int_obj, l;
    unsigned num_fields, offset, null_length;
    unsigned  j, k, pointer_start;
    void *vrecord;
    RC rc;
    
    null_length = ceil(recordDescriptor.size() / 8.0);
    
    // get record length
    record_length = insertrecord_get_record_length(recordDescriptor, data, null_length);
    
    vrecord = malloc(record_length);
    
    offset = 0;
    
    // add number of fields to record 
    num_fields = recordDescriptor.size();
    memcpy((char *)vrecord + offset, &num_fields, sizeof(int));
    offset += sizeof(int);
    
    // add null indicator to record
    memcpy((char *)vrecord + offset, &((char *)data)[0], null_length);
    offset += null_length;
    
    // setup field pointers for adding to record
    pointer_start = offset;
    offset += recordDescriptor.size() * 4;
    
    
    // add field values to record
    bytes_seen = null_length;
    j = 0;
    k = 0;
    for(i = 0; i < recordDescriptor.size(); ++i){
        if(k > 7){
            j++;
            k = 0;
        }
        if((((char*)data)[j] & (1 << (7 - k))) == 1){ // null field
            continue;
        }
        k++;
        
        if(recordDescriptor[i].type == 2){ // char
            int_obj = bytes_to_int(data, bytes_seen);
            bytes_seen += 4;
            memcpy((char *)vrecord + pointer_start, &offset, 4); // place pointer into record
            pointer_start += 4;
            for(l = 0; l < int_obj; ++l){
                memcpy((char *)vrecord + offset, &((char*)data)[bytes_seen + l], 1);
                offset++;
            }
            bytes_seen += int_obj;
        }else{
            memcpy((char *)vrecord + pointer_start, &offset, 4); // place pointer into record
            pointer_start += 4;
            
            memcpy((char *)vrecord + offset, &((char*)data)[bytes_seen], 4);
            bytes_seen += 4;
            offset += 4;
        }
    }
    
    // choose page to place record in
    rc = insertrecord_choose_page(fileHandle, rid, vrecord, record_length);
    free(vrecord);
    return rc;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    unsigned retval;
    unsigned seek_offset;
    unsigned slot_pointer, record_length;
    void *slot_directory;
    void *buffer;
    size_t i;
    unsigned bytes_seen, buffer_seen, curr_buf, null_length;
    unsigned j, k;
    unsigned start_index, end_index, char_len;
    float real_obj;
    
    // get record metadata from slot directory
    seek_offset = 4096 * rid.pageNum - (8 + (8 * rid.slotNum));
    retval = fseek(fileHandle.handled_file, seek_offset, SEEK_SET);
    if(retval != 0){
        return -1;
    }

    slot_directory = malloc(8);
    retval = fread(slot_directory, 8, 1, fileHandle.handled_file);
        
    slot_pointer = bytes_to_int(slot_directory, 0);
    record_length = bytes_to_int(slot_directory, 4);
    free(slot_directory);

    // get record from file
    seek_offset = 4096 * (rid.pageNum - 1) + slot_pointer;
    retval = fseek(fileHandle.handled_file, seek_offset, SEEK_SET);
    if(retval != 0){
        return -1;
    }
    
    // convert record representation to printable version
    buffer = malloc(record_length);
    bytes_seen = 0;
    buffer_seen = 4;
    null_length = ceil(recordDescriptor.size() / 8.0);
    
    retval = fread(buffer, record_length, 1, fileHandle.handled_file);
    
    // add null indicator to data
    memcpy((char *)data + bytes_seen, &((char *)buffer)[buffer_seen], null_length);
    bytes_seen += null_length;
    buffer_seen += null_length;
    
    j = 4;
    k = 0;
    for(i = 0; i < recordDescriptor.size(); ++i){
        if(k > 7){
            j++;
            k = 0;
        }
        if((((char*)buffer)[j] & (1 << (7 - k))) == 1){ // null field
            continue;
        }
        k++;
        
        curr_buf = buffer_seen;
        
        if(recordDescriptor[i].type == 2){ // char
            start_index = bytes_to_int(buffer, curr_buf);
            if(i != recordDescriptor.size() - 1){
                end_index = bytes_to_int(buffer, curr_buf + 4);
            }else{
                end_index = record_length;
            }
            char_len = end_index - start_index;
            memcpy((char *)data + bytes_seen, &char_len, sizeof(int));
            bytes_seen += 4;
            curr_buf = start_index;
            memcpy((char *)data + bytes_seen, &((char *)buffer)[start_index], char_len);
            bytes_seen += char_len;
            buffer_seen += 4;
        }
        
        if(recordDescriptor[i].type == 1){ // real
            start_index = bytes_to_int(buffer, curr_buf);
            curr_buf = start_index;
            real_obj = bytes_to_float(buffer, curr_buf);
            memcpy((char *)data + bytes_seen, &real_obj, sizeof(float));
            bytes_seen += 4;
            buffer_seen += 4;
        }
        
        if(recordDescriptor[i].type == 0){
            start_index = bytes_to_int(buffer, curr_buf);
            curr_buf = start_index;
            end_index = bytes_to_int(buffer, curr_buf);
            memcpy((char *)data + bytes_seen, &end_index, sizeof(int));
            bytes_seen += 4;
            buffer_seen += 4;
        }
    }
    free(buffer);
    return 0;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    size_t i;
    unsigned bytes_seen, int_obj, l;
    unsigned j, k;
    float real_obj;
    
    bytes_seen = ceil(recordDescriptor.size() / 8.0);
    
    j = 0;
    k = 0;
    for(i = 0; i < recordDescriptor.size(); ++i){ // iterate through fields
        if(k > 7){
            j++;
            k = 0;
        }
        if((((char*)data)[j] & (1 << (7 - k))) == 1){ // null field
            continue;
        }
        k++;
        
        if(recordDescriptor[i].type == 2){ // char
            int_obj = bytes_to_int(data, bytes_seen);
            bytes_seen += 4;
            
            for(l = 0; l < int_obj; ++l){
                cout << ((char*)data)[bytes_seen + l];
            }
            bytes_seen += int_obj;
        }
        
        if(recordDescriptor[i].type == 1){ // real
            real_obj = bytes_to_float(data, bytes_seen);
            cout << fixed;
            cout << setprecision(3) << real_obj;
            bytes_seen += 4;
        }
        
        if(recordDescriptor[i].type == 0){
            int_obj = bytes_to_int(data, bytes_seen);
            cout << int_obj;
            bytes_seen += 4;
        }
        cout << " ";
    }
    cout << endl;
    
    return 0;
}