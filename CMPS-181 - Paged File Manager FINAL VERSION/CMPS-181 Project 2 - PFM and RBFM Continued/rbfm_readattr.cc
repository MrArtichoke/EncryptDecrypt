#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include "rbfm.h"

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = NULL;
PagedFileManager *RecordBasedFileManager::_pf_manager = NULL;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();

    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
    // Initialize the internal PagedFileManager instance
    _pf_manager = PagedFileManager::instance();
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) 
{
    // Creating a new paged file.
    if (_pf_manager->createFile(fileName))
        return RBFM_CREATE_FAILED;

    // Setting up the first page.
    void * firstPageData = calloc(PAGE_SIZE, 1);
    if (firstPageData == NULL)
        return RBFM_MALLOC_FAILED;
    newRecordBasedPage(firstPageData);

    // Adds the first record based page.
    FileHandle handle;
    if (_pf_manager->openFile(fileName.c_str(), handle))
        return RBFM_OPEN_FAILED;
    if (handle.appendPage(firstPageData))
        return RBFM_APPEND_FAILED;
    _pf_manager->closeFile(handle);

    free(firstPageData);

    return SUCCESS;
}

RC RecordBasedFileManager::destroyFile(const string &fileName) 
{
    return _pf_manager->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) 
{
    return _pf_manager->openFile(fileName.c_str(), fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) 
{
    return _pf_manager->closeFile(fileHandle);
}

bool fieldIsNull(char *, int);
SlotDirectoryRecordEntry getSlotDirectoryRecordEntry(void *, unsigned);
SlotDirectoryHeader getSlotDirectoryHeader(void *);

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) 
{
    // Gets the size of the record.
    unsigned recordSize = getRecordSize(recordDescriptor, data);

    // Cycles through pages looking for enough free space for the new entry.
    void *pageData = malloc(PAGE_SIZE);
    if (pageData == NULL)
        return RBFM_MALLOC_FAILED;
    bool pageFound = false;
    unsigned i;
    unsigned numPages = fileHandle.getNumberOfPages();
    for (i = 0; i < numPages; i++)
    {
        if (fileHandle.readPage(i, pageData))
            return RBFM_READ_FAILED;

        // When we find a page with enough space (accounting also for the size that will be added to the slot directory), we stop the loop.
        if (getPageFreeSpaceSize(pageData) >= sizeof(SlotDirectoryRecordEntry) + recordSize)
        {
            pageFound = true;
            break;
        }
    }

    // If we can't find a page with enough space, we create a new one
    if(!pageFound)
    {
        newRecordBasedPage(pageData);
    }

    SlotDirectoryHeader slotHeader = getSlotDirectoryHeader(pageData);

    // Setting the return RID.
    rid.pageNum = i;
    rid.slotNum = slotHeader.recordEntriesNumber;

    // Adding the new record reference in the slot directory.
    SlotDirectoryRecordEntry newRecordEntry;
    newRecordEntry.length = recordSize;
    newRecordEntry.offset = slotHeader.freeSpaceOffset - recordSize;
    setSlotDirectoryRecordEntry(pageData, rid.slotNum, newRecordEntry);

    // Updating the slot directory header.
    slotHeader.freeSpaceOffset = newRecordEntry.offset;
    slotHeader.recordEntriesNumber += 1;
    setSlotDirectoryHeader(pageData, slotHeader);

    // Adding the record data.
    setRecordAtOffset (pageData, newRecordEntry.offset, recordDescriptor, data);

    // Writing the page to disk.
    if (pageFound)
    {
        if (fileHandle.writePage(i, pageData))
            return RBFM_WRITE_FAILED;
    }
    else
    {
        if (fileHandle.appendPage(pageData))
            return RBFM_APPEND_FAILED;
    }

    free(pageData);
    return SUCCESS;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) 
{
    // Retrieve the specific page
    void * pageData = malloc(PAGE_SIZE);
    if (fileHandle.readPage(rid.pageNum, pageData))
        return RBFM_READ_FAILED;

    // Checks if the specific slot id exists in the page
    SlotDirectoryHeader slotHeader = getSlotDirectoryHeader(pageData);
    if(slotHeader.recordEntriesNumber < rid.slotNum)
        return RBFM_SLOT_DN_EXIST;

    // Gets the slot directory record entry data
    SlotDirectoryRecordEntry recordEntry = getSlotDirectoryRecordEntry(pageData, rid.slotNum);
    
    if(recordEntry.length < 0){
        return RBFM_SLOT_DN_EXIST;
    }
    
    if(recordEntry.offset < 0){
        RID new_rid;
        new_rid.pageNum = recordEntry.length;
        if(recordEntry.offset != -413){
            new_rid.slotNum = recordEntry.offset * -1;
        }else{
            new_rid.slotNum = 0;
        }
        readRecord(fileHandle, recordDescriptor, new_rid, data);
        free(pageData);
        return SUCCESS;
    }

    // Retrieve the actual entry data
    getRecordAtOffset(pageData, recordEntry.offset, recordDescriptor, data);

    free(pageData);
    return SUCCESS;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) 
{
    // Parse the null indicator into an array
    int nullIndicatorSize = getNullIndicatorSize(recordDescriptor.size());
    char nullIndicator[nullIndicatorSize];
    memset(nullIndicator, 0, nullIndicatorSize);
    memcpy(nullIndicator, data, nullIndicatorSize);
    
    // We've read in the null indicator, so we can skip past it now
    unsigned offset = nullIndicatorSize;

    cout << "----" << endl;
    for (unsigned i = 0; i < (unsigned) recordDescriptor.size(); i++)
    {
        cout << setw(10) << left << recordDescriptor[i].name << ": ";
        // If the field is null, don't print it
        bool isNull = fieldIsNull(nullIndicator, i);
        if (isNull)
        {
            cout << "NULL" << endl;
            continue;
        }
        switch (recordDescriptor[i].type)
        {
            case TypeInt:
                uint32_t data_integer;
                memcpy(&data_integer, ((char*) data + offset), INT_SIZE);
                offset += INT_SIZE;

                cout << "" << data_integer << endl;
            break;
            case TypeReal:
                float data_real;
                memcpy(&data_real, ((char*) data + offset), REAL_SIZE);
                offset += REAL_SIZE;

                cout << "" << data_real << endl;
            break;
            case TypeVarChar:
                // First VARCHAR_LENGTH_SIZE bytes describe the varchar length
                uint32_t varcharSize;
                memcpy(&varcharSize, ((char*) data + offset), VARCHAR_LENGTH_SIZE);
                offset += VARCHAR_LENGTH_SIZE;

                // Gets the actual string.
                char *data_string = (char*) malloc(varcharSize + 1);
                if (data_string == NULL)
                    return RBFM_MALLOC_FAILED;
                memcpy(data_string, ((char*) data + offset), varcharSize);

                // Adds the string terminator.
                data_string[varcharSize] = '\0';
                offset += varcharSize;

                cout << data_string << endl;
                free(data_string);
            break;
        }
    }
    cout << "----" << endl;

    return SUCCESS;
}

