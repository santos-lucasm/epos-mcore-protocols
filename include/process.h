// EPOS Thread Component Declarations

#ifndef __process_h
#define __process_h

#include <architecture.h>
#include <utility/queue.h>
#include <utility/handler.h>
#include <utility/scheduler.h>
#include <memory.h>

extern "C" { void __exit(); }

__BEGIN_SYS

// Thread
class Thread
{
    friend class Init_First;            // context->load()
    friend class Init_System;           // for init() on CPU != 0
    friend class Scheduler<Thread>;     // for link()
    friend class Synchronizer_Base;   // for lock() and sleep()
    friend class Synchronizer_Common<true>;   // for lock() and sleep()
    friend class Synchronizer_Common<false>;   // for lock() and sleep()
    friend class Synchronizer_Common_Test;
    friend class Alarm;                 // for lock()
    friend class System;                // for init()
    friend class IC;                    // for link() for priority ceiling
    friend class Clerk<System>;         // for _statistics

protected:
    static const bool smp = Traits<Thread>::smp;
    static const bool monitored = Traits<Thread>::monitored;
    static const bool preemptive = Traits<Thread>::Criterion::preemptive;
    static const bool multitask = Traits<System>::multitask;
    static const bool reboot = Traits<System>::reboot;

    static const unsigned int QUANTUM = Traits<Thread>::QUANTUM;
    static const unsigned int STACK_SIZE = multitask ? Traits<System>::STACK_SIZE : Traits<Application>::STACK_SIZE;
    static const unsigned int USER_STACK_SIZE = Traits<Application>::STACK_SIZE;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Context Context;

public:
    // Thread State
    enum State {
        RUNNING,
        READY,
        SUSPENDED,
        WAITING,
        FINISHING
    };

    // Thread Priority
    typedef Scheduling_Criteria::Priority Priority;

    // Thread Scheduling Criterion
    typedef Traits<Thread>::Criterion Criterion;
    enum {
        ISR     = Criterion::ISR,
        HIGH    = Criterion::HIGH,
        NORMAL  = Criterion::NORMAL,
        LOW     = Criterion::LOW,
        MAIN    = Criterion::MAIN,
        IDLE    = Criterion::IDLE
    };

    typedef Queue<Thread, Scheduler<Thread>::Element> FIFO_Queue;
    // Thread Queue
    typedef Ordered_Queue<Thread, Criterion, Scheduler<Thread>::Element> Thread_Queue;

    // Thread Configuration
    // t = 0 => Task::self()
    // ss = 0 => user-level stack on an auto expand segment
    struct Configuration {
        Configuration(const State & s = READY, const Criterion & c = NORMAL, const Color & a = WHITE, Task * t = 0, unsigned int ss = STACK_SIZE)
        : state(s), criterion(c), color(a), task(t), stack_size(ss) {}

        State state;
        Criterion criterion;
        Color color;
        Task * task;
        unsigned int stack_size;
    };

    // Thread Statistics (mostly for Monitor)
    struct _Statistics {
        _Statistics(): execution_time(0), last_execution(0), jobs(0), average_execution_time(0), hyperperiod_count_thread(0), hyperperiod_jobs(0), hyperperiod_average_execution_time(0), alarm_times(0), times_p_count(0), missed_deadlines(0) {}

        // Thread Execution Time
        unsigned int execution_time;
        unsigned int last_execution;
        unsigned int jobs;
        unsigned int average_execution_time;
        unsigned int hyperperiod_count_thread;
        unsigned int hyperperiod_jobs;
        unsigned int hyperperiod_average_execution_time;

        // Dealine Miss count
        Alarm * alarm_times;
        unsigned int times_p_count;
        unsigned int missed_deadlines;

        // On Migration
        static unsigned int hyperperiod[Traits<Build>::CPUS];              // recalculate, on _old_hyperperiod + hyperperiod update
        static TSC::Time_Stamp last_hyperperiod[Traits<Build>::CPUS];      // wait for old hyperperiod and update
        static unsigned int hyperperiod_count[Traits<Build>::CPUS];        // reset on next hyperperiod

        // CPU Execution Time
        static TSC::Time_Stamp hyperperiod_idle_time[Traits<Build>::CPUS]; //
        static TSC::Time_Stamp idle_time[Traits<Build>::CPUS];
        static TSC::Time_Stamp last_idle[Traits<Build>::CPUS];
    };

    union _Dummy_Statistics {
        // Thread Execution Time
        unsigned int execution_time;
        unsigned int last_execution;
        unsigned int jobs;
        unsigned int average_execution_time;
        unsigned int hyperperiod_count_thread;
        unsigned int hyperperiod_jobs;
        unsigned int hyperperiod_average_execution_time;

