#include "IUc.hpp"
#include "BleRoutines.hpp"

class BleUc : public Uc<BleUc> {
private:
    BleRoutines bleRoutines;

public:
    BleUc();
    ~BleUc();
    void taskLoop();
    void handle();

    static_assert(IUcConcept<BleUc, void*>);
};