#ifndef ATOMIC_HISTOGRAM_H
#define ATOMIC_HISTOGRAM_H

#include <vector>
#include <algorithm>
#include <set>
#include <map>
#include <mutex>
#include <unordered_map>

template <class TYPE,class CTYPE> class atomic_histogram {
    typedef std::unordered_map<TYPE,CTYPE> hmap_t ;
    hmap_t  amap {};                      // the locked atomic map
    std::mutex M {};                      // my lock
public:
    atomic_histogram(){};

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
        for (auto const &it:amap){
            int ret = (*dump_cb)(user,it.first,it.second);
            if (ret<0) return;
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
            for (const auto &it: amap){
                evect.push_back( new ReportElement(it.first, it.second));
            }
        }
        std::sort(evect.begin(), evect.end(), ReportElement::compare);
        for (const auto &it:evect){
            int ret = (*dump_cb)(user,it->value,it->tally);
            if(ret<0) break;
        }

    }
    uint64_t size_estimate() const;     // Estimate the size of the database
};

#endif
