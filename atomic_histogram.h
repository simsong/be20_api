/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * atomic_histogram uses cppmutex and std::map to implement an atomic histogram.
 *
 */

#ifndef ATOMICMAP_H
#define ATOMICMAP_H

#include "cppmutex.h"
#include <algorithm>
#include <map>
#include <tr1/unordered_map>

class atomicmap {
    typedef std::tr1::unordered_map<std::string,uint64_t> hmap_t;
    hmap_t amap; // the locked atomic map
    cppmutex M;                         // my lock
public:
    atomicmap():amap(),M(){};
    void add(const std::string &val,uint64_t count){
        cppmutex::lock lock(M);
        std::pair<hmap_t::iterator,uint64_t> ret;
        ret = amap.insert( std::pair<std::string,uint64_t>(val,count) );
        if(ret.second==false){
            (ret.first)->second += count;
        }
    }
    /**
     */
    typedef void (*dump_cb_t)(const std::string &val,uint64_t count);
    void dump(dump_cb_t dump_cb); // Dump the database to a user-provided callback.
    void dump_sorted(dump_cb_t dump_cb,int (*sorter)(const std::string &val,uint64_t count)); // dump sorted
    uint64_t size_estimate();     // Estimate the size of the database 
};

inline void atomicmap::dump (dump_cb_t dump_cb)
{
    cppmutex::lock lock(M);
    for(hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
        (*dump_cb)((*it).first,(*it).second);
    }
}

inline void atomicmap::dump_sorted (dump_cb_t dump_cb,int (*sorter)(const std::string &val,uint64_t count))
{
    /* Create a list, sort it, then report the sorted list */

    cppmutex::lock lock(M);
    for(hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
        (*dump_cb)((*it).first,(*it).second);
    }
}

inline uint64_t atomicmap::size_estimate()
{
    // http://stackoverflow.com/questions/720507/how-can-i-estimate-memory-usage-of-stlmap
    uint64_t size=0;
    for(hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
        size += sizeof(std::string) + (*it).first.size() + sizeof(uint64_t) + sizeof(_Rb_tree_node_base);
    }
    size += sizeof(amap);
    return size;
}

#endif
