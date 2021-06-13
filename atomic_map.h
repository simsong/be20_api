/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * defines atomic_map and atomic_set.
 * This is a nice lightweight atomic set when not much else is needed.
 *
 * 2020-07-06 - slg - Upgraded to to C++17.
 */

#ifndef ATOMIC_MAP_H
#define ATOMIC_MAP_H

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <class T1, class T2> class atomic_map {
    // Mutex M protects mymap.
    // It is mutable to allow modification in const methods
    mutable std::mutex M{};
    std::map<T1, T2> mymap{};

public:
    atomic_map() {}
    ~atomic_map() {}
    class KeyError : public std::exception {
        std::string m_error{};

    public:
        KeyError(std::string_view error) : m_error(error) {}
        const char* what() const noexcept override { return m_error.c_str(); }
    };
    T2& operator[](const T1& key) {
        const std::lock_guard<std::mutex> lock(M);
        auto it = mymap.find(key);
        if (it == mymap.end()) { throw KeyError(key); }
        return it->second;
    }
    typename std::map<T1, T2>::const_iterator find(const T1& key) const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.find(key);
    }
    typename std::map<T1, T2>::const_iterator begin() const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.begin();
    }
    typename std::map<T1, T2>::const_iterator end() const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.end();
    }
    void clear() {
        const std::lock_guard<std::mutex> lock(M);
        mymap.clear();
    }
    size_t size() const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.size();
    }
    bool contains(T1 key) const {
        const std::lock_guard<std::mutex> lock(M);
        return mymap.find(key) != mymap.end();
    }
    void insert(T1 key, T2 val) {
        const std::lock_guard<std::mutex> lock(M);
        mymap[key] = val;
    }
    void delete_all() {
        /* deletes all of the values!  Values must be pointers. This might be better done with unique_ptr()*/
        const std::lock_guard<std::mutex> lock(M);
        for (auto it : mymap) { delete it.second; }
    }
    void insertIfNotContains(T1 key, T2 val) {
        const std::lock_guard<std::mutex> lock(M);
        if (mymap.find(key) == mymap.end()) { mymap[key] = val; }
    }
    struct AMReportElement {
        AMReportElement(T1 key_, T2 value_) : key(key_), value(value_){};
        AMReportElement(T1 key_) : key(key_){};
        AMReportElement(){};
        T1 key{};
        T2 value{};
        bool operator==(const AMReportElement& a) const { return (this->key == a.key) && (this->value == a.value); }
        bool operator!=(const AMReportElement& a) const { return !(*this == a); }
        bool operator<(const AMReportElement& a) const {
            if (this->key < a.key) return true;
            if ((this->key == a.key) && (this->value < a.value)) return true;
            return false;
        }
        static bool compare(const AMReportElement& e1, const AMReportElement& e2) { return e1 < e2; }
        virtual ~AMReportElement(){};
        size_t bytes() const { return sizeof(*this) + value.bytes(); } // number of bytes used by object
    };
    typedef std::vector<AMReportElement> report;
    report dump(bool sorted = false) const {
        report r;
        {
            /* Protect access to mymap with mutex */
            const std::lock_guard<std::mutex> lock(M);
            for (auto it : mymap) { r.push_back(AMReportElement(it.first, it.second)); }
        }
        if (sorted) {
            std::sort(r.rbegin(), r.rend(), AMReportElement::compare); // reverse sort
        }
        return r;
    }
};

#endif
