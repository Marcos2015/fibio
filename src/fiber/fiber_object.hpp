//
//  fiber_object.hpp
//  fibio
//
//  Created by Chen Xu on 14-3-4.
//  Copyright (c) 2014 0d0a.com. All rights reserved.
//

#ifndef fibio_fiber_object_hpp
#define fibio_fiber_object_hpp

#define BOOST_COROUTINES_UNIDIRECT
#include <memory>
#include <chrono>
#include <deque>
#include <atomic>
#include <map>
#include <exception>
#include <boost/assert.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <boost/coroutine/coroutine.hpp>
#include <fibio/fibers/exceptions.hpp>
#include <fibio/fibers/detail/fiber_base.hpp>
#include <fibio/fibers/detail/fiber_data.hpp>
#include <fibio/fibers/detail/spinlock.hpp>

#if defined(DEBUG) && !defined(NDEBUG)
#   define CHECK_CALLER(f) do { if (!f->caller_) BOOST_ASSERT(false); } while(0)
#else
#   define CHECK_CALLER(f)
#endif

#if defined(DEBUG) && !defined(NDEBUG)
#   define CHECK_CURRENT_FIBER BOOST_ASSERT(::fibio::fibers::detail::fiber_object::current_fiber_)
#else
#   define CHECK_CURRENT_FIBER
#endif

namespace fibio { namespace fibers { namespace detail {
    typedef boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer_t;
    
    struct scheduler_object;
    typedef std::shared_ptr<scheduler_object> scheduler_ptr_t;
    
    struct fiber_object;
    typedef std::shared_ptr<fiber_object> fiber_ptr_t;
    
    struct mutex_object;
    struct timed_mutex_object;
    struct recursive_mutex_object;
    struct recursive_timed_mutex_object;
    struct condition_variable_object;
    
    typedef std::deque<fiber_ptr_t> waiting_queue_t;
    
    struct fss_cleanup_function;
    typedef const void *fss_key_t;
    typedef std::pair<std::shared_ptr<fss_cleanup_function>,void*> fss_value_t;
    
    typedef std::map<fss_key_t, fss_value_t> fss_map_t;
    
    struct fiber_object : std::enable_shared_from_this<fiber_object>, fiber_base {
        enum state_t {
            READY,
            RUNNING,
            BLOCKED,
            STOPPED,
        };
        
        typedef std::deque<std::function<void()>> cleanup_queue_t;
        typedef boost::coroutines::coroutine<state_t>::pull_type runner_t;
        typedef boost::coroutines::coroutine<state_t>::push_type caller_t;
        typedef std::shared_ptr<boost::asio::strand> strand_ptr_t;
        
        fiber_object(scheduler_ptr_t sched, fiber_data_base *entry);
        fiber_object(scheduler_ptr_t sched, strand_ptr_t strand, fiber_data_base *entry);
        ~fiber_object();
        
        void set_name(const std::string &s);
        std::string get_name();
        
        void raw_set_state(state_t s)
        { state_=s; }
        
        void set_state(state_t s) {
            if (caller_) {
                // We're in fiber context, switch to scheduler context to make state take effect
                //(*caller_)(std::bind(&fiber_object::raw_set_state, shared_from_this(), s));
                (*caller_)(s);
            } else {
                // We're in scheduler context
                state_=s;
            }
        }

        virtual void pause() override;
        virtual void activate() override;
        virtual void resume() override;
        virtual boost::asio::strand &get_fiber_strand() override;
        
        // Following functions can only be called inside coroutine
        void yield();
        void join(fiber_ptr_t f);
        void join_and_rethrow(fiber_ptr_t f);
        void sleep_usec(uint64_t usec);
        
        // Implementations
        void runner_wrapper(caller_t &c);
        void one_step();
        
        void detach();
        
        void add_cleanup_function(std::function<void()> &&f);
        
        static __thread fiber_object *current_fiber_;

        scheduler_ptr_t sched_;
        strand_ptr_t fiber_strand_;
	spinlock mtx_;
        std::atomic<state_t> state_;
        std::unique_ptr<fiber_data_base> entry_;
        runner_t runner_;
        caller_t *caller_;
        cleanup_queue_t cleanup_queue_;
        fss_map_t fss_;
        fiber_ptr_t this_ref_;
        std::string name_;
        std::exception_ptr uncaught_exception_;
    };
    
    template<typename Lockable>
    struct relock_guard {
        inline relock_guard(Lockable &mtx)
        : mtx_(mtx)
        { mtx_.unlock(); }
        
        inline ~relock_guard()
        { mtx_.lock(); }
        
        Lockable &mtx_;
    };
}}} // End of namespace fibio::fibers::detail

#endif