// Private helper methods

// Configures a new record based page, and puts it in "page".
void RecordBasedFileManager::newRecordBasedPage(void * page)
{
    memset(page, 0, PAGE_SIZE);
    // Writes the slot directory header.
    SlotDirectoryHeader slotHeader;
    slotHeader.freeSpaceOffset = PAGE_SIZE;
    slotHeader.recordEntriesNumber = 0;
    setSlotDirectoryHeader(page, slotHeader);
}

SlotDirectoryHeader getSlotDirectoryHeader(void * page)
{
    // Getting the slot directory header.
    SlotDirectoryHeader slotHeader;
    memcpy (&slotHeader, page, sizeof(SlotDirectoryHeader));
    return slotHeader;
}

void RecordBasedFileManager::setSlotDirectoryHeader(void * page, SlotDirectoryHeader slotHeader)
{
    // Setting the slot directory header.
    memcpy (page, &slotHeader, sizeof(SlotDirectoryHeader));
}

SlotDirectoryRecordEntry getSlotDirectoryRecordEntry(void * page, unsigned recordEntryNumber)
{
    // Getting the slot directory entry data.
    SlotDirectoryRecordEntry recordEntry;
    memcpy  (
            &recordEntry,
            ((char*) page + sizeof(SlotDirectoryHeader) + recordEntryNumber * sizeof(SlotDirectoryRecordEntry)),
            sizeof(SlotDirectoryRecordEntry)
            );

    return recordEntry;
}

