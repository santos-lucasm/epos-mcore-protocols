// EPOS ARMv8 PMU Mediator Declarations

#ifndef __armv8_pmu_h
#define __armv8_pmu_h

#include <architecture/cpu.h>
#include <architecture/pmu.h>

__BEGIN_SYS

class ARMv8_A_PMU: public PMU_Common
{
private:
    typedef CPU::Reg32 Reg32;

    static const unsigned int EVENTS = 54;

public:
    static const unsigned int CHANNELS = 6;
    static const unsigned int FIXED = 0;

    // Useful bits in the PMCR register
    enum {                      // Description                          Type    Value after reset
        PMCR_E = 1 << 0,        // Enable all counters                  r/w
        PMCR_P = 1 << 1,        // Reset event counters                 r/w
        PMCR_C = 1 << 2,        // Cycle counter reset                  r/w
        PMCR_D = 1 << 3,        // Enable cycle counter prescale (1/64) r/w
        PMCR_X = 1 << 4,        // Export events                        r/w
    };

    // Useful bits in the PMCNTENSET register
    enum {                      // Description                          Type    Value after reset
        PMCNTENSET_C = 1 << 31, // Cycle counter enable                 r/w
    };

    // Useful bits in the PMOVSR register
    enum {                      // Description                          Type    Value after reset
        PMOVSR_C = 1 << 31,     // Cycle counter overflow clear         r/w
    };

    // Predefined architectural performance events
    enum {
        // Event
        L1I_REFILL                            = 0x01,
        L1I_TLB_REFILL                        = 0x02,
        L1D_REFILL                            = 0x03,
        L1D_ACCESS                            = 0x04,
        L1D_TLB_REFILL                        = 0x05,
        INSTRUCTIONS_ARCHITECTURALLY_EXECUTED = 0x08,
        EXCEPTION_TAKEN                       = 0x09,
        BRANCHES_ARCHITECTURALLY_EXECUTED     = 0x0c,
        IMMEDIATE_BRANCH                      = 0X0d,
        UNALIGNED_LOAD_STORE                  = 0X0f,
        MISPREDICTED_BRANCH                   = 0x10,
        CYCLE                                 = 0x11,
        PREDICTABLE_BRANCH_EXECUTED           = 0x12,
        DATA_MEMORY_ACCESS                    = 0x13,
        L1I_ACCESS                            = 0x14,
        L1D_WRITEBACK                         = 0x15,
        L2D_ACCESS                            = 0x16,
        L2D_REFILL                            = 0x17,
        L2D_WRITEBACK                         = 0x18,
        BUS_ACCESS                            = 0x19,
        LOCAL_MEMORY_ERROR                    = 0x1a,
        INSTRUCTION_SPECULATIVELY_EXECUTED    = 0x1b,
        BUS_CYCLE                             = 0x1d,
        CHAIN                                 = 0x1e,
        // Cortex A-53 specific events
        BUS_ACCESS_LD                         = 0x60,
        BUS_ACCESS_ST                         = 0x61,
        BR_INDIRECT_SPEC                      = 0x7a,
        EXC_IRQ                               = 0x86,
        EXC_FIQ                               = 0x87,
        EXTERNAL_MEM_REQUEST                  = 0xc0,
        EXTERNAL_MEM_REQUEST_NON_CACHEABLE    = 0xc1,
        PREFETCH_LINEFILL                     = 0xc2,
        ICACHE_THROTTLE                       = 0xc3,
        ENTER_READ_ALLOC_MODE                 = 0xc4,
        READ_ALLOC_MODE                       = 0xc5,
        PRE_DECODE_ERROR                      = 0xc6,
        DATA_WRITE_STALL_ST_BUFFER_FULL       = 0xc7,
        SCU_SNOOPED_DATA_FROM_OTHER_CPU       = 0xc8,
        CONDITIONAL_BRANCH_EXECUTED           = 0xc9,
        IND_BR_MISP                           = 0xca,
        IND_BR_MISP_ADDRESS_MISCOMPARE        = 0xcb,
        CONDITIONAL_BRANCH_MISP               = 0xcc,
        L1_ICACHE_MEM_ERROR                   = 0xd0,
        L1_DCACHE_MEM_ERROR                   = 0xd1,
        TLB_MEM_ERROR                         = 0xd2,
        EMPTY_DPU_IQ_NOT_GUILTY               = 0xe0,
        EMPTY_DPU_IQ_ICACHE_MISS              = 0xe1,
        EMPTY_DPU_IQ_IMICRO_TLB_MISS          = 0xe2,
        EMPTY_DPU_IQ_PRE_DECODE_ERROR         = 0xe3,
        INTERLOCK_CYCLE_NOT_GUILTY            = 0xe4,
        INTERLOCK_CYCLE_LD_ST_WAIT_AGU_ADDRESS= 0xe5,
        INTERLOCK_CYCLE_ADV_SIMD_FP_INST      = 0xe6,
        INTERLOCK_CYCLE_WR_STAGE_STALL_BC_MISS= 0xe7,
        INTERLOCK_CYCLE_WR_STAGE_STALL_BC_STR = 0xe8
    };

public:
    ARMv8_A_PMU() {}

