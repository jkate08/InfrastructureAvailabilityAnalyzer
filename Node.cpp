#include "Node.h"
#include "Edge.h"

using namespace std;

// создаёт узел по id и объекту Point, координаты передаются в базовый класс Point
Node::Node(int id, Point position)
    : Point(position.getX(), position.getY()) {
    this->id = id;
}
// создаёт узел по id и координатам
Node::Node(int id, double x, double y)
    : Point(x, y) {
    this->id = id;
}

int Node::getId() {
    return id;
}

Point Node::getPosition() {
    return Point(getX(), getY());
}

void Node::addEdge(Edge* edge) {
    edges.push_back(edge);
}

vector<Edge*> Node::getEdges() {
    return edges;
}