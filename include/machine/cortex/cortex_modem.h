// EPOS Quectel M95 GPRS NIC Mediator, commanded by Quectel AT commands

#include <system/config.h>
#if !defined(__m95_h) && defined(__modem__)
#define __m95_h

#include <machine.h>
#include <machine/uart.h>
#include <machine/gpio.h>
#include <network/ethernet.h>

__BEGIN_SYS

class M95: public NIC<Ethernet>
{
    template <typename Type, int unit> friend void call_init();

private:
    static const unsigned int UNITS = Traits<M95>::UNITS;

    // Interrupt dispatching binding
    struct Device {
        M95 * device;
    };

protected:
    M95(unsigned int unit);

public:
    typedef char RSSI;

    ~M95();

    void on();
    void off();

    Microsecond now();

    int send(const Address & dst, const Protocol & prot, const void * data, unsigned int size) {
        const char * cmd = reinterpret_cast<const char *>(data);
        if((cmd[0] == 'A') && (cmd[1] == 'T'))
            return send_command(cmd, size);
        else
            return send_data(cmd, size);
    }

    void reset() {
        off();
        Machine::delay(500000); // 500ms delay recommended by the manual
        on();
    }

    static M95 * get_by_unit(unsigned int unit) {
        assert(unit < UNITS);
        return _devices[unit].device;
    }

    RSSI rssi();

    static M95 * get(unsigned int unit = 0) { return get_by_unit(unit); }

    static void init(unsigned int unit);

    void check_timeout();

    int receive(Address * src, Protocol * prot, void * data, unsigned int size) { return 0; }
    Buffer * alloc(NIC * nic, const Address & dst, const Protocol & prot, unsigned int once, unsigned int always, unsigned int payload) { return 0; }
    void free(Buffer * buf) { return; }
    int send(Buffer * buf) { return 0; }
    const Address & address() { return _addr; }
    const Address broadcast() { return address(); }
    void address(const Address & address) { }
    unsigned int channel() { return 0; }
    void channel(unsigned int channel) { }
    unsigned int mtu() { return 0; }
    const Statistics & statistics() { return _statistics; }

private:
    int send_command(const char *command, unsigned int size);
    int send_data(const char * data, unsigned int size);
    bool wait_response(const char * expected, const Microsecond & timeout, char * response = 0, unsigned int response_length = 0);

    Address _addr;
    Statistics _statistics;
    unsigned int _unit;
    static Device _devices[UNITS];
    GPIO * _pwrkey;
    GPIO * _status;
    UART * _uart;
    bool _http_data_mode;
    TSC::Time_Stamp _last_send;
    TSC::Time_Stamp _init_timeout;
};

class Modem: public M95 {};

__END_SYS

#endif
