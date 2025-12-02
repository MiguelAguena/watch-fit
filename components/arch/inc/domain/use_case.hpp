#pragma once


namespace domain {

    class IRootUseCase {
    public:
        virtual ~IRootUseCase() = default;
        virtual void run() = 0;
    };
};