#include "Edge.h"
#include "Node.h"

Edge::Edge(Node* from, Node* to, double distance) {
    this->from = from;
    this->to = to;
    this->distance = distance;
}

Node* Edge::getFrom() {
    return from;
}

Node* Edge::getTo() {
    return to;
}

double Edge::getDistance() {
    return distance;
}

double Edge::getTime(double speed) {
    if (speed <= 0) {
        return 0;
    }

    return distance / speed;
}