#include <machine.h>
#include <process.h>
#include <network.h>
#include <smartdata.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "TSTP Sensor test" << endl;

    cout << "My machine ID is:";
    for(unsigned int i = 0; i < 8; i++)
        cout << " " << hex << Machine::uuid()[i];
    cout << endl;
    cout << "You can set this value at src/component/tstp_init.cc to set initial coordinates for this mote." << endl;

    cout << "My coordinates are " << TSTP::here() << endl;
    cout << "The time now is " << TSTP::now() << endl;
    cout << "I am" << (TSTP::here() == TSTP::sink() ? " " : " not ") << "the sink" << endl;

    Acceleration a(0, 1000000, Acceleration::ADVERTISED);

    Thread::self()->suspend();

    return 0;
}
