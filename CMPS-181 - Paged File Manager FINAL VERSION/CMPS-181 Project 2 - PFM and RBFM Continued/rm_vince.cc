
#include <sys/stat.h>
#include <string.h>
#include <cmath>
#include "rm.h"
#include "rbfm.h"

RelationManager* RelationManager::_rm = 0;
RecordBasedFileManager* RelationManager::_rbf_manager = NULL;

RelationManager* RelationManager::instance()
{
    if(!_rm)
        _rm = new RelationManager();
    
    return _rm;
}

RelationManager::RelationManager()
{
    Attribute tableID, tableName, fileName, columnName, columnType, columnLength, columnPosition;
    
    tableID.name = "table-id"; tableID.type = TypeInt; tableID.length = 4;
    tableName.name = "table-name"; tableName.type = TypeVarChar; tableName.length = 50;
    fileName.name = "file-name"; fileName.type = TypeVarChar; fileName.length = 50;
    columnName.name = "column-name"; columnName.type = TypeVarChar; columnName.length = 50;
    columnType.name = "column-type"; columnType.type = TypeInt; columnType.length = 4;
    columnLength.name = "column-length"; columnLength.type = TypeInt; columnLength.length = 4;
    columnPosition.name = "column-position"; columnPosition.type = TypeInt; columnPosition.length = 4;
    
    initializeAttributesTable(tableID, tableName, fileName);
    initializeAttributesColumn(tableID, columnName, columnType, columnLength, columnPosition);
    
    // Initialize the internal RecordBasedFileManager instance
    _rbf_manager = RecordBasedFileManager::instance();
    
    num_tables = 0;
}

RelationManager::~RelationManager()
{
    if (_rm) {
        delete _rm;
        _rm = NULL;
    }
    
    else
        cout << "ERROR: _rm doesn't exist" << endl;
}

void RelationManager::initializeAttributesTable(Attribute tID, Attribute tName, Attribute fName)
{
    tAttributes.push_back(tID); tAttributes.push_back(tName); tAttributes.push_back(fName);
}

void RelationManager::initializeAttributesColumn(Attribute tID, Attribute cName, Attribute cType, Attribute cLength, Attribute cPosition)
{
    cAttributes.push_back(tID); cAttributes.push_back(cName); cAttributes.push_back(cType); cAttributes.push_back(cLength); cAttributes.push_back(cPosition);
}

// Check if a file already exists
bool fileExists(const string &fileName)
{
    // If stat fails, we can safely assume the file doesn't exist
    struct stat sb;
    return stat(fileName.c_str(), &sb) == 0;
}

void update_table_data(const int table_id, const int table_name_len, const string &table_name, const int file_name_len, const string &file_name, void *buffer)
{
    int offset = 0;
    void *null_field = calloc(1, sizeof(char));
    
    // Null-indicator for the fields
    memcpy((char *)buffer + offset, null_field, sizeof(char));
    offset += sizeof(char);
    
    // table id
    memcpy((char *)buffer + offset, &table_id, sizeof(int));
    offset += sizeof(int);
    
    // table name
    memcpy((char *)buffer + offset, &table_name_len, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, table_name.c_str(), table_name_len);
    offset += table_name_len;
    
    // file name
    memcpy((char *)buffer + offset, &file_name_len, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, file_name.c_str(), file_name_len);
    offset += file_name_len;
}

RC RelationManager::update_tables(FileHandle &handle, const string &tableName)
{
    RID rid;
    unsigned table_name_size = tableName.length();
    unsigned table_size = 1 + 4 + 8 + table_name_size * 2;
    void *buffer = malloc(table_size);
    
    update_table_data(num_tables + 1, table_name_size, tableName, table_name_size, tableName, buffer);
    
    if(_rbf_manager->insertRecord(handle, tAttributes, buffer, rid))
        return -1;
    
    num_tables++;
    
    free(buffer);
    return 0;
}

