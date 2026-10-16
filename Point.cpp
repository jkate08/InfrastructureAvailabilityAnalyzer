#include "Point.h"

// если координаты не переданы, точка создаётся в координатах 0, 0
Point::Point() {
    x = 0;
    y = 0;
}
// конструктор с координатами
Point::Point(double x, double y) {
    this->x = x;
    this->y = y;
}
// возвращает координату x (используется как долгота)
double Point::getX() {
    return x;
}
// возвращает координату y (используется как широта)
double Point::getY() {
    return y;
}
void Point::setX(double x) {
    this->x = x;
}
void Point::setY(double y) {
    this->y = y;
}