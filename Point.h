#pragma once

// класс хранит координаты точки
// x Ч долгота, y Ч широта
class Point {
private:
    double x;
    double y;

public:
    Point();
    Point(double x, double y);

    double getX();
    double getY();

    void setX(double x);
    void setY(double y);
};