    static void config(const Channel & channel, const Event & event, const Flags & flags = NONE) {
        assert((static_cast<unsigned int>(channel) < CHANNELS) && (static_cast<unsigned int>(event) < EVENTS));
        db<PMU>(TRC) << "PMU::config(c=" << channel << ",e=" << event << ",f=" << flags << ")" << endl;
        pmselr(channel);
        pmxevtyper(_events[event]);
        start(channel);
    }

    static Count read(const Channel & channel) {
        db<PMU>(TRC) << "PMU::read(c=" << channel << ")" << endl;
        pmselr(channel);
        return pmxevcntr();
    }

    static void write(const Channel & channel, const Count & count) {
        db<PMU>(TRC) << "PMU::write(ch=" << channel << ",ct=" << count << ")" << endl;
        pmselr(channel);
        pmxevcntr(count);
    }

    static void start(const Channel & channel) {
        db<PMU>(TRC) << "PMU::start(c=" << channel << ")" << endl;
        pmcntenset(pmcntenset() | (1 << channel));
    }

    static void stop(const Channel & channel) {
        db<PMU>(TRC) << "PMU::stop(c=" << channel << ")" << endl;
        pmcntenclr(pmcntenclr() | (1 << channel));
    }

    static void reset(const Channel & channel) {
        db<PMU>(TRC) << "PMU::reset(c=" << channel << ")" << endl;
        write(channel, 0);
    }

