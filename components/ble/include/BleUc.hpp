#pragma once
#include "Uc.hpp"
//#include "BleRoutines.hpp"
#include <string>
#include "esp_log.h"

class BleUc : public Uc<BleUc> {
private:
    //BleRoutines bleRoutines;
    void handle(std::string value);

public:
    BleUc();
    ~BleUc();
    void taskLoop();

    static_assert(IUcConcept<BleUc, void*>);
};