void RecordBasedFileManager::setSlotDirectoryRecordEntry(void * page, unsigned recordEntryNumber, SlotDirectoryRecordEntry recordEntry)
{
    // Setting the slot directory entry data.
    memcpy  (
            ((char*) page + sizeof(SlotDirectoryHeader) + recordEntryNumber * sizeof(SlotDirectoryRecordEntry)),
            &recordEntry,
            sizeof(SlotDirectoryRecordEntry)
            );
}

// Computes the free space of a page (function of the free space pointer and the slot directory size).
unsigned RecordBasedFileManager::getPageFreeSpaceSize(void * page) 
{
    SlotDirectoryHeader slotHeader = getSlotDirectoryHeader(page);
    return slotHeader.freeSpaceOffset - slotHeader.recordEntriesNumber * sizeof(SlotDirectoryRecordEntry) - sizeof(SlotDirectoryHeader);
}

unsigned RecordBasedFileManager::getRecordSize(const vector<Attribute> &recordDescriptor, const void *data) 
{
    // Read in the null indicator
    int nullIndicatorSize = getNullIndicatorSize(recordDescriptor.size());
    char nullIndicator[nullIndicatorSize];
    memset(nullIndicator, 0, nullIndicatorSize);
    memcpy(nullIndicator, (char*) data, nullIndicatorSize);

    // Offset into *data. Start just after null indicator
    unsigned offset = nullIndicatorSize;
    // Running count of size. Initialize to size of header
    unsigned size = sizeof (RecordLength) + (recordDescriptor.size()) * sizeof(ColumnOffset) + nullIndicatorSize;

    for (unsigned i = 0; i < (unsigned) recordDescriptor.size(); i++)
    {
        // Skip null fields
        if (fieldIsNull(nullIndicator, i))
            continue;
        switch (recordDescriptor[i].type)
        {
            case TypeInt:
                size += INT_SIZE;
                offset += INT_SIZE;
            break;
            case TypeReal:
                size += REAL_SIZE;
                offset += REAL_SIZE;
            break;
            case TypeVarChar:
                uint32_t varcharSize;
                // We have to get the size of the VarChar field by reading the integer that precedes the string value itself
                memcpy(&varcharSize, (char*) data + offset, VARCHAR_LENGTH_SIZE);
                size += varcharSize;
                offset += varcharSize + VARCHAR_LENGTH_SIZE;
            break;
        }
    }

    return size;
}

// Calculate actual bytes for nulls-indicator for the given field counts
int RecordBasedFileManager::getNullIndicatorSize(int fieldCount) 
{
    return int(ceil((double) fieldCount / CHAR_BIT));
}

bool fieldIsNull(char *nullIndicator, int i)
{
    int indicatorIndex = i / CHAR_BIT;
    int indicatorMask  = 1 << (CHAR_BIT - 1 - (i % CHAR_BIT));
    return (nullIndicator[indicatorIndex] & indicatorMask) != 0;
}

