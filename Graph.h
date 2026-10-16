#pragma once

#include <vector>
#include "Node.h"
#include "Edge.h"

// хранит весь дорожный граф
class Graph {
private:
    std::vector<Node*> nodes;
    std::vector<Edge*> edges;

public:
    Graph();
    ~Graph();

    void addNode(Node* node);
    void addEdge(Edge* edge);

    std::vector<Node*> getNodes();
    std::vector<Edge*> getEdges();

    void clear();
};