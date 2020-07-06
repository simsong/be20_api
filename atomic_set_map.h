/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * defines atomic_map and atomic_set
 * 2020-07-06 - slg - start upgrade to C++14
 */

#ifndef ATOMIC_SET_MAP_H
#define ATOMIC_SET_MAP_H


#include <algorithm>
#include <set>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

template <class TYPE,class CTYPE> class atomic_histogram {
    typedef std::unordered_map<TYPE,CTYPE> hmap_t;
    hmap_t  amap;                         // the locked atomic map
    std::mutex M;                         // my lock
public:
    atomic_histogram():amap(),M(){};

    // The callback is used to report the histogram.
    // The callback returns '0' if no error is encountered, '-1' if the dumping should stop
    typedef int (*dump_callback_t)(void *user, const TYPE &val, const CTYPE &count);

    // add and return the count
    // http://www.cplusplus.com/reference/unordered_map/unordered_map/insert/
    CTYPE add(const TYPE &val,const CTYPE &count){
        const std::lock_guard<std::mutex> lock(M);
        std::pair<typename hmap_t::iterator,bool> p = amap.insert(std::make_pair(val,count));

        if (!p.second) {
            p.first->second += count;
        }
        return p.first->second;
    }

    // Dump the database to a user-provided callback.
    void dump(void *user,dump_callback_t dump_cb) const {
        const std::lock_guard<std::mutex> lock(M);
        for(typename hmap_t::const_iterator it = amap.begin();it!=amap.end();it++){
            int ret = (*dump_cb)(user,(*it).first,(*it).second);
            if(ret<0) return;
        }
    }
    struct ReportElement {
        ReportElement(TYPE aValue,uint64_t aTally):value(aValue),tally(aTally){ }
        TYPE value;
        CTYPE tally;
        static bool compare(const ReportElement *e1,
                            const ReportElement *e2) {
	    if (e1->tally > e2->tally) return true;
	    if (e1->tally < e2->tally) return false;
	    return e1->value < e2->value;
	}
        virtual ~ReportElement(){};
    };
    typedef std::vector< const ReportElement *> element_vector_t;

    // dump_sorted() can't be const because the callback might modify *user.
    // (this took me a long time to figure out.)
    //
    void     dump_sorted(void *user,dump_callback_t dump_cb)  {
        /* Create a list of new elements, sort it, then report the sorted list */
        element_vector_t  evect;
        {
            const std::lock_guard<std::mutex> lock(M);
            for(const auto &it: amap){
                evect.push_back( new ReportElement(it.first, it.second));
            }
        }
        std::sort(evect.begin(), evect.end(), ReportElement::compare);
        for(const auto &it:evect){
            int ret = (*dump_cb)(user,it->value,it->tally);
            if(ret<0) break;
        }

    }
    uint64_t size_estimate() const;     // Estimate the size of the database 
};

template <class TYPE > class atomic_set {
    std::mutex M;
    std::unordered_set<TYPE>myset;
public:
    atomic_set():M(),myset(){}
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

#endif