void RecordBasedFileManager::setRecordAtOffset(void *page, unsigned offset, const vector<Attribute> &recordDescriptor, const void *data)
{
    // Read in the null indicator
    int nullIndicatorSize = getNullIndicatorSize(recordDescriptor.size());
    char nullIndicator[nullIndicatorSize];
    memset (nullIndicator, 0, nullIndicatorSize);
    memcpy(nullIndicator, (char*) data, nullIndicatorSize);

    // Points to start of record
    char *start = (char*) page + offset;

    // Offset into *data
    unsigned data_offset = nullIndicatorSize;
    // Offset into page header
    unsigned header_offset = 0;

    RecordLength len = recordDescriptor.size();
    memcpy(start + header_offset, &len, sizeof(len));
    header_offset += sizeof(len);

    memcpy(start + header_offset, nullIndicator, nullIndicatorSize);
    header_offset += nullIndicatorSize;

    // Keeps track of the offset of each record
    // Offset is relative to the start of the record and points to the END of a field
    ColumnOffset rec_offset = header_offset + (recordDescriptor.size()) * sizeof(ColumnOffset);

    unsigned i = 0;
    for (i = 0; i < recordDescriptor.size(); i++)
    {
        if (!fieldIsNull(nullIndicator, i))
        {
            // Points to current position in *data
            char *data_start = (char*) data + data_offset;

            // Read in the data for the next column, point rec_offset to end of newly inserted data
            switch (recordDescriptor[i].type)
            {
                case TypeInt:
                    memcpy (start + rec_offset, data_start, INT_SIZE);
                    rec_offset += INT_SIZE;
                    data_offset += INT_SIZE;
                break;
                case TypeReal:
                    memcpy (start + rec_offset, data_start, REAL_SIZE);
                    rec_offset += REAL_SIZE;
                    data_offset += REAL_SIZE;
                break;
                case TypeVarChar:
                    unsigned varcharSize;
                    // We have to get the size of the VarChar field by reading the integer that precedes the string value itself
                    memcpy(&varcharSize, data_start, VARCHAR_LENGTH_SIZE);
                    memcpy(start + rec_offset, data_start + VARCHAR_LENGTH_SIZE, varcharSize);
                    // We also have to account for the overhead given by that integer.
                    rec_offset += varcharSize;
                    data_offset += VARCHAR_LENGTH_SIZE + varcharSize;
                break;
            }
        }
        // Copy offset into record header
        // Offset is relative to the start of the record and points to END of field
        memcpy(start + header_offset, &rec_offset, sizeof(ColumnOffset));
        header_offset += sizeof(ColumnOffset);
    }
}

// Support header size and null indicator. If size is less than recordDescriptor size, then trailing records are null
// Memset null indicator as 1?
void RecordBasedFileManager::getRecordAtOffset(void *page, unsigned offset, const vector<Attribute> &recordDescriptor, void *data)
{
    // Pointer to start of record
    char *start = (char*) page + offset;

    // Allocate space for null indicator. The returned null indicator may be larger than
    // the null indicator in the table has had fields added to it
    int nullIndicatorSize = getNullIndicatorSize(recordDescriptor.size());
    char nullIndicator[nullIndicatorSize];
    memset(nullIndicator, 0, nullIndicatorSize);

    // Get number of columns and size of the null indicator for this record
    RecordLength len = 0;
    memcpy (&len, start, sizeof(RecordLength));
    int recordNullIndicatorSize = getNullIndicatorSize(len);

    // Read in the existing null indicator
    memcpy (nullIndicator, start + sizeof(RecordLength), recordNullIndicatorSize);

    // If this new recordDescriptor has had fields added to it, we set all of the new fields to null
    for (unsigned i = len; i < recordDescriptor.size(); i++)
    {
        int indicatorIndex = (i+1) / CHAR_BIT;
        int indicatorMask  = 1 << (CHAR_BIT - 1 - (i % CHAR_BIT));
        nullIndicator[indicatorIndex] |= indicatorMask;
    }
    // Write out null indicator
    memcpy(data, nullIndicator, nullIndicatorSize);

    // Initialize some offsets
    // rec_offset: points to data in the record. We move this forward as we read data from our record
    unsigned rec_offset = sizeof(RecordLength) + recordNullIndicatorSize + len * sizeof(ColumnOffset);
    // data_offset: points to our current place in the output data. We move this forward as we write data to data.
    unsigned data_offset = nullIndicatorSize;
    // directory_base: points to the start of our directory of indices
    char *directory_base = start + sizeof(RecordLength) + recordNullIndicatorSize;
    
    for (unsigned i = 0; i < recordDescriptor.size(); i++)
    {
        if (fieldIsNull(nullIndicator, i))
            continue;
        
        // Grab pointer to end of this column
        ColumnOffset endPointer;
        memcpy(&endPointer, directory_base + i * sizeof(ColumnOffset), sizeof(ColumnOffset));

        // rec_offset keeps track of start of column, so end-start = total size
        uint32_t fieldSize = endPointer - rec_offset;

        // Special case for varchar, we must give data the size of varchar first
        if (recordDescriptor[i].type == TypeVarChar)
        {
            memcpy((char*) data + data_offset, &fieldSize, VARCHAR_LENGTH_SIZE);
            data_offset += VARCHAR_LENGTH_SIZE;
        }
        // Next we copy bytes equal to the size of the field and increase our offsets
        memcpy((char*) data + data_offset, start + rec_offset, fieldSize);
        rec_offset += fieldSize;
        data_offset += fieldSize;
    }
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, const string &attributeName, void *data)
{
    // get size of record
    unsigned attr_index = -1;
    int nullIndicatorSize = getNullIndicatorSize(recordDescriptor.size());
    unsigned size = nullIndicatorSize;
    unsigned offset = size;
    bool is_varchar = false;
    unsigned varchar_len;
    for(unsigned i = 0; i < recordDescriptor.size(); ++i){
        size += recordDescriptor[i].length;
        if(recordDescriptor[i].type == TypeVarChar){
            size += 4;
        }
        
        // get the index of the attribute we want
        if(recordDescriptor[i].name == attributeName){
            attr_index = i;
            if(recordDescriptor[i].type == TypeVarChar){
                is_varchar = true;
            }
        }
    }
    void *record = malloc(size);
   
    // get record
    readRecord(fileHandle, recordDescriptor, rid, record);
    
    // get null indicator
    char nullIndicator[nullIndicatorSize];
    memset(nullIndicator, 0, nullIndicatorSize);
    memcpy(nullIndicator, (char*) record, nullIndicatorSize);
    bool isNull = fieldIsNull(nullIndicator, attr_index);
    char new_null_ind[1];
    memset(new_null_ind, 0, 1);
    
    if (isNull){
        new_null_ind[0] ^= 128;
        memcpy((char *)data, &new_null_ind, 1);
        return SUCCESS;
    }else{
        memcpy((char *)data, &new_null_ind, 1);
    }
    
    // get attribute and put into data
    size = nullIndicatorSize;
    for(unsigned i = 0; i < recordDescriptor.size(); ++i){
        if (fieldIsNull(nullIndicator, i))
            continue;
        
        if(recordDescriptor[i].type == TypeVarChar){
            memcpy(&varchar_len, (char *)record + size, sizeof(int));
            size += 4;
            size += varchar_len;
        }else{
            size += 4;
        }
        
        // get offset of attribute we want
        if(i == attr_index){
            if(is_varchar){
                offset = size - varchar_len;
            }else{
                offset = size - sizeof(int);
            }
        }
    }
    
    if(is_varchar){
        memcpy((char *)data + 1, (char *)record + (offset - 4), 4);
        memcpy((char *)data + 5, (char *)record + offset, varchar_len);
    }else{
         memcpy((char *)data + 1, (char *)record + offset, sizeof(int));
    }

    free(record);
    return SUCCESS;
}