        // Dealine Miss count
        Alarm * alarm_times;
        unsigned int times_p_count;
        unsigned int missed_deadlines;

        // On Migration
        static unsigned int hyperperiod[Traits<Build>::CPUS];              // recalculate, on _old_hyperperiod + hyperperiod update
        static TSC::Time_Stamp last_hyperperiod[Traits<Build>::CPUS];      // wait for old hyperperiod and update
        static unsigned int hyperperiod_count[Traits<Build>::CPUS];        // reset on next hyperperiod

        // CPU Execution Time
        static TSC::Time_Stamp hyperperiod_idle_time[Traits<Build>::CPUS]; //
        static TSC::Time_Stamp idle_time[Traits<Build>::CPUS];
        static TSC::Time_Stamp last_idle[Traits<Build>::CPUS];
    };
    typedef IF<monitored, _Statistics, _Dummy_Statistics>::Result Statistics;

public:
    template<typename ... Tn>
    Thread(int (* entry)(Tn ...), Tn ... an);
    template<typename ... Tn>
    Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an);
    ~Thread();

    const volatile State & state() const { return _state; }
    const volatile Statistics & statistics() const { return _statistics; }

    const volatile Criterion & priority() const { return _link.rank(); }
    void priority(const Criterion & p);

    int preemptLevel(){ return criterion().preempt_level; }
    void preemptLevel(int p){ criterion().preempt_level = p; }

    void setPriority( int p )
    {
        lock();
        criterion().setPriority( p );
        unlock();
    }

    Task * task() const { return _task; }

    int join();
    void pass();
    void suspend() { suspend(false); }
    void resume();

    static Thread * volatile self() { return running(); }
    static void yield();
    static void exit(int status = 0);

    Criterion & criterion() { return const_cast<Criterion &>(_link.rank()); }

protected:
    void constructor_prologue(const Color & color, unsigned int stack_size);
    void constructor_epilogue(const Log_Addr & entry, unsigned int stack_size);

    Thread_Queue::Element * link() { return &_link; }

    void suspend(bool locked);

    Criterion begin_isr(IC::Interrupt_Id i) {
        assert(_state == RUNNING);
        Criterion c = criterion();
        _link.rank(Criterion::ISR + int(i));
        return c;
    }
    void end_isr(IC::Interrupt_Id i, const Criterion & c) {
        assert(_state == RUNNING);
        _link.rank(c);
    }

    static Thread * volatile running() { return _scheduler.chosen(); }

    static void lock() {
        CPU::int_disable();
        if(smp)
            _lock.acquire();
    }

    static void unlock() {
        if(smp)
            _lock.release();
        CPU::int_enable();
    }

    static volatile bool locked() { return (smp) ? _lock.taken() : CPU::int_disabled(); }

    static void sleep(Thread_Queue * q);
    static void wakeup(Thread_Queue * q);
    static void wakeup_all(Thread_Queue * q);

    static void sleep(FIFO_Queue * q);
    static void wakeup(FIFO_Queue * q);
    static void wakeup_all(FIFO_Queue * q);

    static void reschedule();
    static void reschedule(unsigned int cpu);
    static void rescheduler(IC::Interrupt_Id interrupt);
    static void time_slicer(IC::Interrupt_Id interrupt);

    static void dispatch(Thread * prev, Thread * next, bool charge = true);

    static int idle();

private:
    static void init();

protected:
    Task * _task;
    Segment * _user_stack;

    char * _stack;
    Context * volatile _context;
    volatile State _state;
    Thread_Queue * _waiting;
    FIFO_Queue * _waiting_fifo;
    Thread * volatile _joining;
    Thread_Queue::Element _link;

    Statistics _statistics;

    static volatile unsigned int _thread_count;
    static Scheduler_Timer * _timer;
    static Scheduler<Thread> _scheduler;
    static Spin _lock;
};


// Task (only used in multitasking configurations)
class Task
{
    friend class Init_First;
    friend class System;
    friend class Thread;

private:
    static const bool multitask = Traits<System>::multitask;

    typedef CPU::Log_Addr Log_Addr;
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Context Context;
    typedef Thread::Thread_Queue Queue;

protected:
    // This constructor is only used by Init_First
    template<typename ... Tn>
    Task(Address_Space * as, Segment * cs, Segment * ds, int (* entry)(Tn ...), const Log_Addr & code, const Log_Addr & data, Tn ... an)
    : _as(as), _cs(cs), _ds(ds), _entry(entry), _code(code), _data(data) {
        db<Task, Init>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _current = this;
        activate();
        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN, WHITE, this, 0), entry, an ...);
    }

