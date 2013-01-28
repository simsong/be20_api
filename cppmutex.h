/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * Cppmutex is an easy-to-use mutex class.
 * Create a cppmutex instance for a mutex.
 * Create a cppmutex::lock(M) object to get a lock; delete the object to free it.
 *
 * BE SURE THAT HAVE_PTHREAD IS DEFINED BEFORE INCLUDING THIS FILE,
 * otherwise this class becomes a no-op...
 */


#ifndef CPPMUTEX_H
#define CPPMUTEX_H

#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <string.h>

#include <pthread.h>
#include <exception>

class cppmutex {
    class not_impl: public std::exception {
        const char *what() const throw() {
            return "copying feature_recorder objects is not implemented.";
        }
    };

#ifdef HAVE_PTHREAD
    pthread_mutex_t M;
#else
    int M;                              // so M() works
#endif
    cppmutex(const cppmutex &c) __attribute__((__noreturn__)) :M(){throw new not_impl();}
    const cppmutex &operator=(const cppmutex &cp){ throw new not_impl();}
public:
    cppmutex():M(){
#ifdef HAVE_PTHREAD
        if(pthread_mutex_init(&M,NULL)){
            std::cerr << "pthread_mutex_init failed: " << strerror(errno) << "\n";
            exit(1);
        }
#endif
    }
    virtual ~cppmutex(){
#ifdef HAVE_PTHREAD
        pthread_mutex_destroy(&M);
#endif
    }
    class lock {                        // get
    private:
        cppmutex &myMutex;      
        lock(const lock &lock_):myMutex(lock_.myMutex){}
    public:
        lock(cppmutex &m):myMutex(m){
#ifdef HAVE_PTHREAD
            pthread_mutex_lock(&myMutex.M);
#endif
        }
        ~lock(){
#ifdef HAVE_PTHREAD
            pthread_mutex_unlock(&myMutex.M);
#endif
        }
    };
};

#endif
