
#pragma once

#ifndef _H_ROTATE_SKIPLIST
#define _H_ROTATE_SKIPLIST

// This contains std::less and std::equal_to
#include <functional>
#include <atomic>

// Traditional C libraries 
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <pthread.h>

namespace rotate_skiplist {

// This prevents compiler rearranging the code cross this point
// Usually a hardware memury fence is not needed for x86-64
#define BARRIER() asm volatile("" ::: "memory")

// Note that since this macro may be defined also in other modules, we
// only define it if it is missing from the context
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// This macro allocates a cahce line aligned chunk of memory
#define CACHE_ALIGNED_ALLOC(_s)                                 \
    ((void *)(((unsigned long)malloc((_s)+CACHE_LINE_SIZE*2) +  \
        CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE-1)))

/*
 * class GCState
 */
class GCState {

};

/*
 * class ThreadState - This class implements thread state related functions
 *
 * We allocate thread state for each thread as its thread local data area, and
 * chain them up as a linked list. When a thread is created, we try to search 
 * a nonoccupied thread state object and assign it to the thread, or create
 * a new one if none was found. When a thread exist we simply clear the owned
 * flag to release a thread state object
 */
class ThreadState {
 public:
  // This is the current thread ID
  unsigned int id;
  
  // Whether the thread state object is already owned by some
  // active thread. We set this flag when a thread claims a certain
  // object, and clears the flag when thread exits by using a callback
  std::atomic_flag owned;

  // Points to the next state object in the linked list
  ThreadState *next_p;
  // Points to the gargabe collection state for the current thread
  GCState *gc_p;

  ///////////////////////////////////////////////////////////////////
  // Static data for maintaining the global linked list & thread ID
  ///////////////////////////////////////////////////////////////////

  // This is used as a key to access thread local data (i.e. pthread library
  // will return the registered thread local object when given the key)
  static pthread_key_t thread_state_key;
  // This is the ID of the next thread
  static std::atomic<unsigned int> next_id;
  // This is the head of the linked list
  static ThreadState *thread_state_head_p;

  // This is used for debugging purpose to check whether the global states
  // are initialized
  static bool inited;

  /*
   * ClearOwnedFlag() - This is called when the threda exits and the OS
   *                    destroies the thread local structure
   *
   * The function prototype is defined by the pthread library
   */
  static void ClearOwnedFlag(void *data) {
    ThreadState *thread_state_p = static_cast<ThreadState *>(data);

    // This does not have to be atomic
    // Frees the thread state object for the next thread
    thread_state_p->owned.clear();
    
    return;
  }

  /*
   * Init() - Initialize the thread local environment
   */
  static void Init() {
    assert(inited == false);

    // Initialize the thread ID allocator
    next_id = 0U;
    // There is no element in the linked list
    thread_state_head_p = nullptr;

    // Must do a barrier to make sure all cores observe the same value
    BARRIER();

    // We use the clear owned flag as a call back which will be 
    // invoked when the thread exist
    if (pthread_key_create(&thread_state_key, ClearOwnedFlag)) {
      // Use this for system call failure as it translates errno
      // The format is: the string here + ": " + strerror(errno)
      perror("ThreadState::Init::pthread_key_create()");
      exit(1);
    }

    // Finally
    inited = true;
    return;
  }

  /*
   * EnterCritical() - Inform GC that some thread is still on the structure
   */
  void EnterCritical() {

  }

 private:
  /*
   * GetCurrentThreadState() - This function returns the current thread local
   *                           object for thread
   *
   * If the object is registered with pthread library then we fetch it using
   * a library call; Otherwise we try to recycle one from the linked list
   * by atomically CAS into a nonoccupied local object in the linked list.
   * Finally if none above succeeds, we allocate a cache aligned chunk of
   * memory and then add it into the list and register with pthread
   */
  ThreadState *GetCurrentThreadState() {
    // 1. Try to obtain it as a registered per-thread object
    ThreadState *thread_state_p = static_cast<ThreadState *>(
      pthread_getspecific(thread_state_key));
    
    // If found then return - this should be the normal path
    if(thread_state_p != nullptr) {
      return thread_state_p;
    }

    // This is the head of the linked list
    thread_state_p = thread_state_head_p;
    while(thread_state_p != nullptr) {
      // Try to CAS a true value into the boolean
      // Note that the TAS will return false if successful so we take 
      // logical not
      bool ownership_acquired = !thread_state_p->owned.test_and_set();
      // If we succeeded and we successfully acquired an object, so just
      // register it and we are done
      if(ownership_acquired == true) {
        pthread_setspecific(thread_state_key, thread_state_p);
        return thread_state_p;
      }
    }

    // If we are here then the while loop exited without finding
    // an appropriate thread state object. So allocate one
    thread_state_p = CACHE_ALIGNED_ALLOC(sizeof(ThreadState));
  } 
};

/*
 * class RotateSkiplist - Main class of the skip list
 *
 * Note that for simplicity of coding, we contain all private classes used
 * by the main Skiplist class in the main class. We avoid a policy-based design
 * and do not templatize internal nodes in the skiplist as type parameters
 *
 * The skiplist is built based on partial ordering of keys, and therefore we 
 * need at least one functor which is the key comparator. To make key comparison
 * faster, a key equality checker is also required.
 */
template <typename _KeyType, 
          typename _ValueType,
          typename _KeyLess = std::less<_KeyType>,
          typename _KeyEq = std::equal_to<_KeyType>>
class RotateSkiplist {
 public:
  // Define member types to make it easier for external code to manipulate
  // types inside this class (if we just write it in the template argument
  // list then it is impossible for external code to obtain the actual
  // template arguments)
  using KeyType = _KeyType;
  using ValueType = _ValueType;
  using KeyLess = _KeyLess;
  using KeyEq = _KeyEq;
 
 private:
  
};

} // end of namespace rotate-skiplist

#endif