public:
    template<typename ... Tn>
    Task(Segment * cs, Segment * ds, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _entry(entry), _code(_as->attach(_cs)), _data(_as->attach(_ds)) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _main = new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::MAIN, WHITE, this, 0), entry, an ...);
    }
    template<typename ... Tn>
    Task(const Thread::Configuration & conf, Segment * cs, Segment * ds, int (* entry)(Tn ...), Tn ... an)
    : _as (new (SYSTEM) Address_Space), _cs(cs), _ds(ds), _entry(entry), _code(_as->attach(_cs)), _data(_as->attach(_ds)) {
        db<Task>(TRC) << "Task(as=" << _as << ",cs=" << _cs << ",ds=" << _ds << ",entry=" << _entry << ",code=" << _code << ",data=" << _data << ") => " << this << endl;

        _main = new (SYSTEM) Thread(Thread::Configuration(conf.state, conf.criterion, this, 0), entry, an ...);
    }
    ~Task();

    Address_Space * address_space() const { return _as; }

    Segment * code_segment() const { return _cs; }
    Segment * data_segment() const { return _ds; }

    Log_Addr code() const { return _code; }
    Log_Addr data() const { return _data; }

    Thread * main() const { return _main; }

    static Task * volatile self() { return current(); }

private:
    void activate() const { _as->activate(); }

    void insert(Thread * t) { _threads.insert(new (SYSTEM) Queue::Element(t)); }
    void remove(Thread * t) { Queue::Element * el = _threads.remove(t); if(el) delete el; }

    static Task * volatile current() { return _current; }
    static void current(Task * t) { _current = t; }

    static void init();

private:
    Address_Space * _as;
    Segment * _cs;
    Segment * _ds;
    Log_Addr _entry;
    Log_Addr _code;
    Log_Addr _data;
    Thread * _main;
    Queue _threads;

    static Task * volatile _current;
};


// A Java-like Active Object
class Active: public Thread
{
public:
    Active(): Thread(Configuration(Thread::SUSPENDED), &entry, this) {}
    virtual ~Active() {}

    virtual int run() = 0;

    void start() { resume(); }

private:
    static int entry(Active * runnable) { return runnable->run(); }
};


// An event handler that triggers a thread (see handler.h)
class Thread_Handler : public Handler
{
public:
    Thread_Handler(Thread * h) : _handler(h) {}
    ~Thread_Handler() {}

    void operator()() { _handler->resume(); }

private:
    Thread * _handler;
};


// Thread inline methods that depend on Task
template<typename ... Tn>
inline Thread::Thread(int (* entry)(Tn ...), Tn ... an)
: _task(Task::self()), _user_stack(0), _state(READY), _waiting(0), _waiting_fifo(0), _joining(0), _link(this, NORMAL)
{
    constructor_prologue(WHITE, STACK_SIZE);
    _context = CPU::init_stack(0, _stack + STACK_SIZE, &__exit, entry, an ...);
    constructor_epilogue(entry, STACK_SIZE);
}

template<typename ... Tn>
inline Thread::Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an)
: _task(conf.task ? conf.task : Task::self()), _state(conf.state), _waiting(0), _waiting_fifo(0), _joining(0), _link(this, conf.criterion)
{
    if(multitask && !conf.stack_size) { // Auto-expand, user-level stack
        constructor_prologue(conf.color, STACK_SIZE);
        _user_stack = new (SYSTEM) Segment(USER_STACK_SIZE);

        // Attach the thread's user-level stack to the current address space so we can initialize it
        Log_Addr ustack = Task::self()->address_space()->attach(_user_stack);

        // Initialize the thread's user-level stack and determine a relative stack pointer (usp) from the top of the stack
        Log_Addr usp = ustack + USER_STACK_SIZE;
        if(conf.criterion == MAIN)
            usp -= CPU::init_user_stack(usp, 0, an ...); // the main thread of each task must return to crt0 to call _fini (global destructors) before calling __exit
        else
            usp -= CPU::init_user_stack(usp, &__exit, an ...); // __exit will cause a Page Fault that must be properly handled

        // Attach the thread's user-level stack from the current address space
        Task::self()->address_space()->detach(_user_stack, ustack);

        // Attach the thread's user-level stack to its task's address space so it will be able to access it when it runs
        ustack = _task->address_space()->attach(_user_stack);

        // Determine an absolute stack pointer (usp) from the top of the thread's user-level stack considering the address it will see it when it runs
        usp = ustack + USER_STACK_SIZE - usp;

        // Initialize the thread's system-level stack
        _context = CPU::init_stack(usp, _stack + STACK_SIZE, &__exit, entry, an ...);
    } else {
        constructor_prologue(conf.color, conf.stack_size);
        _user_stack = 0;
        _context = CPU::init_stack(0, _stack + conf.stack_size, &__exit, entry, an ...);
    }

    constructor_epilogue(entry, STACK_SIZE);
}

__END_SYS

#endif
