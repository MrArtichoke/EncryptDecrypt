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
float bytes_to_float(const void *data, const int index){
    float real_obj;
    char real_bytes[] = {((char*)data)[index], 
                                 ((char*)data)[index + 1], 
                                 ((char*)data)[index + 2], 
                                 ((char*)data)[index + 3],};
    memcpy(&real_obj, &real_bytes, sizeof(real_obj));
    return real_obj;
}

// code for byte[] -> int found here:
// http://www.cplusplus.com/forum/beginner/3076/
int bytes_to_int(const void *data, const int index){
    int int_obj;
    
    int_obj = 0;
    int_obj = (int_obj << 8) + ((char*)data)[index + 3];
    int_obj = (int_obj << 8) + ((char*)data)[index + 2];
    int_obj = (int_obj << 8) + ((char*)data)[index + 1];
    int_obj = (int_obj << 8) + ((char*)data)[index];
    
    return int_obj;
}

RC insertrecord_append_page(FileHandle &fileHandle, RID &rid, const void *vrecord, const unsigned record_length){
    void *slot_directory;
    int slot_pointer, offset, retval;
        
    fileHandle.appendPage(vrecord);
        
    // set up slot directory
    slot_directory = malloc(16);
    offset = 0;
        
    // add record pointer to directory
    slot_pointer = 0;
    memcpy((char *)slot_directory + offset, &slot_pointer, 4);
    offset += 4;
        
    // add record length to directory
    slot_pointer = record_length;
    memcpy((char *)slot_directory + offset, &slot_pointer, 4);
    offset +=4;
        
    // add number of slots to directory
    slot_pointer = 1;
    memcpy((char *)slot_directory + offset, &slot_pointer, 4);
    offset +=4;
        
    // add pointer to start of free space to directory
    slot_pointer = record_length + 1;
    memcpy((char *)slot_directory + offset, &slot_pointer, 4);
    offset +=4;
        
    // write slot directory to file
    retval = fseek(fileHandle.handled_file, -16, SEEK_END);
    if(retval != 0){
        //perror("insertrecord: fseek");
        return -1;
    }
        
    retval = fwrite(slot_directory, 16, 1, fileHandle.handled_file);
    if(retval != 1){
        //perror("insertrecord: fwrite");
        return -1;
    }
    
    // add rid
    rid.pageNum = fileHandle.getNumberOfPages();
    rid.slotNum = 1;
    
    return 0;
}

RC insertrecord_last_page(FileHandle &fileHandle, RID &rid, const void *vrecord, const unsigned record_length, const int num_records, const int free_space_pointer){
    int retval, new_free_space_pointer, new_num_records, new_slot;
    void *new_slot_directory, *record_metadata;
    
    // write record to page
    retval = fseek(fileHandle.handled_file, ((4096-free_space_pointer) * -1), SEEK_END);
    if(retval != 0){
        //perror("insertrecord: fseek");
        return -1;
    }
    
    retval = fwrite(vrecord, record_length, 1, fileHandle.handled_file);
    if(retval != 1){
        //perror("insertrecord: fwrite");
        return -1;
    }
    
    // update slot directory
    
    // set up new slot directory
    new_slot_directory = malloc(8);
    new_num_records = num_records + 1;
    new_free_space_pointer = free_space_pointer + record_length;
    memcpy((char *)new_slot_directory, &new_num_records, sizeof(int));
    memcpy((char *)new_slot_directory + 4, &new_free_space_pointer, sizeof(int));

    // write number of slots and free space pointer
    retval = fseek(fileHandle.handled_file, -8, SEEK_END);
    if(retval != 0){
        //perror("insertrecord: fseek");
        return -1;
    }
        
    retval = fwrite(new_slot_directory, 8, 1, fileHandle.handled_file);
    if(retval != 1){
        //perror("insertrecord: fwrite");
        return -1;
    }
    
    // add new record metadata to slot directory
    record_metadata = malloc(8);
    memcpy((char *)record_metadata, &free_space_pointer, sizeof(int));
    memcpy((char *)record_metadata + 4, &record_length, sizeof(int));
    
    new_slot = 8 + (8 * new_num_records);
    retval = fseek(fileHandle.handled_file, -new_slot, SEEK_END);
    if(retval != 0){
        //perror("insertrecord: fseek");
        return -1;
    }
    
    retval = fwrite(record_metadata, 8, 1, fileHandle.handled_file);
    if(retval != 1){
        //perror("insertrecord: fwrite");
        return -1;
    }
    
    // update rid
    rid.pageNum = fileHandle.getNumberOfPages();
    rid.slotNum = new_num_records;
    
    return 0;
}

RC insertrecord_choose_page(FileHandle &fileHandle, RID &rid, const void *vrecord, const unsigned record_length){
    int num_records, free_space_pointer;
    unsigned retval, free_space;
    void *slot_directory;
    RC rc;
    
    retval = fileHandle.getNumberOfPages();
    
    if(retval != 0){ // attempt to insert record into last page
        // check if there is space in last page
        // slot directory size = 8 + (8 * #records)
        
        // get number of slots and free space pointer
        retval = fseek(fileHandle.handled_file, -8, SEEK_END);
        if(retval != 0){
           //perror("insertrecord: fseek");
           return -1;
        }
        
        slot_directory = malloc(8);
        retval = fread(slot_directory, 8, 1, fileHandle.handled_file);
        if(retval != 1 && feof(fileHandle.handled_file) == 0){
            //perror("readpage: fread");
            return -1;
        }
        
        num_records = bytes_to_int(slot_directory, 0);
        free_space_pointer = bytes_to_int(slot_directory, 4);
        
        free_space = (4096 - (8 + 8 * (num_records + 1))) - free_space_pointer;
        if(free_space >= record_length){ // last page has enough space for record
            rc = insertrecord_last_page(fileHandle, rid, vrecord, record_length, num_records, free_space_pointer);
        }else{ // last page has no space for record
            
        }
 
    }else{ // append a page
        rc = insertrecord_append_page(fileHandle, rid, vrecord, record_length);
    }
    return rc;
}

unsigned insertrecord_get_record_length(const vector<Attribute> &recordDescriptor, const void *data, int null_length){
    int j, k, int_obj;
    size_t i;
    unsigned record_length, bytes_seen;
    
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
    unsigned bytes_seen, record_length;
    int num_fields, int_obj, offset, null_length;
    int  j, k, l, pointer_start;
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
    memcpy((char *)vrecord + offset, &data, null_length);
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
            bytes_seen += 4;
            memcpy((char *)vrecord + offset, &((char*)data)[bytes_seen], 4);
            offset += 4;
        }
    }
    
    // choose page to place record in
    rc = insertrecord_choose_page(fileHandle, rid, vrecord, record_length);
    
    return rc;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    size_t i;
    unsigned bytes_seen;
    int int_obj, j, k, l;
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