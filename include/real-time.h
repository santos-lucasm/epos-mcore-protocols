// EPOS Real-time Declarations

#ifndef __real_time_h
#define __real_time_h

#include <utility/handler.h>
#include <utility/math.h>
#include <utility/convert.h>
#include <time.h>
#include <process.h>
#include <synchronizer.h>

__BEGIN_SYS

// Aperiodic Thread
typedef Thread Aperiodic_Thread;

// Periodic threads are achieved by programming an alarm handler to invoke
// p() on a control semaphore after each job (i.e. task activation). Base
// threads are created in BEGINNING state, so the scheduler won't dispatch
// them before the associate alarm and semaphore are created. The first job
// is dispatched by resume() (thus the _state = SUSPENDED statement)

// Periodic Thread
class Periodic_Thread: public Thread
{
public:
    enum {
        SAME        = Scheduling_Criteria::RT_Common::SAME,
        NOW         = Scheduling_Criteria::RT_Common::NOW,
        UNKNOWN     = Scheduling_Criteria::RT_Common::UNKNOWN,
        ANY         = Scheduling_Criteria::RT_Common::ANY
    };

protected:
    // Alarm Handler for periodic threads under static scheduling policies
    class Static_Handler: public Semaphore_Handler
    {
    public:
        Static_Handler(Semaphore * s, Periodic_Thread * t): Semaphore_Handler(s) {}
        ~Static_Handler() {}
    };

    // Alarm Handler for periodic threads under dynamic scheduling policies
    class Dynamic_Handler: public Semaphore_Handler
    {
    public:
        Dynamic_Handler(Semaphore * s, Periodic_Thread * t): Semaphore_Handler(s), _thread(t) {}
        ~Dynamic_Handler() {}

        void operator()() {
            _thread->criterion().update();

            Semaphore_Handler::operator()();
        }

    private:
        Periodic_Thread * _thread;
    };

    typedef IF<Criterion::dynamic, Dynamic_Handler, Static_Handler>::Result Handler;

public:
    struct Configuration: public Thread::Configuration {
        Configuration(const Microsecond & p, const Microsecond & d = SAME, const Microsecond & cap = UNKNOWN, const Microsecond & act = NOW, const unsigned int n = INFINITE, int cpu_id = ANY, const State & s = READY, const Criterion & c = NORMAL, const Color & a = WHITE, Task * t = 0, unsigned int ss = STACK_SIZE)
        : Thread::Configuration(s, c, a, t, ss), period(p), deadline(d == SAME ? p : d), capacity(cap), activation(act), times(n), cpu(cpu_id) {}

        Microsecond period;
        Microsecond deadline;
        Microsecond capacity;
        Microsecond activation;
        unsigned int times;
        int cpu;
    };

public:
    template<typename ... Tn>
    Periodic_Thread(const Microsecond & p, int (* entry)(Tn ...), Tn ... an)
    : Thread(Thread::Configuration(SUSPENDED, Criterion(p)), entry, an ...),
      _semaphore(0), _handler(&_semaphore, this), _alarm(p, &_handler, INFINITE) { resume(); }

    template<typename ... Tn>
    Periodic_Thread(const Configuration & conf, int (* entry)(Tn ...), Tn ... an)
    : Thread(Thread::Configuration(SUSPENDED, (conf.criterion != NORMAL) ? conf.criterion : Criterion(conf.period), conf.color, conf.task, conf.stack_size), entry, an ...),
      _semaphore(0), _handler(&_semaphore, this), _alarm(conf.period, &_handler, conf.times) {
        if(monitored) {
            if(INARRAY(Traits<Monitor>::SYSTEM_EVENTS, Traits<Monitor>::THREAD_EXECUTION_TIME) || INARRAY(Traits<Monitor>::SYSTEM_EVENTS, Traits<Monitor>::CPU_EXECUTION_TIME)) {
                TSC::Time_Stamp ts = TSC::time_stamp();
                if(_statistics.last_hyperperiod[_link.rank().queue()] == 0) {
                    _statistics.last_hyperperiod[_link.rank().queue()] = ts;
                    _statistics.hyperperiod[_link.rank().queue()] = Convert::us2count<TSC::Time_Stamp, Microsecond>(TSC::frequency(), conf.period);
                } else {
                    _statistics.hyperperiod[_link.rank().queue()] = Math::lcm(_statistics.hyperperiod[_link.rank().queue()], Convert::us2count<TSC::Time_Stamp, Microsecond>(TSC::frequency(),conf.period));
                }
                _statistics.last_execution = ts;
            }
            if(INARRAY(Traits<Monitor>::SYSTEM_EVENTS, Traits<Monitor>::DEADLINE_MISSES)) {
                _statistics.times_p_count = conf.times;
                _statistics.alarm_times = &_alarm;
            }
        }
        if((conf.state == READY) || (conf.state == RUNNING)) {
            _state = SUSPENDED;
            resume();
        } else
            _state = conf.state;
    }

