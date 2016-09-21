/**
 * @file algorithm.h
 * @author Chase Geigle
 *
 * All files in META are dual-licensed under the MIT and NCSA licenses. For more
 * details, consult the file LICENSE.mit and LICENSE.ncsa in the root of the
 * project.
 */

#ifndef META_PARALLEL_ALGORITHM_H_
#define META_PARALLEL_ALGORITHM_H_

#include "meta/config.h"
#include "meta/parallel/thread_pool.h"
#include "meta/parallel/parallel_for.h"

namespace meta
{
namespace parallel
{

/**
 * Performs a reduction across a set of mapped values in parallel. This
 * algorithm has three distinct phases:
 *
 * 1. Initialization: each thread invokes the LocalStorage functor, which
 *    should return the local storage needed to perform the reduction
 *    across the set of values that will be assigned to a particular
 *    thread. This is done *within* the thread to ensure that memory
 *    allocations occur within the worker thread (so it can take advantage
 *    of thread-local heap structures in, for example, jemalloc).
 *
 * 2. Mapping: each thread invokes the MappingFunction functor, which is a
 *    *binary* operator that takes a mutable reference to the thread's
 *    local storage that was created using the LocalStorage functor as its
 *    first argument and the element in the iterator range (by const ref)
 *    as its second argument. It is *not* expected to return anything as
 *    the calculation results should be being placed in the thread's local
 *    storage.
 *
 * 3. Reduction: finally, the main thread will compute the final value of
 *    the reduction by applying ReductionFunction across the local storage
 *    for each of the threads. ReductionFunction is a *binary* functor that
 *    takes the return type of LocalStorage by *mutable reference* as the
 *    first argument and a *const reference* to an object of the same type
 *    as the second argument. It is *not* expected to return anything and
 *    instead should compute the reduction by modifying the first argument.
 */
template <class Iterator, class LocalStorage, class MappingFunction,
          class ReductionFunction>
typename std::result_of<LocalStorage()>::type
reduction(Iterator begin, Iterator end, thread_pool& pool, LocalStorage&& ls_fn,
          MappingFunction&& map_fn, ReductionFunction&& red_fn)
{
    using value_type = typename std::iterator_traits<Iterator>::value_type;

    auto futures = for_each_block(begin, end, pool,
            [&](Iterator tbegin, Iterator tend)
            {
                auto local_storage = ls_fn();
                std::for_each(tbegin, tend, [&](const value_type& val) {
                    map_fn(local_storage, val);
                });
                return local_storage;
            });

    // reduction phase
    auto local_storage = futures[0].get();
    for (auto it = ++futures.begin(); it != futures.end(); ++it)
    {
        red_fn(local_storage, it->get());
    }
    return local_storage;
}

template <class Iterator, class LocalStorage, class MappingFunction,
          class ReductionFunction>
typename std::result_of<LocalStorage()>::type
reduction(Iterator begin, Iterator end, LocalStorage&& ls_fn,
          MappingFunction&& map_fn, ReductionFunction&& red_fn)
{
    parallel::thread_pool pool;
    return reduction(begin, end, pool, ls_fn, map_fn, red_fn);
}
}
}
#endif