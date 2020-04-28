#ifndef __traits_h
#define __traits_h

#include <system/config.h>

__BEGIN_SYS

// Build
template<> struct Traits<Build>: public Traits<void>
{
    static const unsigned int MODE = LIBRARY;
    static const unsigned int ARCHITECTURE = ARMv7;
    static const unsigned int MACHINE = Cortex;
    static const unsigned int MODEL = Realview_PBX;
    static const unsigned int CPUS = 4;
    static const unsigned int NODES = 1;     // (> 1 => NETWORKING)
    static const unsigned int EXPECTED_SIMULATION_TIME = 300;    // s (0 => not simulated)
};


// Utilities
template<> struct Traits<Debug>: public Traits<void>
{
    static const bool error   = true;
    static const bool warning = true;
    static const bool info    = false;
    static const bool trace   = false;
};

template<> struct Traits<Lists>: public Traits<void>
{
    static const bool debugged = hysterically_debugged;
};

template<> struct Traits<Spin>: public Traits<void>
{
    static const bool debugged = hysterically_debugged;
};

template<> struct Traits<Heaps>: public Traits<void>
{
    static const bool debugged = hysterically_debugged;
};

template<> struct Traits<Observers>: public Traits<void>
{
    // Some observed objects are created before initializing the Display
    // Enabling debug may cause trouble in some Machines
    static const bool debugged = false;
};


// System Parts (mostly to fine control debugging)
template<> struct Traits<Boot>: public Traits<void>
{
};

template<> struct Traits<Setup>: public Traits<void>
{
};

template<> struct Traits<Init>: public Traits<void>
{
};

template<> struct Traits<Framework>: public Traits<void>
{
};

template<> struct Traits<Aspect>: public Traits<void>
{
    static const bool debugged = hysterically_debugged;
};


__END_SYS

// Mediators
#include __ARCHITECTURE_TRAITS_H
#include __MACHINE_TRAITS_H

__BEGIN_SYS


// API Components
template<> struct Traits<Application>: public Traits<void>
{
    static const unsigned int STACK_SIZE = Traits<Machine>::STACK_SIZE;
    static const unsigned int HEAP_SIZE = Traits<Machine>::HEAP_SIZE;
    static const unsigned int MAX_THREADS = Traits<Machine>::MAX_THREADS;
};

template<> struct Traits<System>: public Traits<void>
{
    static const unsigned int mode = Traits<Build>::MODE;
    static const bool multithread = (Traits<Build>::CPUS > 1) || (Traits<Application>::MAX_THREADS > 1);
    static const bool multitask = (mode != Traits<Build>::LIBRARY);
    static const bool multicore = (Traits<Build>::CPUS > 1) && multithread;
    static const bool multiheap = multitask || Traits<Scratchpad>::enabled;

    static const unsigned long LIFE_SPAN = 1 * YEAR; // s
    static const unsigned int DUTY_CYCLE = 1000000; // ppm

    static const bool reboot = true;

    static const unsigned int STACK_SIZE = Traits<Machine>::STACK_SIZE;
    static const unsigned int HEAP_SIZE = (Traits<Application>::MAX_THREADS + 1) * Traits<Application>::STACK_SIZE;
};

template<> struct Traits<Task>: public Traits<void>
{
    static const bool enabled = Traits<System>::multitask;
};

template<> struct Traits<Thread>: public Traits<void>
{
    static const bool enabled = Traits<System>::multithread;
    static const bool smp = Traits<System>::multicore;
    static const bool trace_idle = hysterically_debugged;
    static const bool simulate_capacity = false;

    typedef Scheduling_Criteria::GEDF Criterion;
    static const unsigned int QUANTUM = 10000; // us
};

template<> struct Traits<Scheduler<Thread>>: public Traits<void>
{
    static const bool debugged = Traits<Thread>::trace_idle || hysterically_debugged;
};

template<> struct Traits<Synchronizer>: public Traits<void>
{
    static const bool enabled = Traits<System>::multithread;
};

