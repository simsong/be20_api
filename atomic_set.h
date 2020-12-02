/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * defines atomic_map and atomic_set.
 * This is a nice lightweight atomic set when not much else is needed.
 *
 * 2020-07-06 - slg - Upgraded to to C++17.
 */

#ifndef ATOMIC_SET_H
#define ATOMIC_SET_H

#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

template <class TYPE > class atomic_set {
    mutable std::mutex M {};
    std::unordered_set<TYPE>myset {};
public:
    atomic_set(){}
    bool contains(const TYPE &s) const{
        const std::lock_guard<std::mutex> lock(M);
        return myset.find(s)!=myset.end();
    }
    void insert(const TYPE &s) {
        const std::lock_guard<std::mutex> lock(M);
        myset.insert(s);
    }
    bool check_for_presence_and_insert(const TYPE &s){
        const std::lock_guard<std::mutex> lock(M);
        if(myset.find(s)!=myset.end()) return true; // in the set
        myset.insert(s);                // otherwise insert it
        return false;                   // and return that it wasn't
    }
    size_t size() const {
        const std::lock_guard<std::mutex> lock(M);
        return myset.size();
    }
};

#endif
