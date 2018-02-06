// continuing off Chris's version

#include <iostream>
#include <math.h>
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
    
    return -1;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    
    return 0;

}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    size_t i, j;
    unsigned null_length;
    void *obj;
    int int_obj;
    float real_obj;
    unsigned string_obj;
    char *make_string;
    
    null_length = ceil(recordDescriptor.size() / 8.0);
    
    for(i = 0; i < recordDescriptor.size(); ++i){ // iterate through fields
        if(((char*)data)[i] == 1){ // null field
            continue;
        }
        
        obj = malloc(recordDescriptor[i].length);
        
        for(j = 0; j < recordDescriptor[i].length; ++j){ // iterate through data
            ((char*)obj)[j] = ((char*)data)[null_length + j];
            /*if(recordDescriptor[i].type == 2){
             cout << ((char*)data)[null_length + j];
             if(((char*)data)[null_length + j] > 126 || ((char*)data)[null_length + j] < 0){
             break;
             }
             }*/
        }
        if(recordDescriptor[i].type == 0){ // int
            int_obj = int((unsigned char)(((char*)obj)[0]) << 24 |
                          (unsigned char)(((char*)obj)[1]) << 16 |
                          (unsigned char)(((char*)obj)[2]) << 8 |
                          (unsigned char)(((char*)obj)[3]));
            cout << int_obj;
        }else if(recordDescriptor[i].type == 1){ // real
            // convert obj to object of type real
            real_obj = float((unsigned char)(((char*)obj)[0]) << 24 |
                           (unsigned char)(((char*)obj)[1]) << 16 |
                           (unsigned char)(((char*)obj)[2]) << 8 |
                           (unsigned char)(((char*)obj)[3]));
            cout << real_obj;
        }else{ // char
            // convert obj to object of type string or char*
            string_obj = char((unsigned char)(((char*)obj)[0]) << 24 |
                               (unsigned char)(((char*)obj)[1]) << 16 |
                               (unsigned char)(((char*)obj)[2]) << 8 |
                               (unsigned char)(((char*)obj)[3]));
            make_string = (char*) malloc(string_obj++);
            cout << make_string;
            free(&string_obj);
            
        }
        null_length += j;
        cout << " ";
        
        free(obj);
    }
    cout << endl;
    
    
    return 0;
}