#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>

#include "../rbf/rbfm.h"

typedef struct idir
{
    unsigned nchildren;
} idir;

typedef struct ldir
{
    // if lptr or rptr are 0, then it is first/last leaf in list
    unsigned lptr; 
    unsigned rptr;
    unsigned nentries;
    unsigned free_space_offset;
} ldir;

# define IX_EOF (-1)  // end of the index scan

class IX_ScanIterator;
class IXFileHandle;

class IndexManager {

    public:
        static IndexManager* instance();
        unsigned levels;
        vector<unsigned> path;

        // Create an index file.
        RC createFile(const string &fileName);

        // Delete an index file.
        RC destroyFile(const string &fileName);

        // Open an index and return an ixfileHandle.
        RC openFile(const string &fileName, IXFileHandle &ixfileHandle);

        // Close an ixfileHandle for an index.
        RC closeFile(IXFileHandle &ixfileHandle);

        // Insert an entry into the given index that is indicated by the given ixfileHandle.
        RC insertEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid);

        // Delete an entry from the given index that is indicated by the given ixfileHandle.
        RC deleteEntry(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid);

        // Initialize and IX_ScanIterator to support a range search
        RC scan(IXFileHandle &ixfileHandle,
                const Attribute &attribute,
                const void *lowKey,
                const void *highKey,
                bool lowKeyInclusive,
                bool highKeyInclusive,
                IX_ScanIterator &ix_ScanIterator);

        // Print the B+ tree in pre-order (in a JSON record format)
        void printBtree(IXFileHandle &ixfileHandle, const Attribute &attribute) const;

    protected:
        IndexManager();
        ~IndexManager();

    private:
        static IndexManager *_index_manager;
        static PagedFileManager *_pf_manager;
        
        unsigned get_num_children(void * pageData) const;
        unsigned getIndexPageFreeSpaceSize(const Attribute &attr, void * page);
        void print_leaf(void *page, const Attribute &attr, const unsigned flag) const; // 0 = root, 1 = non-root
        void initialize_leaves_from_root(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, 
                                 const void *key, const RID &rid, void *page);
        void print_index(IXFileHandle &ixfileHandle, void * page, const unsigned nchildren, const Attribute &attr, unsigned current_level) const;
        void find_leaf(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid, void *page, const unsigned nchildren, const unsigned parent_ptr, const unsigned next_level);
        void split_leaf(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, 
                                 const void *key, const RID &rid, const unsigned parent_ptr, const unsigned ind_ptr, void *parent_page, void *page);
        void split_index(IXFileHandle &ixfileHandle, const Attribute &attr, const unsigned de_size, const void *key, const unsigned leaf_ptr);
        RC search_and_delete(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const RID &rid, const unsigned current_level, const unsigned current_ptr);
        RC delete_from_leaf(IXFileHandle &ixfileHandle, const Attribute &attr, const void *key, const RID &rid, void *page);
        unsigned search(IXFileHandle &ixfileHandle, const Attribute &attribute, const void *key, const unsigned current_level, const unsigned current_ptr);
};


class IX_ScanIterator {
    public:
	    IXFileHandle *ixfileHandle;
	    int currentPageNum;
	    int lastPageNum;
	    bool startFlag;
	    bool endOfFile;
	    Attribute attribute;
	    bool lowKeyInclusive;
	    bool highKeyInclusive;
	    void *lowKey;
	    void *highKey;
	    int currentEntryNum;
		// Constructor
        IX_ScanIterator();

        // Destructor
        ~IX_ScanIterator();

        // Get next matching entry
        RC getNextEntry(RID &rid, void *key);

        // Terminate index scan
        RC close();
};



class IXFileHandle {
    public:

    friend class IndexManager;
    
    // variables to keep counter for each operation
    unsigned ixReadPageCounter;
    unsigned ixWritePageCounter;
    unsigned ixAppendPageCounter;
    unsigned is_search;

    // Constructor
    IXFileHandle();

    // Destructor
    ~IXFileHandle();

	// Put the current counter values of associated PF FileHandles into variables
	RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount);
	
    FileHandle *get_handle();
	private:
	
	FileHandle *handle;
	
	void set_handle(FileHandle *fh);

};

#endif