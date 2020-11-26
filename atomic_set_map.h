/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * defines atomic_map and atomic_set
 * 2020-07-06 - slg - start upgrade to C++14
 */

#ifndef ATOMIC_SET_MAP_H
#define ATOMIC_SET_MAP_H

#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

template <class TYPE > class atomic_set {
    std::mutex M {};
    std::unordered_set<TYPE>myset {};
public:
    atomic_set(){}
    bool contains(const TYPE &s){
        const std::lock_guard<std::mutex> lock(M);
        return myset.find(s)!=myset.end();
    }
    void insert(const TYPE &s){
        const std::lock_guard<std::mutex> lock(M);
        myset.insert(s);
    }
    bool check_for_presence_and_insert(const TYPE &s){
        const std::lock_guard<std::mutex> lock(M);
        if(myset.find(s)!=myset.end()) return true; // in the set
        myset.insert(s);                // otherwise insert it
        return false;                   // and return that it wasn't
    }
};

template <class T1,class T2 > class atomic_map {
    mutable std::mutex M {};            // allow modification in const methods
    std::map<T1, T2>mymap {};
public:
    atomic_map(){}
    ~atomic_map(){}
    T2 operator [](const T1 &key){
        const std::lock_guard<std::mutex> lock(M);
        return mymap[key];
    }
    typename std::map<T1, T2>::const_iterator find(const T1 &key) const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.find(key);
    }
    typename std::map<T1, T2>::const_iterator begin() const{
        const std::lock_guard<std::mutex> lock(M);
        return mymap.begin();
    }
    typename std::map<T1, T2>::const_iterator end() const{
        const std::lock_guard<std::mutex> lock(M);
        return mymap.end();
    }
    void delete_all() {
        const std::lock_guard<std::mutex> lock(M);
        for (auto it: mymap) {
            delete it.second;
        }
    }
    bool contains(T1 key) const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.find(key)!=mymap.end();
    }
    void insert(T1 key, T2 val){
        mymap[key] = val;
    }
};

#endif