void update_column_data(const int table_id, const int col_name_length, const string &col_name, const int col_t, const int col_len, const int col_pos, void *buffer)
{
    int offset = 0;
    void *null_field = calloc(1, sizeof(char));
    
    // Null-indicator for the fields
    memcpy((char *)buffer + offset, null_field, sizeof(char));
    offset += sizeof(char);
    
    // table id
    memcpy((char *)buffer + offset, &table_id, sizeof(int));
    offset += sizeof(int);
    
    // column name
    memcpy((char *)buffer + offset, &col_name_length, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, col_name.c_str(), col_name_length);
    offset += col_name_length;
    
    // column type
    memcpy((char *)buffer + offset, &col_t, sizeof(int));
    offset += sizeof(int);
    
    // column length
    memcpy((char *)buffer + offset, &col_len, sizeof(int));
    offset += sizeof(int);
    
    // column position
    memcpy((char *)buffer + offset, &col_pos, sizeof(int));
    offset += sizeof(int);
}

RC RelationManager::update_cols(FileHandle &handle, const vector<Attribute> &attrs)
{
    RID rid;
    
    for(unsigned i = 0; i < attrs.size(); ++i){
        unsigned attr_name_size = attrs[i].name.length();
        unsigned tuple_size = 1 + 4 + 4 + attr_name_size + 4 + 4 + 4;
        void *buffer = malloc(tuple_size);
        
        update_column_data(num_tables, attr_name_size, attrs[i].name, attrs[i].type, attrs[i].length, i + 1, buffer);
        
        if(_rbf_manager->insertRecord(handle, cAttributes, buffer, rid))
            return -1;
        
        free(buffer);
    }
    
    return 0;
}

RC RelationManager::createCatalog()
{
    if (_rbf_manager->createFile("Tables"))
        return -1;
    
    if (_rbf_manager->createFile("Columns"))
        return -1;
    
    FileHandle t_handle, c_handle;
    
    // initialize Tables and Columns
    if(_rbf_manager->openFile("Tables", t_handle))
        return -1;
    
    if(_rbf_manager->openFile("Columns", c_handle))
        return -1;
    
    if(update_tables(t_handle, "Tables"))
        return -1;
    
    if(update_cols(c_handle, tAttributes))
        return -1;
    
    if(update_tables(t_handle, "Columns"))
        return -1;
    
    if(update_cols(c_handle, cAttributes))
        return -1;
    
    if(_rbf_manager->closeFile(t_handle))
        return -1;
    
    if(_rbf_manager->closeFile(c_handle))
        return -1;
    
    return 0;
}

RC RelationManager::deleteCatalog()
{
    if(deleteTable("Tables"))
        return -1;
    if(deleteTable("Columns"))
        return -1;
    
    return 0;
}

RC RelationManager::createTable(const string &tableName, const vector<Attribute> &attrs)
{
    if (_rbf_manager->createFile(tableName))
        return -1;
    
    FileHandle handle;
    
    // update Tables
    if(_rbf_manager->openFile("Tables", handle))
        return -1;
    
    if(update_tables(handle, tableName))
        return -1;
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    // update Columns
    if(_rbf_manager->openFile("Columns", handle))
        return -1;
    
    if(update_cols(handle, attrs))
        return -1;
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    return 0;
}

RC RelationManager::deleteTable(const string &tableName)
{
    if(_rbf_manager->destroyFile(tableName))
        return -1;
    
    return 0;
}

void populate_attrs(vector<Attribute> &attrs, RBFM_ScanIterator &scan_col)
{
    void *data = malloc(4080);
    unsigned name_len;
    
    for(unsigned i = 0; i < scan_col.scan_result.size(); ++i){
        scan_col.getNextRecord(scan_col.scan_result.at(i), data);
        unsigned offset = 0;
        
        // process data
        memcpy(&name_len, (char *)data + offset, sizeof(int));
        offset += sizeof(int);
        
        // get name
        void *name = malloc(name_len);
        memcpy(name, (char *)data + offset, name_len);
        offset += name_len;
        
        // get type
        AttrType type;
        memcpy(&type, (char *)data + offset, sizeof(int));
        offset += sizeof(int);
        
        // get length
        unsigned length;
        memcpy(&length, (char *)data + offset, sizeof(int));
        offset += 4;
        
        Attribute attr;
        for(unsigned k = 0; k < name_len; ++k){
            attr.name += ((char*)name)[k];
        }
        attr.type = type;
        attr.length = length;
        
        attrs.push_back(attr);
        
        free(name);
    }
    free(data);
}

