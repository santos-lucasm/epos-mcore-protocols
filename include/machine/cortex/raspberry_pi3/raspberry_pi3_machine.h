// EPOS Raspberry Pi3 (Cortex-A53) Mediator Declarations

#ifndef __raspberry_pi3_machine_h
#define __raspberry_pi3_machine_h

#include <machine/machine.h>
#include <system/memory_map.h>
#include <system.h>

__BEGIN_SYS

class Raspberry_Pi3: public Machine_Common
{
    friend Machine; // for pre_init() and init()

protected:
    typedef CPU::Reg32 Reg32;
    typedef CPU::Log_Addr Log_Addr;


public:
    Raspberry_Pi3() {}

    static void delay(const Microsecond & time);

    static void reboot();
    static void poweroff() { reboot(); }

    static const UUID & uuid() { return System::info()->bm.uuid; }

public:
    static void smp_barrier_init(unsigned int n_cpus) {
        _cores = n_cpus;
    }

private:
    static void pre_init();
    static void init() {}

private:
    static volatile unsigned int _cores;
};

typedef Raspberry_Pi3 Machine_Model;

__END_SYS

#endif