int compare_attr(const void *attr, const CompOp compOp, const void *value, const int cmp_t, const unsigned size){
    uint32_t int_attr, int_value;
    float real_attr, real_value;
    int memcmp_val;
    if(cmp_t == 0){
        memcpy(&int_attr, ((char*) attr + 1), INT_SIZE);
        memcpy(&int_value, ((char*) value + 1), INT_SIZE);
    }
    if(cmp_t == 1){
        memcpy(&real_attr, ((char*) attr + 1), REAL_SIZE);
        memcpy(&real_value, ((char*) value + 1), REAL_SIZE);
    }
    if(cmp_t == 2){
        cout << ((char *)attr)[2] << endl;
        //cout << ((char *)value)[2] << endl;
        unsigned str_size;
        memcpy(&str_size, (char *)attr + 1, 4);
        memcmp_val = memcmp((char *)attr + 5, (char *)value + 5, str_size);
    }
    
    switch(compOp){
        case 0: // ==
            if(cmp_t == 0){
                return int_attr == int_value;
            }
            if(cmp_t == 1){
                return real_attr == real_value;
            }
            if(cmp_t == 2){
                return memcmp_val == 0;
            }
            break;
        case 1: // <
            if(cmp_t == 0){
                return int_attr < int_value;
            }
            if(cmp_t == 1){
                return real_attr < real_value;
            }
            if(cmp_t == 2){
                return memcmp_val < 0;
            }
            break;
        case 2: // <=
            if(cmp_t == 0){
                return int_attr <= int_value;
            }
            if(cmp_t == 1){
                return real_attr <= real_value;
            }
            if(cmp_t == 2){
                return (memcmp_val == 0 || memcmp_val < 0);
            }
            break;
        case 3: // >
            if(cmp_t == 0){
                return int_attr > int_value;
            }
            if(cmp_t == 1){
                return real_attr > real_value;
            }
            if(cmp_t == 2){
                return memcmp_val > 0;
            }
            break;
        case 4: // >=
            if(cmp_t == 0){
                return int_attr >= int_value;
            }
            if(cmp_t == 1){
                return real_attr >= real_value;
            }
            if(cmp_t == 2){
                return (memcmp_val == 0 || memcmp_val > 0);
            }
            break;
        case 5: // !=
            if(cmp_t == 0){
                return int_attr != int_value;
            }
            if(cmp_t == 1){
                return real_attr != real_value;
            }
            if(cmp_t == 2){
                return memcmp_val != 0;
            }
            break;
        default: // no op
            break;
    }
    return -413;
}

