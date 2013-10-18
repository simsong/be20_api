/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * defines atomic_map and atomic_set
 *
 */

#ifndef ATOMIC_SET_MAP_H
#define ATOMIC_SET_MAP_H


#include "cppmutex.h"
#include <algorithm>
#include <map>

#if defined(HAVE_UNORDERED_MAP)
# include <unordered_map>
# undef HAVE_TR1_UNORDERED_MAP           // be sure we don't use it
#else
# if defined(HAVE_TR1_UNORDERED_MAP)
#  include <tr1/unordered_map>
# else
#  error Requires <unordered_map> or <tr1/unordered_map>
# endif
#endif

#if defined(HAVE_UNORDERED_SET)
#include <unordered_set>
#undef HAVE_TR1_UNORDERED_SET           // be sure we don't use it
#else
#if defined(HAVE_TR1_UNORDERED_SET)
#include <tr1/unordered_set>
#else
#error Requires <unordered_set> or <tr1/unordered_set>
#endif
#endif

template <class TYPE> class atomic_histogram {
#ifdef HAVE_UNORDERED_MAP
    typedef std::unordered_map<TYPE,uint64_t> hmap_t;
#endif
#ifdef HAVE_TR1_UNORDERED_MAP
    typedef std::tr1::unordered_map<TYPE,uint64_t> hmap_t;
#endif
    hmap_t amap; // the locked atomic map
    cppmutex M;                         // my lock
public:
    atomic_histogram():amap(),M(){};
    typedef void (*dump_cb_t)(const TYPE &val,uint64_t count);
    // add and return the count
    uint64_t add(const TYPE &val,uint64_t addend=1){
        cppmutex::lock lock(M);
        return (amap[val] += addend);
    }

    // Dump the database to a user-provided callback.
    void     dump(dump_cb_t dump_cb){
        cppmutex::lock lock(M);
        for(typename hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
            (*dump_cb)((*it).first,(*it).second);
        }
    }
    void     dump_sorted(dump_cb_t dump_cb,
                         int (*sorter)(std::pair<const TYPE &,uint64_t>,
                                       std::pair<const TYPE &,uint64_t>)){
        /* Create a list, sort it, then report the sorted list */
        cppmutex::lock lock(M);
        for(typename hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
            (*dump_cb)((*it).first,(*it).second);
        }
    }
    uint64_t size_estimate();     // Estimate the size of the database 
};

template <class TYPE > class atomic_set {
    cppmutex M;
#ifdef HAVE_UNORDERED_SET
    std::unordered_set<TYPE>myset;
#endif
#ifdef HAVE_TR1_UNORDERED_SET
    std::tr1::unordered_set<TYPE>myset;
#endif
public:
    atomic_set():M(),myset(){}
    bool contains(const TYPE &s){
        cppmutex::lock lock(M);
        return myset.find(s)!=myset.end();
    }
    void insert(const TYPE &s){
        cppmutex::lock lock(M);
        myset.insert(s);
    }
    bool check_for_presence_and_insert(const TYPE &s){
        cppmutex::lock lock(M);
        if(myset.find(s)!=myset.end()) return true; // in the set
        myset.insert(s);                // otherwise insert it
        return false;                   // and return that it wasn't
    }
};

#endif
