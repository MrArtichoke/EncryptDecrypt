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
    // constructor
    // creates an instance of a PageFileManager class object
}


PagedFileManager::~PagedFileManager()
{
    // destructor
    // deletes an instance of a PagedFileManager class object
}


RC PagedFileManager::createFile(const string &fileName)
{
    // check if the file exists first; if not, then create the file
    FILE* newFile;
    const char *c_fileName = fileName.c_str();
    
    newFile = fopen(c_fileName, "w");
    
    if (c_fileName != NULL)
        return 0;
    
    return -1;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
    FILE* newFile;
    const char *c_fileName = fileName.c_str();
    
    newFile = fopen(c_fileName, "r");
    
    if (!newFile)
        return -1;
    
    else
    {
        remove(c_fileName);
        return 0;
    }
}


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    FILE* newFile;
    const char *c_fileName = fileName.c_str();
    
    newFile = fopen(c_fileName, "w");
    
    if (!newFile)
        return -1;
    
    else
    {
        fileHandle.track_file = newFile;
        return 0;
    }
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    if (fileHandle.track_file == NULL)
        return -1;
    
    else
    {
        fclose(fileHandle.track_file);
        return 0;
    }
}


FileHandle::FileHandle()
{
    // constructor
    // initalize the counters to 0
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
}


FileHandle::~FileHandle()
{
    // destructor
    // deletes an instance of a FileHandle class object
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
    fseek(track_file, (pageNum * PAGE_SIZE), SEEK_SET);          // ??
    
    if((fread(data, 1, PAGE_SIZE, track_file)))                  // ??  1 might not be correct...
    {
        readPageCounter++;
        return 0;
    }
    
    else
        return -1;
}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    int fseek_ret = fseek(track_file, PAGE_SIZE * pageNum, SEEK_SET);
    int fwrite_ret = fwrite(data, PAGE_SIZE, 1, track_file);
    
    if (fseek_ret != 0 || fwrite_ret != 1)
        return -1;
    
    writePageCounter++;
    
    return 0;
}


RC FileHandle::appendPage(const void *data)
{
    int fwrite_ret = fwrite(data, PAGE_SIZE, 1, track_file);
    
    if (fwrite_ret != 1)
        return -1;
    
    appendPageCounter++;
    return -1;
}


unsigned FileHandle::getNumberOfPages()
{
    int fseek_ret;
    unsigned num_pages;
    long int f_size;
    fseek_ret = fseek(track_file, BEGIN, SEEK_END);
    
    if (fseek_ret != 0)
        return -1;
    
    f_size = ftell(track_file);
    num_pages = f_size/PAGE_SIZE;
    
    return num_pages;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
    
    return 0;
}