RC RelationManager::getAttributes(const string &tableName, vector<Attribute> &attrs)
{
    FileHandle handle;
    RBFM_ScanIterator scan_tab;
    RBFM_ScanIterator scan_col;
    vector<string> attr_names;
    attr_names.push_back("table-id");
    unsigned table_id;
    
    if(_rbf_manager->openFile("Tables", handle))
        return -1;
    
    if(_rbf_manager->scan(handle, tAttributes, "table-name", EQ_OP, tableName.c_str(), attr_names, scan_tab))
        return -1;
    
    if(_rbf_manager->readAttribute(handle, tAttributes, scan_tab.scan_result.at(0), "table-id", &table_id))
        return -1;
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    if(_rbf_manager->openFile("Columns", handle))
        return -1;
    
    attr_names.pop_back();
    attr_names.push_back("column-name");
    attr_names.push_back("column-type");
    attr_names.push_back("column-length");
    
    if(_rbf_manager->scan(handle, cAttributes, "table-id", EQ_OP, &table_id, attr_names, scan_col))
        return -1;
    
    populate_attrs(attrs, scan_col);
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    return 0;
}

RC RelationManager::insertTuple(const string &tableName, const void *data, RID &rid)
{
    return -1;
}

RC RelationManager::deleteTuple(const string &tableName, const RID &rid)
{
    FileHandle handle;
    vector<Attribute> attribs;
    //RBFM_ScanIterator scan_tab;
    //RBFM_ScanIterator scan_col;
    
    if(_rbf_manager->openFile("Tables", handle))
        return -1;
    

    if(getAttributes(tableName, attribs)){
        if(_rbf_manager->deleteRecord(handle, attribs, rid))
           return -1;
        // deleteTable(tableName);  // Don't think this works like it should
    }
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    return 0;
}

RC RelationManager::updateTuple(const string &tableName, const void *data, const RID &rid)
{
    FileHandle handle;
    RBFM_ScanIterator scan_tab;
    vector<string> attr_names;
    vector<Attribute> getAttr;
    RC updatedTuple;
    // RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, const RID &rid)
    
    if(_rbf_manager->openFile("Tables", handle))
        return -1;

    if(_rbf_manager->scan(handle, tAttributes, "table-name", EQ_OP, tableName.c_str(), attr_names, scan_tab))
        return -1;
    
    if(!getAttributes(tableName, getAttr))
        return -1;
    
    if(_rbf_manager->updateRecord(handle, getAttr, data, rid))
        return -1;
        
        
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    return 0;
}

RC RelationManager::readTuple(const string &tableName, const RID &rid, void *data)
{
    return 0;
}

RC RelationManager::printTuple(const vector<Attribute> &attrs, const void *data)
{
    return -1;
}

RC RelationManager::readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data)
{
    FileHandle handle;
    unsigned table_id;
    vector<string> attr_names;
    RBFM_ScanIterator scan_tab;
    RBFM_ScanIterator scan_col;
    
    if(_rbf_manager->openFile("Tables", handle))
        return -1;
    
    if(_rbf_manager->closeFile(handle))
        return -1;
    
    if(_rbf_manager->scan(handle, cAttributes, "table-id", EQ_OP, &table_id, attr_names, scan_col))
        return -1;
    
    cout << tableName << endl;
    cout << table_id << endl;

    // what else to put to get eveything to display?
    
    return 0;
}

RC RelationManager::scan(const string &tableName,
                         const string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const vector<string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator)
{
    return -1;
}