    const Microsecond & period() const { return _alarm.period(); }
    void period(const Microsecond & p) { _alarm.period(p); }

    static volatile bool wait_next() {
        Periodic_Thread * t = reinterpret_cast<Periodic_Thread *>(running());

        if(monitored) {
            if(INARRAY(Traits<Monitor>::SYSTEM_EVENTS, Traits<Monitor>::THREAD_EXECUTION_TIME)) {
                TSC::Time_Stamp ts = TSC::time_stamp();
                t->_statistics.execution_time += ts - t->_statistics.last_execution;
                t->_statistics.last_execution = ts;
                t->_statistics.average_execution_time += t->_statistics.execution_time;
                t->_statistics.jobs++;
                t->_statistics.execution_time = 0;
            }

            if(INARRAY(Traits<Monitor>::SYSTEM_EVENTS, Traits<Monitor>::DEADLINE_MISSES))
                t->_statistics.missed_deadlines = t->_statistics.times_p_count - (t->_statistics.alarm_times->_times);
        }

        db<Thread>(TRC) << "Thread::wait_next(this=" << t << ",times=" << t->_alarm._times << ")" << endl;

        if(t->_alarm._times)
            t->_semaphore.p();

        return t->_alarm._times;
    }

protected:
    Semaphore _semaphore;
    Handler _handler;
    Alarm _alarm;
};

class RT_Thread: public Periodic_Thread
{
public:
    RT_Thread(void (* function)(), const Microsecond & deadline, const Microsecond & period = SAME, const Microsecond & capacity = UNKNOWN, const Microsecond & activation = NOW, int times = INFINITE, int cpu = ANY, const Color & color = WHITE, unsigned int stack_size = STACK_SIZE)
    : Periodic_Thread(Configuration(activation ? activation : period ? period : deadline, deadline, capacity, activation, activation ? 1 : times, cpu, SUSPENDED, Criterion(deadline, period ? period : deadline, capacity, cpu), color, 0, stack_size), &entry, this, function, activation, times) {
        if(activation && Criterion::dynamic)
            // The priority of dynamic criteria will be adjusted to the correct value by the
            // update() in the operator()() of Handler
            const_cast<Criterion &>(_link.rank())._priority = Criterion::PERIODIC;
        resume();
    }

private:
    static int entry(RT_Thread * t, void (*function)(), const Microsecond activation, int times) {
        if(activation) {
            // Wait for activation time
            t->_semaphore.p();

            // Adjust alarm's period
            t->_alarm.~Alarm();
            new (&t->_alarm) Alarm(t->criterion().period(), &t->_handler, times);
        }

        // Periodic execution loop
        do {
//            Alarm::Tick tick;
//            if(Traits<Periodic_Thread>::simulate_capacity && t->criterion()._capacity)
//                tick = Alarm::elapsed() + Alarm::ticks(t->criterion()._capacity);

            // Release job
            function();

//            if(Traits<Periodic_Thread>::simulate_capacity && t->criterion()._capacity)
//                while(Alarm::elapsed() < tick);
        } while (wait_next());

        return 0;
    }
};

typedef Periodic_Thread::Configuration RTConf;

__END_SYS

#endif
