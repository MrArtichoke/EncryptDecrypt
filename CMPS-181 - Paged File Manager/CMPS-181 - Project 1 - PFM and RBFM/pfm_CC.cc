#include <iostream>
#include "pfm.h"

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}


RC PagedFileManager::createFile(const string &fileName)
{
    FILE* file;
    const char *c_fileName;
    
    c_fileName = fileName.c_str();
    
    // make sure fileName is not already present
    file = fopen(c_fileName, "rb"); 
    if(file != NULL){
        fclose(file);
        return -1;
    }
    
    file = fopen(c_fileName, "wb");
    if(file != NULL){
        fclose(file);
    }else{
        return -1;
    }
    return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
    int rem_ret;
    const char *c_fileName; 
    
    c_fileName = fileName.c_str();
    
    rem_ret = remove(c_fileName);
    if(rem_ret != 0){ 
        return -1;
    }
    return 0;
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    FILE* file;
    const char *c_fileName;
    
    c_fileName = fileName.c_str();
    
    file = fopen(c_fileName, "rb+");
    if(file == NULL){
        return -1;
    }
    
    if(fileHandle.handled_file == NULL){
        fileHandle.handled_file = file;
    }else{
        return -1;
    }
    return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    int fflush_ret;
    
    if(fileHandle.handled_file == NULL){
        return -1;
    }
    
    fflush_ret = fflush(fileHandle.handled_file);
    if(fflush_ret != 0){
        return -1;
    }
    
    fclose(fileHandle.handled_file);
    fileHandle.handled_file = NULL;
    return 0;
}


FileHandle::FileHandle()
{
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    handled_file = NULL;
}


FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
    int fseek_ret;
    size_t fread_ret;
    
    fseek_ret = fseek(handled_file, PAGE_SIZE * pageNum, SEEK_SET);
    if(fseek_ret != 0){
       return -1;
    }
    
    fread_ret = fread(data, PAGE_SIZE, 1, handled_file);
    if(fread_ret != 1 && feof(handled_file) == 0){
        return -1;
    }
    
    readPageCounter += 1;
    return 0;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    int fseek_ret;
    size_t fwrite_ret;
    
    fseek_ret = fseek(handled_file, PAGE_SIZE * pageNum, SEEK_SET);
    if(fseek_ret != 0){
       return -1;
    }
    
    fwrite_ret = fwrite(data, PAGE_SIZE, 1, handled_file);
    if(fwrite_ret != 1){
        return -1;
    }
    
    writePageCounter += 1;
    return 0;
}


RC FileHandle::appendPage(const void *data)
{
    int fseek_ret;
    size_t fwrite_ret;
    
    fseek_ret = fseek(handled_file, PAGE_SIZE * appendPageCounter, SEEK_SET);
    if(fseek_ret != 0){
       return -1;
    }
    
    fwrite_ret = fwrite(data, PAGE_SIZE, 1, handled_file);
    if(fwrite_ret != 1){
        return -1;
    }
    
    appendPageCounter += 1;
    return 0;
}


unsigned FileHandle::getNumberOfPages()
{    
    unsigned num_pages;
    
    num_pages = appendPageCounter;
    
    return num_pages;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
    
    return 0;
}
