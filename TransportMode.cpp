#include "TransportMode.h"

using namespace std;

TransportMode::~TransportMode() {
}

double WalkMode::getSpeed() {
    return 1.4;
}
wstring WalkMode::getName() {
    return L"пешком";
}
double CarMode::getSpeed() {
    return 11.1;
}
wstring CarMode::getName() {
    return L"на машине";
}