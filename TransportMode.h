#pragma once

#include <string>

// базовый класс режима движения.
class TransportMode {
public:
    virtual ~TransportMode();

    virtual double getSpeed() = 0;

    virtual std::wstring getName() = 0;
};

class WalkMode : public TransportMode {
public:
    double getSpeed();
    std::wstring getName();
};

class CarMode : public TransportMode {
public:
    double getSpeed();
    std::wstring getName();
};