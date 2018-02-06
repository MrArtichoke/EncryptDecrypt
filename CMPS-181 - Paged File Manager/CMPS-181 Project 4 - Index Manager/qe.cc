#include <string.h>
#include <math.h>
#include <iomanip>

#include "qe.h"

RC Filter::getNextTuple(void *data)
{
    RC rc = 0;
    switch(child_type){
        case 0: // tablescan
            rc = ts->getNextTuple(data);
            break;
        case 1: // indexscan
            rc = is->getNextTuple(data);
            break;
        case 2: // filter
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
    
    return rc;
}

Filter::Filter(Iterator* input, const Condition &condition) 
{
    child_type = input->child_t();
    
    // for table scan
    string attr = "";
    
    // for index scan
    bool low_inc = false;
    bool high_inc = false;
    low = NULL;
    high = NULL;
    
    unsigned char_len;
    
    if(child_type == 1){ // if index scan
        if(condition.rhsValue.type == TypeVarChar){
            memcpy(&char_len, condition.rhsValue.data, sizeof(unsigned));
        }
    
        if(condition.op == EQ_OP || condition.op == NE_OP){
            low_inc = true;
            high_inc = true;
            if(condition.rhsValue.type == TypeVarChar){        
                low = malloc(char_len + sizeof(unsigned));
                memcpy(low, condition.rhsValue.data, char_len + sizeof(unsigned));
                
                high = malloc(char_len + sizeof(unsigned));
                memcpy(high, condition.rhsValue.data, char_len + sizeof(unsigned));
            }else{
                low = malloc(sizeof(unsigned));
                memcpy(low, condition.rhsValue.data, sizeof(unsigned));
                
                high = malloc(sizeof(unsigned));
                memcpy(high, condition.rhsValue.data, sizeof(unsigned));
            }
        }
    
        if(condition.op == LT_OP || condition.op == LE_OP){
            low_inc = true;
            if(condition.op == LE_OP){
                high_inc = true;
            }
            if(condition.rhsValue.type == TypeVarChar){
                high = malloc(char_len + sizeof(unsigned));
                memcpy(high, condition.rhsValue.data, char_len + sizeof(unsigned));
            }else{  
                high = malloc(sizeof(unsigned));
                memcpy(high, condition.rhsValue.data, sizeof(unsigned));
            }
        }
    
        if(condition.op == GT_OP || condition.op == GE_OP){
            high_inc = true;
            if(condition.op == GE_OP){
                low_inc = true;
            }
            if(condition.rhsValue.type == TypeVarChar){
                low = malloc(char_len + sizeof(unsigned));
                memcpy(low, condition.rhsValue.data, char_len + sizeof(unsigned));
            }else{  
                low = malloc(sizeof(unsigned));
                memcpy(low, condition.rhsValue.data, sizeof(unsigned));
            }
        }
    
        if(condition.op == NO_OP){
            low_inc = true;
            high_inc = true;
        }
    }
    
    switch(child_type){
        case 0: // tablescan
            ts = (TableScan *)input;
            attr += condition.lhsAttr.substr((ts->tableName).size() + 1, string::npos);
            ts->setIterator(attr, condition.op, condition.rhsValue.data);
            break;
        case 1: // indexscan
            is = (IndexScan *)input;
            is->setIterator(low, high, low_inc, high_inc);
            break;
        case 2: // filter
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
}

void Filter::getAttributes(vector<Attribute> &attrs) const
{
    switch(child_type){
        case 0: // tablescan
            ts->getAttributes(attrs);
            break;
        case 1: // indexscan
            is->getAttributes(attrs);
            break;
        case 2: // filter
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
}

void Project::project_record(void *data)
{
    vector<Attribute> attrs;
    fs->getAttributes(attrs);
    
    unsigned null_size = unsigned(ceil((double) attrs.size() / 8));
    unsigned new_null_size = unsigned(ceil((double) attrNames.size() / 8));
    
    unsigned offset = null_size;
    
    unsigned end_ptr = null_size;
    unsigned i;
    for(i = 0; i < attrs.size(); ++i){
        switch (attrs[i].type)
        {
            case TypeInt:
                end_ptr += INT_SIZE;
            break;
            case TypeReal:
                end_ptr += REAL_SIZE;
            break;
            case TypeVarChar:
                uint32_t varcharSize;
                // We have to get the size of the VarChar field by reading the integer that precedes the string value itself
                memcpy(&varcharSize, (char*) data + end_ptr, VARCHAR_LENGTH_SIZE);
                end_ptr += varcharSize + VARCHAR_LENGTH_SIZE;
            break;
        }
    }
    
    //cout << end_ptr << endl;
    
    bool keep = false;
    unsigned char_len, new_offset;
    unsigned j;
    for(i = 0; i < attrs.size(); ++i){
        for(j = 0; j < attrNames.size(); ++j){
            if(attrs[i].name == attrNames[j]){
                keep = true;
            }
        }
        
        if(attrs[i].type == TypeVarChar){
            memcpy(&char_len, (char *)data + offset, sizeof(unsigned));
        }
        
        if(keep == true){
            // increase offset accordingly
            if(attrs[i].type == TypeVarChar){
                offset += char_len;
            }
            offset += sizeof(unsigned);
        }else{
            if(attrs[i].type == TypeVarChar){
                new_offset = offset + char_len + sizeof(unsigned);
                memmove((char *)data + offset, (char *)data + new_offset, end_ptr - new_offset);
                end_ptr -= char_len + sizeof(unsigned);
            }else{
                new_offset = offset + sizeof(unsigned);
                memmove((char *)data + offset, (char *)data + new_offset, end_ptr - new_offset);
                end_ptr -= sizeof(unsigned);
            }
        }
        keep = false;
    }
    
    //cout << end_ptr << endl;
    //cout << "-------" << endl;
    
    // set new null indicator
    if(new_null_size != null_size){
        memmove((char *)data + new_null_size, (char *)data + null_size, end_ptr - null_size);
    }
    
    float t;
    memcpy(&t, (char *)data + 5, sizeof(float));
    //cout << fixed << t << endl;
}

RC Project::getNextTuple(void *data)
{
    RC rc = 0;
    switch(child_type){
        case 0: // tablescan
            rc = ts->getNextTuple(data);
            break;
        case 1: // indexscan
            rc = is->getNextTuple(data);
            break;
        case 2: // filter
            rc = fs->getNextTuple(data);
            project_record(data);
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
    
    return rc;
}

Project::Project(Iterator *input, const vector<string> &attrNames)
{
    child_type = input->child_t();
    unsigned i;
    
    switch(child_type){
        case 0: // tablescan
            ts = (TableScan *)input;
            ts->attrNames.clear();
            for(i = 0; i < attrNames.size(); ++i){
                ts->attrNames.push_back(attrNames[i].substr((ts->tableName).size() + 1, string::npos));
            }
            ts->setIterator(attrNames[0].substr((ts->tableName).size() + 1, string::npos), NO_OP, NULL);
            break;
        case 1: // indexscan
            is = (IndexScan *)input;
            //is->setIterator(low, high, low_inc, high_inc);
            break;
        case 2: // filter
            fs = (Filter *)input;
            this->attrNames = attrNames;
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
}

void Project::getAttributes(vector<Attribute> &attrs) const
{   
    vector<Attribute> old_attrs;
    unsigned i, j;
    switch(child_type){
        case 0: // tablescan
            ts->getAttributes(old_attrs);
            break;
        case 1: // indexscan
            is->getAttributes(old_attrs);
            break;
        case 2: // filter
            fs->getAttributes(old_attrs);
            for(i = 0; i < old_attrs.size(); ++i){
                for(j = 0; j < attrNames.size(); ++j){
                    if(old_attrs[i].name == attrNames[j]){
                        attrs.push_back(old_attrs[i]);
                    }
                }
            }
            break;
        case 3: // project
            break;
        case 4: // inl
            break;
    }
}

bool int_cmp(unsigned l, unsigned r, CompOp op){
    switch (op){
        case EQ_OP: return l == r;
        case LT_OP: return l < r;
        case GT_OP: return l > r;
        case LE_OP: return l <= r;
        case GE_OP: return l >= r;
        case NE_OP: return l != r;
        case NO_OP: return true;
        default: return false;
    }
    
    return false;
}

bool real_cmp(float l, float r, CompOp op){
    switch (op){
        case EQ_OP: return l == r;
        case LT_OP: return l < r;
        case GT_OP: return l > r;
        case LE_OP: return l <= r;
        case GE_OP: return l >= r;
        case NE_OP: return l != r;
        case NO_OP: return true;
        default: return false;
    }
    
    return false;
}

bool char_cmp(char *l, char *r, CompOp op){
    if (op == NO_OP)
        return true;
    
    int cmp = strcmp(l, r);
    switch (op){
        case EQ_OP: return cmp == 0;
        case LT_OP: return cmp <  0;
        case GT_OP: return cmp >  0;
        case LE_OP: return cmp <= 0;
        case GE_OP: return cmp >= 0;
        case NE_OP: return cmp != 0;
        default: return false;
    }
    
    return false;
}

bool check_condition(AttrType lt, AttrType rt, unsigned loffset, unsigned roffset, CompOp op, void *data, void *r_data)
{   
    if(lt == TypeInt){
        unsigned l,r;
        memcpy(&l, (char *)data + loffset, sizeof(unsigned));
        memcpy(&r, (char *)r_data + roffset, sizeof(unsigned));
        return int_cmp(l, r, op);
    }
    
    if(lt == TypeReal){
        float l,r;
        memcpy(&l, (char *)data + loffset, sizeof(float));
        memcpy(&r, (char *)r_data + roffset, sizeof(float));
        //cout << fixed << l << " " << r << endl;
        return real_cmp(l, r, op);
    }
    
    if(lt == TypeVarChar){
        unsigned size;
        
        memcpy(&size, (char *)data + loffset, sizeof(unsigned));
        char l[size + 1];
        memcpy(l, (char*)data + loffset + sizeof(unsigned), size);
        l[size] = '\0';
        
        memcpy(&size, (char *)r_data + roffset, sizeof(unsigned));
        char r[size + 1];
        memcpy(r, (char*)r_data + roffset + sizeof(unsigned), size);
        r[size] = '\0';
        
        return char_cmp(l, r, op);
    }
    
    return false;
}

void set_null_ind(unsigned attr_size, unsigned r_attr_size, unsigned new_ns, void *data, void *r_data)
{
    unsigned null_size = unsigned(ceil((double) attr_size / 8));
    unsigned r_null_size = unsigned(ceil((double) r_attr_size / 8));
    
    char null_ind[new_ns];
    memset(null_ind, 0, new_ns);
    memcpy (null_ind, data, null_size);
    
    memcpy(data, null_ind, null_size); // dont actually do anything cause im tired of this
    
    char r_null_ind[r_null_size];
    memset(r_null_ind, 0, r_null_size);
    memcpy (r_null_ind, r_data, r_null_size);
}

RC INLJoin::getNextTuple(void *data)
{
    RC rc = 0;
    RC rc2 = 0;
    
    unsigned i, j, char_len;
    unsigned loffset;
    unsigned roffset;
    
    unsigned null_size = unsigned(ceil((double) lattrs.size() / 8));
    unsigned r_null_size = unsigned(ceil((double) rattrs.size() / 8));
    unsigned new_null_size = unsigned(ceil((double) (lattrs.size() + rattrs.size()) / 8));
    
    void *r_data = malloc(r_size);
    
    switch(child_type){
        case 0: // tablescan
            while(!(rc = ts->getNextTuple(data))){
                loffset = null_size;
                for(i = 0; i < lattrs.size(); ++i){
                    if(lattr == lattrs[i].name){
                        break;
                    }
                    
                    if(lattrs[i].type == TypeVarChar){
                        memcpy(&char_len, (char *)data + loffset, sizeof(unsigned));
                        
                        loffset += char_len;
                    }
                    loffset += sizeof(unsigned);
                }

                while(!(rc2 = ris->getNextTuple(r_data))){
                    roffset = r_null_size;
                    for(j = 0; j < rattrs.size(); ++j){
                        if(rattr == rattrs[j].name){
                            break;
                        }
                        
                        if(rattrs[j].type == TypeVarChar){
                            memcpy(&char_len, (char *)r_data + roffset, sizeof(unsigned));
                        
                            roffset += char_len;
                        }
                        roffset += sizeof(unsigned);
                    }
                
                    if(check_condition(lattrs[i].type, rattrs[j].type, loffset, roffset, op, data, r_data) == true){
                        // concatenate r_data onto data 
                        set_null_ind(lattrs.size(), rattrs.size(), new_null_size, data, r_data);
                        
                        for(; i < lattrs.size(); ++i){
                    
                            if(lattrs[i].type == TypeVarChar){
                                memcpy(&char_len, (char *)data + loffset, sizeof(unsigned));
                        
                                loffset += char_len;
                            }
                            loffset += sizeof(unsigned);
                        }
                        for(; j < rattrs.size(); ++j){
                    
                            if(rattrs[j].type == TypeVarChar){
                                memcpy(&char_len, (char *)r_data + roffset, sizeof(unsigned));
                        
                                roffset += char_len;
                            }
                            roffset += sizeof(unsigned);
                        }
                        memcpy((char *)data + loffset, (char *)r_data + r_null_size, roffset - r_null_size);
                        free(r_data);
                        return rc;
                    }
                }
            }
            break;
        case 1: // indexscan
            rc = is->getNextTuple(data);
            break;
        case 2: // filter
            break;
        case 3: // project
            while(!(rc = ps->getNextTuple(data))){
                loffset = null_size;
                for(i = 0; i < lattrs.size(); ++i){
                    if(lattr == lattrs[i].name){
                        break;
                    }
                    
                    if(lattrs[i].type == TypeVarChar){
                        memcpy(&char_len, (char *)data + loffset, sizeof(unsigned));
                        
                        loffset += char_len;
                    }
                    loffset += sizeof(unsigned);
                }
            
                while(!(rc2 = ris->getNextTuple(r_data))){
                    roffset = r_null_size;
                    for(j = 0; j < rattrs.size(); ++j){
                        if(rattr == rattrs[j].name){
                            break;
                        }
                        
                        if(rattrs[j].type == TypeVarChar){
                            memcpy(&char_len, (char *)r_data + roffset, sizeof(unsigned));
                        
                            roffset += char_len;
                        }
                        roffset += sizeof(unsigned);
                    }
                
                    if(check_condition(lattrs[i].type, rattrs[j].type, loffset, roffset, op, data, r_data) == true){
                        // concatenate r_data onto data 
                        set_null_ind(lattrs.size(), rattrs.size(), new_null_size, data, r_data);
                        for(; i < lattrs.size(); ++i){
                    
                            if(lattrs[i].type == TypeVarChar){
                                memcpy(&char_len, (char *)data + loffset, sizeof(unsigned));
                        
                                loffset += char_len;
                            }
                            loffset += sizeof(unsigned);
                        }
                        for(; j < rattrs.size(); ++j){
                    
                            if(rattrs[j].type == TypeVarChar){
                                memcpy(&char_len, (char *)r_data + roffset, sizeof(unsigned));
                        
                                roffset += char_len;
                            }
                            roffset += sizeof(unsigned);
                        }
                        memcpy((char *)data + loffset, (char *)r_data + r_null_size, roffset - r_null_size);
                        free(r_data);
                        return rc;
                    }
                }
            }
            break;
        case 4: // inl
            break;
    }
    
    free(r_data);
    return rc;
}

INLJoin::INLJoin(Iterator *leftIn,           // Iterator of input R
               IndexScan *rightIn,          // IndexScan Iterator of input S
               const Condition &condition   // Join condition
        )
{
    child_type = leftIn->child_t();
    
    op = condition.op;
    lattr = condition.lhsAttr;//.substr((ts->tableName).size() + 1, string::npos);
    rattr = condition.rhsAttr;//.substr((ts->tableName).size() + 1, string::npos);
    
    unsigned i;
    vector<Attribute> attrs;
    
    switch(child_type){
        case 0: // tablescan
            ts = (TableScan *)leftIn;
            
            ts->attrNames.clear();
            ts->getAttributes(attrs);

            for(i = 0; i < attrs.size(); ++i){
                lattrs.push_back(attrs[i]);
                ts->attrNames.push_back(attrs[i].name.substr((ts->tableName).size() + 1, string::npos));
            }
            ts->setIterator(ts->attrNames[0], NO_OP, NULL);
            break;
        case 1: // indexscan
            is = (IndexScan *)leftIn;
            break;
        case 2: // filter
            break;
        case 3: // project
            ps = (Project *)leftIn;
            
            ps->getAttributes(attrs);
            for(i = 0; i < attrs.size(); ++i){
                lattrs.push_back(attrs[i]);
            }
            break;
        case 4: // inl
            break;
    }
    
    ris = rightIn;
    
    attrs.clear();
    ris->getAttributes(attrs);
    
    unsigned size = unsigned(ceil((double) attrs.size() / 8));
    
    for(i = 0; i < attrs.size(); ++i){
        rattrs.push_back(attrs[i]);
        size += attrs[i].length;
    }
    
    r_size = size;
}