int RecordBasedFileManager::scan_attrs(FileHandle &fileHandle, 
                const vector<Attribute> &recordDescriptor, 
                const string &conditionAttribute, const CompOp compOp,
                const void *value, const vector<string> &attributeNames,
                const RID &rid){
    int cmp_t = 0; // 0 = int comp, 1 = real comp, 2 = string comp
    
    // get the size of the attribute to be compared
    unsigned size = 0;
    for(unsigned i = 0; i < recordDescriptor.size(); ++i){
        // get the size of the attribute we want
        if(recordDescriptor[i].name == conditionAttribute){
            size = recordDescriptor[i].length + 1;
            
            // get comparison type
            if(recordDescriptor[i].type == 2){ // string compare
                cmp_t = 2;
            }else if(recordDescriptor[i].type == 1){ // real compare
                cmp_t = 1;
            } // default: int compare
        }
    }
    
    // get the attribute value for comparison
    void *attr = malloc(size);
    readAttribute(fileHandle, recordDescriptor, rid, conditionAttribute, attr);
    if((((char*)attr)[0] & (1<<7))  != 0){
        return 0; // comparison with null is false
    }
    
    // compare the values: 0 = false, !0 = true, -413 = error
    int cmp_val = -413;
    cmp_val = compare_attr(attr, compOp, value, cmp_t, size);
    
    free(attr);
    return cmp_val;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle,
      const vector<Attribute> &recordDescriptor,
      const string &conditionAttribute,
      const CompOp compOp,                  // comparision type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RBFM_ScanIterator &rbfm_ScanIterator)
{
    uint32_t curr_page, curr_record;
    unsigned num_pages = fileHandle.getNumberOfPages();
    void *page_data = malloc(PAGE_SIZE);
    
    // iterate through pages
    for(curr_page = 0; curr_page < num_pages; ++curr_page){
        if (fileHandle.readPage(curr_page, page_data))
            return RBFM_READ_FAILED;

        SlotDirectoryHeader slot_header = getSlotDirectoryHeader(page_data);
        
        // iterate through records
        for(curr_record = 0; curr_record < slot_header.recordEntriesNumber; ++curr_record){
            RID rid;
            rid.pageNum = curr_page;
            rid.slotNum = curr_record;
            
            // iterate through fields
            int scan_val = -413; // -413 = error, 0 = false, !0 = true
            scan_val = scan_attrs(fileHandle, recordDescriptor, conditionAttribute, compOp, value, attributeNames, rid);  
            if(scan_val == -413){
                return RBFM_SCAN_ERROR;
            }
            
            if(scan_val != 0){ // record fits criteria
                // store the rid into the iterator
                rbfm_ScanIterator.scan_result.push_back(rid);
            } 
        }
    }
    
    for(size_t k = 0; k < attributeNames.size(); ++k){
        rbfm_ScanIterator.attr_names.push_back(attributeNames[k]);
    }
    
    rbfm_ScanIterator.handle = fileHandle;
    rbfm_ScanIterator.recordDescriptor = recordDescriptor;
    
    free(page_data);
    return SUCCESS;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid)
{
    // get page
    void *page_data = malloc(PAGE_SIZE);
    if (fileHandle.readPage(rid.pageNum, page_data))
        return RBFM_READ_FAILED;
    
    // get slot directory header
    SlotDirectoryHeader slot_header = getSlotDirectoryHeader(page_data);
    if(slot_header.recordEntriesNumber <= rid.slotNum)
        return RBFM_SLOT_DN_EXIST;

    // get the slot directory record entry data
    SlotDirectoryRecordEntry record_entry = getSlotDirectoryRecordEntry(page_data, rid.slotNum);
    
    if(record_entry.length < 0){
        return RBFM_SLOT_DN_EXIST;
    }
    
    void *start, *dest;
    
    if(rid.slotNum < (slot_header.recordEntriesNumber - (unsigned)1)){ // record to be deleted is not in the last slot
        /*// move record entry data back one space, deleting the desired record's entry data
        unsigned entries_start = 8 + 8 * (rid.slotNum + 1);
        start = (char *)page_data + entries_start;
        unsigned entries_size = 8 * ((slot_header.recordEntriesNumber - 1) - rid.slotNum);
        //void *record_entries = malloc(entries_size);
        
        //memcpy(record_entries, start, entries_size);
        
        dest = (char *)page_data + (entries_start - 8);
        
        // move the data back one slot size
        memmove(dest, start, entries_size);*/
        
        // move records back one space, deleting desired record
        unsigned records_start = slot_header.freeSpaceOffset;
        start = (char *)page_data + records_start;
        unsigned records_size = record_entry.offset - records_start;
        dest = (char *)page_data + records_start + record_entry.length;
        
        // move records back one space
        memmove(dest, start, records_size);
        
        // update record offsets
        for(unsigned i = rid.slotNum + 1; i < slot_header.recordEntriesNumber; ++i){
            SlotDirectoryRecordEntry update_offsets = getSlotDirectoryRecordEntry(page_data, i);
            update_offsets.offset += record_entry.length;
            setSlotDirectoryRecordEntry(page_data, i, update_offsets);
        }
    }

    // update slot directory header
    slot_header.freeSpaceOffset += record_entry.length;
    slot_header.recordEntriesNumber -= 1;
    setSlotDirectoryHeader(page_data, slot_header);
    
    // instead of compacting slot directory, set length to negative
    record_entry.length *= -1;
    setSlotDirectoryRecordEntry(page_data, rid.slotNum, record_entry);
    
    // write page to disk
    if (fileHandle.writePage(rid.pageNum, page_data))
            return RBFM_WRITE_FAILED;
    
    free(page_data);
    return SUCCESS;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid)
{
    deleteRecord(fileHandle, recordDescriptor, rid);
    
    void *page_data = malloc(PAGE_SIZE);
    if (fileHandle.readPage(rid.pageNum, page_data))
        return RBFM_READ_FAILED;
    
    // get the size of the record.
    unsigned recordSize = getRecordSize(recordDescriptor, data);
    // get slot directory header
    SlotDirectoryHeader slot_header = getSlotDirectoryHeader(page_data);
    slot_header.recordEntriesNumber += 1;
    if(slot_header.recordEntriesNumber <= rid.slotNum)
        return RBFM_SLOT_DN_EXIST;

    // get the slot directory record entry data
    SlotDirectoryRecordEntry record_entry = getSlotDirectoryRecordEntry(page_data, rid.slotNum);
    
    if(getPageFreeSpaceSize(page_data) < recordSize){ // record too big for page
        RID new_rid;
        insertRecord(fileHandle, recordDescriptor, data, new_rid);
        record_entry.length = new_rid.pageNum;
        if(new_rid.slotNum != 0){
            record_entry.offset = new_rid.slotNum * -1;
        }else{
            record_entry.offset = -413;
        }
    }else{ // record fits into page
        setRecordAtOffset (page_data, slot_header.freeSpaceOffset - recordSize, recordDescriptor, data);
        
        record_entry.length = recordSize;
        record_entry.offset = slot_header.freeSpaceOffset - recordSize;
        
        slot_header.freeSpaceOffset = record_entry.offset;
        setSlotDirectoryHeader(page_data, slot_header);
    }
    
    setSlotDirectoryRecordEntry(page_data, rid.slotNum, record_entry);
    
    if (fileHandle.writePage(rid.pageNum, page_data))
            return RBFM_WRITE_FAILED;
    
    free(page_data);
    return SUCCESS;
}

void RBFM_ScanIterator::get_projected_at_offset(void *page, unsigned offset, const vector<Attribute> &recordDescriptor, void *data)
{
    // Pointer to start of record
    char *start = (char*) page + offset;

    // Allocate space for null indicator. The returned null indicator may be larger than
    // the null indicator in the table has had fields added to it
    int nullIndicatorSize = int(ceil((double) recordDescriptor.size() / CHAR_BIT));
    char nullIndicator[nullIndicatorSize];
    memset(nullIndicator, 0, nullIndicatorSize);

    // Get number of columns and size of the null indicator for this record
    RecordLength len = 0;
    memcpy (&len, start, sizeof(RecordLength));
    int recordNullIndicatorSize = int(ceil((double) len / CHAR_BIT));

    // Read in the existing null indicator
    memcpy (nullIndicator, start + sizeof(RecordLength), recordNullIndicatorSize);

    // If this new recordDescriptor has had fields added to it, we set all of the new fields to null
    for (unsigned i = len; i < recordDescriptor.size(); i++)
    {
        int indicatorIndex = (i+1) / CHAR_BIT;
        int indicatorMask  = 1 << (CHAR_BIT - 1 - (i % CHAR_BIT));
        nullIndicator[indicatorIndex] |= indicatorMask;
    }
    // Write out null indicator
    //memcpy(data, nullIndicator, nullIndicatorSize);

    // Initialize some offsets
    // rec_offset: points to data in the record. We move this forward as we read data from our record
    unsigned rec_offset = sizeof(RecordLength) + recordNullIndicatorSize + len * sizeof(ColumnOffset);
    // data_offset: points to our current place in the output data. We move this forward as we write data to data.
    unsigned data_offset = 0;//nullIndicatorSize;
    // directory_base: points to the start of our directory of indices
    char *directory_base = start + sizeof(RecordLength) + recordNullIndicatorSize;
    
    for (unsigned i = 0; i < recordDescriptor.size(); i++)
    {
        if (fieldIsNull(nullIndicator, i))
            continue;
        
        int is_projected = 0;
        for(unsigned k = 0; k < attr_names.size(); ++k){
            if(recordDescriptor[i].name == attr_names[k]){
                is_projected = 1;
                break;
            }
        }
        
        // Grab pointer to end of this column
        ColumnOffset endPointer;
        memcpy(&endPointer, directory_base + i * sizeof(ColumnOffset), sizeof(ColumnOffset));

        // rec_offset keeps track of start of column, so end-start = total size
        uint32_t fieldSize = endPointer - rec_offset;

        // Special case for varchar, we must give data the size of varchar first
        if(is_projected == 1){
            if (recordDescriptor[i].type == TypeVarChar)
            {
                memcpy((char*) data + data_offset, &fieldSize, VARCHAR_LENGTH_SIZE);
                data_offset += VARCHAR_LENGTH_SIZE;
            }
            // Next we copy bytes equal to the size of the field and increase our offsets
            memcpy((char*) data + data_offset, start + rec_offset, fieldSize);
            data_offset += fieldSize;
        }
        rec_offset += fieldSize;
    }
}

RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data)
{
    void * pageData = malloc(PAGE_SIZE);
    if (handle.readPage(rid.pageNum, pageData))
        return RBFM_READ_FAILED;

    // Checks if the specific slot id exists in the page
    SlotDirectoryHeader slotHeader = getSlotDirectoryHeader(pageData);
    if(slotHeader.recordEntriesNumber < rid.slotNum)
        return RBFM_SLOT_DN_EXIST;

    // Gets the slot directory record entry data
    SlotDirectoryRecordEntry recordEntry = getSlotDirectoryRecordEntry(pageData, rid.slotNum);
    
    if(recordEntry.length < 0){
        return RBFM_SLOT_DN_EXIST;
    }
    
    if(recordEntry.offset < 0){
        RID new_rid;
        new_rid.pageNum = recordEntry.length;
        if(recordEntry.offset != -413){
            new_rid.slotNum = recordEntry.offset * -1;
        }else{
            new_rid.slotNum = 0;
        }
        getNextRecord(new_rid, data);
        free(pageData);
        return SUCCESS;
    }
    
    // Retrieve the actual entry data
    get_projected_at_offset(pageData, recordEntry.offset, recordDescriptor, data);
    
    free(pageData);
    return SUCCESS;
}