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

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    size_t i;
    unsigned bytes_seen;
    unsigned record_length;
    int num_fields, int_obj, offset, null_length;
    int  j, k, l, pointer_start;
    void *vrecord;
    
    record_length = 4; // first four bytes are number of fields
    
    null_length = ceil(recordDescriptor.size() / 8.0);
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
            // code for byte[] -> int found here:
            // http://www.cplusplus.com/forum/beginner/3076/
            int_obj = 0;
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 3];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 2];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 1];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen];
            bytes_seen += 4;
            record_length += int_obj; // bytes for char field values
            bytes_seen += int_obj;
        }else{
            bytes_seen += 4;
            record_length += recordDescriptor[i].length; // bytes for int or real field values
        }
    }
    
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
            // code for byte[] -> int found here:
            // http://www.cplusplus.com/forum/beginner/3076/
            int_obj = 0;
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 3];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 2];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 1];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen];
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
    
    return -1;
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
            // code for byte[] -> int found here:
            // http://www.cplusplus.com/forum/beginner/3076/
            int_obj = 0;
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 3];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 2];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 1];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen];
            bytes_seen += 4;
            
            for(l = 0; l < int_obj; ++l){
                cout << ((char*)data)[bytes_seen + l];
            }
            bytes_seen += int_obj;
        }
        
        if(recordDescriptor[i].type == 1){ // real
            // code for byte[] -> float found here:
            // stackoverflow.com/questions/3991478/building-a-32bit-float-out-of-its-4-composite-bytes-c
            char real_bytes[] = {((char*)data)[bytes_seen], 
                                 ((char*)data)[bytes_seen + 1], 
                                 ((char*)data)[bytes_seen + 2], 
                                 ((char*)data)[bytes_seen + 3],};
            memcpy(&real_obj, &real_bytes, sizeof(real_obj));
            cout << fixed;
            cout << setprecision(3) << real_obj;
            bytes_seen += 4;
        }
        
        if(recordDescriptor[i].type == 0){
            // code for byte[] -> int found here:
            // http://www.cplusplus.com/forum/beginner/3076/
            int_obj = 0;
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 3];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 2];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen + 1];
            int_obj = (int_obj << 8) + ((char*)data)[bytes_seen];
            cout << int_obj;
            bytes_seen += 4;
        }
        cout << " ";
    }
    cout << endl;
    
    return 0;
}