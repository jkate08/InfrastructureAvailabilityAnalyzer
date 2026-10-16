#include "Graph.h"

using namespace std;

Graph::Graph() {
}

Graph::~Graph() {
    clear();
}

void Graph::addNode(Node* node) {
    nodes.push_back(node);
}

void Graph::addEdge(Edge* edge) {
    edges.push_back(edge);
}

vector<Node*> Graph::getNodes() {
    return nodes;
}

vector<Edge*> Graph::getEdges() {
    return edges;
}

void Graph::clear() {
    for (int i = 0; i < edges.size(); i++) {
        delete edges[i];
    }

    for (int i = 0; i < nodes.size(); i++) {
        delete nodes[i];
    }

    edges.clear();
    nodes.clear();
}