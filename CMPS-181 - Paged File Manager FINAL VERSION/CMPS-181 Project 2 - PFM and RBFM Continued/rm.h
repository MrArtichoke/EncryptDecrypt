
#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>
#include <iostream>

#include "../rbf/rbfm.h"

using namespace std;

# define RM_EOF (-1)  // end of a scan operator

// RM_ScanIterator is an iterator to go through tuples
class RM_ScanIterator {
public:
    unsigned pos;
    
    RM_ScanIterator() { pos = 0; };
    ~RM_ScanIterator() {};
    
    // update the iterator to help with the rm::scan()
    void setRbfmScanIterator(RBFM_ScanIterator &scanner) {
        iter = scanner;
    };
    
    
    // "data" follows the same format as RelationManager::insertTuple()
    RC getNextTuple(RID &rid, void *data);
    RC close();
    
private:
    // to delegate to the rm_scaniterator to help with rm::scan()
    RBFM_ScanIterator iter;

};


// Relation Manager
class RelationManager
{
public:
  static RelationManager* instance();

  RC createCatalog();

  RC deleteCatalog();

  RC createTable(const string &tableName, const vector<Attribute> &attrs);

  RC deleteTable(const string &tableName);

  RC getAttributes(const string &tableName, vector<Attribute> &attrs);

  RC insertTuple(const string &tableName, const void *data, RID &rid);

  RC deleteTuple(const string &tableName, const RID &rid);

  RC updateTuple(const string &tableName, const void *data, const RID &rid);

  RC readTuple(const string &tableName, const RID &rid, void *data);

  // Print a tuple that is passed to this utility method.
  // The format is the same as printRecord().
  RC printTuple(const vector<Attribute> &attrs, const void *data);

  RC readAttribute(const string &tableName, const RID &rid, const string &attributeName, void *data);

  // Scan returns an iterator to allow the caller to go through the results one by one.
  // Do not store entire results in the scan iterator.
  RC scan(const string &tableName,
      const string &conditionAttribute,
      const CompOp compOp,                  // comparison type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RM_ScanIterator &rm_ScanIterator);

  void initializeAttributesTable(Attribute tID, Attribute tName, Attribute fName);
  void initializeAttributesColumn(Attribute tID, Attribute cName, Attribute cType, Attribute cLength, Attribute cPosition);
  RC update_tables(FileHandle &handle, const string &tableName);
  RC update_cols(FileHandle &handle, const vector<Attribute> &attrs);
    
  vector<Attribute> tAttributes;      // vector of table attributes
  vector<Attribute> cAttributes;      // vector of column attributes
  unsigned num_tables;

protected:
  RelationManager();
  ~RelationManager();

private:
  static RelationManager *_rm;
  static RecordBasedFileManager *_rbf_manager;
};
//
#endif