

#pragma once


namespace driven_ports::time {

    class ITimePort {
    public:
        virtual ~ITimePort() = default;

        virtual void sleep_for(unsigned int milliseconds) = 0;
    };

};