    static void init();

private:
    static void pmcr(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c12, 0\n\t" : : "r"(reg)); }
    static Reg32 pmcr() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c12, 0\n\t" : "=r"(reg) : ); return reg; }

    static void pmcntenset(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c12, 1\n\t" : : "r"(reg)); }
    static Reg32 pmcntenset() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c12, 1\n\t" : "=r"(reg) : ); return reg; }

    static void pmcntenclr(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c12, 2\n\t" : : "r"(reg)); }
    static Reg32 pmcntenclr() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c12, 2\n\t" : "=r"(reg) : ); return reg; }

    static void pmovsr(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c12, 3\n\t" : : "r"(reg)); }
    static Reg32 pmovsr() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c12, 3\n\t" : "=r"(reg) : ); return reg; }

    static void pmselr(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c12, 5\n\t" : : "r"(reg)); }
    static Reg32 pmselr() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c12, 5\n\t" : "=r"(reg) : ); return reg; }

    static void pmxevtyper(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c13, 1\n\t" : : "r"(reg)); }
    static Reg32 pmxevtyper() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c13, 1\n\t" : "=r"(reg) : ); return reg; }

    static void pmxevcntr(Reg32 reg) { ASM("mcr p15, 0, %0, c9, c13, 2\n\t" : : "r"(reg)); }
    static Reg32 pmxevcntr() { Reg32 reg; ASM("mrc p15, 0, %0, c9, c13, 2\n\t" : "=r"(reg) : ); return reg; }

private:
    static const Reg32 _events[EVENTS] = {
                INSTRUCTIONS_ARCHITECTURALLY_EXECUTED,  // 0
        IMMEDIATE_BRANCH,                       // 1
        CYCLE,                                  // 2
        BRANCHES_ARCHITECTURALLY_EXECUTED,      // 3
        MISPREDICTED_BRANCH,                    // 4
        L1D_ACCESS,                             // 5
        L2D_ACCESS,                             // 6
        L1D_REFILL,                             // 7
        DATA_MEMORY_ACCESS,                     // 8 (LLC MISS)
        L1I_REFILL,                             // 9
        L1I_TLB_REFILL,                         // 10
        PREDICTABLE_BRANCH_EXECUTED,            // 11
        L1D_WRITEBACK,                          // 12
        L2D_WRITEBACK,                          // 13
        L2D_REFILL,                             // 14
        UNALIGNED_LOAD_STORE,                   // 15
        L1I_ACCESS,                             // 16
        L1D_TLB_REFILL,                         // 17
        EXCEPTION_TAKEN,                        // 18
        BUS_ACCESS,                             // 19
        LOCAL_MEMORY_ERROR,                     // 20
        INSTRUCTION_SPECULATIVELY_EXECUTED,     // 21
        BUS_CYCLE,                              // 22
        CHAIN,                                  // 23
        // ARM Cortex-A53 specific events (24-62 are Cortex-A9 events)
        BUS_ACCESS_LD,                          // 63
        BUS_ACCESS_ST,                          // 64
        BR_INDIRECT_SPEC,                       // 65
        EXC_IRQ,                                // 66
        EXC_FIQ,                                // 67
        EXTERNAL_MEM_REQUEST,                   // 68
        EXTERNAL_MEM_REQUEST_NON_CACHEABLE,     // 69
        PREFETCH_LINEFILL,                      // 70
        ICACHE_THROTTLE,                        // 71
        ENTER_READ_ALLOC_MODE,                  // 72
        READ_ALLOC_MODE,                        // 73
        PRE_DECODE_ERROR,                       // 74
        DATA_WRITE_STALL_ST_BUFFER_FULL,        // 75
        SCU_SNOOPED_DATA_FROM_OTHER_CPU,        // 76
        CONDITIONAL_BRANCH_EXECUTED,            // 77
        IND_BR_MISP,                            // 78
        IND_BR_MISP_ADDRESS_MISCOMPARE,         // 79
        CONDITIONAL_BRANCH_MISP,                // 80
        L1_ICACHE_MEM_ERROR,                    // 81
        L1_DCACHE_MEM_ERROR,                    // 82
        TLB_MEM_ERROR,                          // 83
        EMPTY_DPU_IQ_NOT_GUILTY,                // 84
        EMPTY_DPU_IQ_ICACHE_MISS,               // 85
        EMPTY_DPU_IQ_IMICRO_TLB_MISS,           // 86
        EMPTY_DPU_IQ_PRE_DECODE_ERROR,          // 87
        INTERLOCK_CYCLE_NOT_GUILTY,             // 88
        INTERLOCK_CYCLE_LD_ST_WAIT_AGU_ADDRESS, // 89
        INTERLOCK_CYCLE_ADV_SIMD_FP_INST,       // 90
        INTERLOCK_CYCLE_WR_STAGE_STALL_BC_MISS, // 91
        INTERLOCK_CYCLE_WR_STAGE_STALL_BC_STR   // 92
    }
};


class PMU: private IF<Traits<Build>::MACHINE == Traits<Build>::Cortex_A, ARMv8_A_PMU, ARMv8_A_PMU>::Result
{
    friend class CPU;

private:
    typedef IF<Traits<Build>::MACHINE == Traits<Build>::Cortex_A, ARMv8_A_PMU, ARMv8_A_PMU>::Result Engine;

public:
    using Engine::CHANNELS;
    using Engine::FIXED;
    using Engine::EVENTS;

    using Engine::Event;
    using Engine::Count;
    using Engine::Channel;

public:
    PMU() {}

    using Engine::config;
    using Engine::read;
    using Engine::write;
    using Engine::start;
    using Engine::stop;
    using Engine::reset;

private:
    static void init() { Engine::init(); }
};

__END_SYS

#endif
