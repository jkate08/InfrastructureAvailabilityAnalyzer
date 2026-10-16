#pragma once

class Node;

// описывает ребро графа
// ребро соединяет два узла: from и to
// distance хранит длину участка дороги в метрах
class Edge {
private:
    Node* from;
    Node* to;
    double distance;

public:
    Edge(Node* from, Node* to, double distance);

    Node* getFrom();
    Node* getTo();

    double getDistance();

    double getTime(double speed);
};