template<> struct Traits<Alarm>: public Traits<void>
{
    static const bool visible = hysterically_debugged;
};

//template<typename Transducer, typename Network>
template<> struct Traits<SmartData>: public Traits<void>
{
    static const unsigned char PREDICTOR = NONE;
};

template<> struct Traits<Monitor>: public Traits<void>
{
    static const bool enabled = monitored;

    // Monitoring frequencies (in Hz, aka samples per second)
    static const unsigned int MONITOR_ELAPSED_TIME      = 2;
    static const unsigned int MONITOR_DEADLINE_MISS     = 2;

    static const unsigned int MONITOR_CLOCK             = 0;
    static const unsigned int MONITOR_DVS_CLOCK         = 0;
    static const unsigned int MONITOR_INSTRUCTION       = 0;
    static const unsigned int MONITOR_BRANCH            = 0;
    static const unsigned int MONITOR_BRANCH_MISS       = 0;
    static const unsigned int MONITOR_L1_HIT            = 0;
    static const unsigned int MONITOR_L2_HIT            = 0;
    static const unsigned int MONITOR_L3_HIT            = 0;
    static const unsigned int MONITOR_LLC_HIT           = 0;
    static const unsigned int MONITOR_CACHE_HIT         = 0;
    static const unsigned int MONITOR_L1_MISS           = 0;
    static const unsigned int MONITOR_L2_MISS           = 0;
    static const unsigned int MONITOR_L3_MISS           = 0;
    static const unsigned int MONITOR_LLC_MISS          = 0;
    static const unsigned int MONITOR_CACHE_MISS        = 0;
    static const unsigned int MONITOR_LLC_HITM          = 0;

    static const unsigned int MONITOR_TEMPERATURE       = 0;
    static const unsigned int CPU_MONITOR_TEMPERATURE   = 0;
};

template<> struct Traits<Network>: public Traits<void>
{
    static const bool enabled = (Traits<Build>::NODES > 1);

    static const unsigned int RETRIES = 3;
    static const unsigned int TIMEOUT = 10; // s

    typedef LIST<> NETWORKS;
};

template<> struct Traits<ELP>: public Traits<Network>
{
    typedef Ethernet NIC_Family;

    static const bool enabled = NETWORKS::Count<ELP>::Result;
};

template<> struct Traits<TSTP>: public Traits<Network>
{
    typedef Ethernet NIC_Family;

    static const bool enabled = NETWORKS::Count<TSTP>::Result;

    static const unsigned int KEY_SIZE = 16;
    static const unsigned int RADIO_RANGE = 8000; // Approximated radio range in centimeters
};

template<> struct Traits<IP>: public Traits<Network>
{
    static const bool enabled = NETWORKS::Count<IP>::Result;

    struct Default_Config {
        static const unsigned int  TYPE    = DHCP;
        static const unsigned long ADDRESS = 0;
        static const unsigned long NETMASK = 0;
        static const unsigned long GATEWAY = 0;
    };

    template<unsigned int UNIT>
    struct Config: public Default_Config {};

    static const unsigned int TTL  = 0x40; // Time-to-live
};

template<> struct Traits<IP>::Config<0> //: public Traits<IP>::Default_Config
{
    static const unsigned int  TYPE      = MAC;
    static const unsigned long ADDRESS   = 0x0a000100;  // 10.0.1.x x=MAC[5]
    static const unsigned long NETMASK   = 0xffffff00;  // 255.255.255.0
    static const unsigned long GATEWAY   = 0;           // 10.0.1.1
};

template<> struct Traits<IP>::Config<1>: public Traits<IP>::Default_Config
{
};

template<> struct Traits<UDP>: public Traits<Network>
{
    static const bool checksum = true;
};

template<> struct Traits<TCP>: public Traits<Network>
{
    static const unsigned int WINDOW = 4096;
};

template<> struct Traits<DHCP>: public Traits<Network>
{
};

__END_SYS

#endif