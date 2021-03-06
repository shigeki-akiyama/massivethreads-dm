#ifndef MADI_ISO_STACK_EX_WORKER_H
#define MADI_ISO_STACK_EX_WORKER_H

#include "../madi.h"
#include "taskq.h"
#include "context.h"
#include "../future.h"
#include "../debug.h"
#include <deque>
#include <tuple>

namespace madi {

    template <class F, class... Args>
    struct start_params;

    struct steal_rep;

    struct thread_storage_holder {
        int debug;
        thread_storage_holder *parent;
        context *parent_ctx;
        bool stolen;
        void *value;
    public:
        explicit thread_storage_holder(thread_storage_holder *tls,
                                       context *ctx)
            : debug(423)
            , parent(tls)
            , parent_ctx(ctx)
            , stolen(false)
            , value(NULL) {}
        ~thread_storage_holder() = default;
    };

    template <class F, class... Args>
    inline void worker_start(void *arg0, void *arg1, void *arg2, void *arg3);
    template <class F, class... Args>
    inline void worker_do_start(context *ctx, void *arg0, void *arg1);

    class worker {
        template <class F, class... Args>
        friend void worker_do_fork(context *ctx_ptr,
                                   void *f_ptr, void *arg_ptr);

        template <class F, class... Args>
        friend void worker_do_suspend(context *ctx_ptr,
                                      void *f_ptr, void *arg_ptr);

        template <class F, class... Args>
        friend void worker_start(void *arg0, void *arg1, void *arg2,
                                 void *arg3, Args... args);
        template <class F, class... Args>
        friend void worker_do_start(context *ctx, void *arg0, void *arg1);

        friend void madi_worker_do_resume_remote(void *p0, void *p1, 
                                                 void *p2, void *p3);
        friend void resume_remote_context(saved_context *sctx, 
                                          std::tuple<taskq_entry *,
                                          uth_pid_t, taskque *, tsc_t> *arg);
        friend void resume_remote_context_by_messages(saved_context *sctx,
                                                      steal_rep *rep);

        void *wls_;
        thread_storage_holder *tls_;
        bool is_main_task_;
        size_t max_stack_usage_;
        uint8_t *stack_bottom_;

        taskque *taskq_;
        taskque **taskq_array_;
        taskq_entry **taskq_entries_array_;
        taskque *taskq_buf_;
        taskq_entry *taskq_entry_buf_;

        future_pool fpool_;

        context *main_ctx_;
        std::deque<saved_context *> waitq_;
        
        bool done_;

    public:
        worker();
        ~worker();

        worker(const worker& sched);
        const worker& operator=(const worker& sched);

        void initialize(uth_comm& c);
        void finalize(uth_comm& c);

        template <class F, class... Args>
        void start(F f, int argc, char **argv, Args... args);

        void notify_done();

        template <class F, class... Args>
        void fork(F f, Args... args);

        void exit();

        template <class F, class... Args>
        void suspend(F f, Args... args);

        void resume(saved_context *next_sctx);

        void do_scheduler_work();

        future_pool& fpool() { return fpool_; }
        taskque& taskq() { return *taskq_; }
        std::deque<saved_context *>& waitq() { return waitq_; }

        size_t max_stack_usage() const { return max_stack_usage_; }

        template <class T> void set_thread_local(T *value)
        { MADI_ASSERT(tls_ != NULL);
          tls_->value = reinterpret_cast<void *>(value); }

        template <class T> T * get_thread_local()
        { MADI_ASSERT(tls_ != NULL); MADI_ASSERT(tls_->debug == 423);
          return reinterpret_cast<T *>(tls_->value); }

        template <class T> void set_worker_local(T *value)
        { wls_= reinterpret_cast<void *>(value); }

        template <class T> T * get_worker_local()
        { return reinterpret_cast<T *>(wls_); }

    private:
        void go();
        static void do_resume(worker& w, const taskq_entry& entry,
                              uth_pid_t victim);
        bool steal_with_lock(taskq_entry *entry,
                             uth_pid_t *victim,
                             taskque **taskq);
        bool steal();
        bool steal_by_rdmas();
        bool steal_by_messages();
        bool is_main_task();
    };
    
}

#endif
