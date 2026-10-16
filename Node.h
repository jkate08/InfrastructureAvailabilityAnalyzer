#pragma once

#include <vector>
#include "Point.h"

class Edge;

// класс Node Ч узел дорожного графа
class Node : public Point {
private:
    int id;
    std::vector<Edge*> edges;

public:
    Node(int id, Point position);
    Node(int id, double x, double y);

    int getId();

    Point getPosition();

    void addEdge(Edge* edge);
    std::vector<Edge*> getEdges();
};