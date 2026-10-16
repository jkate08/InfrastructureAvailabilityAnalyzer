#include "IsochroneSolver.h"

#include <limits>
#include <vector>

using namespace std;

// элемент очереди (в нём хранится время достижения узла и узел)
struct QueueItem {
    double time;
    Node* node;

    QueueItem() {
        time = 0;
        node = nullptr;
    }

    QueueItem(double time, Node* node) {
        this->time = time;
        this->node = node;
    }
};

// приоритетная очередь (элемент с самым маленьким временем всегда находится наверху)
class MyPriorityQueue {
private:
    vector<QueueItem> heap;

    // меняет два элемента местами.
    void swapItems(int i, int j) {
        QueueItem temp = heap[i];
        heap[i] = heap[j];
        heap[j] = temp;
    }

    // перемещает элемент ближе к началу очереди, 
    // если его время меньше времени родительского элемента
    void siftUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;

            if (heap[parent].time <= heap[index].time) {
                break;
            }

            swapItems(parent, index);
            index = parent;
        }
    }

    // опускает элемент ниже в куче,
    // если у одного из дочерних элементов время достижения меньше
    void siftDown(int index) {
        int size = (int)heap.size();

        while (true) {
            int left = index * 2 + 1;
            int right = index * 2 + 2;
            int smallest = index;

            if (left < size && heap[left].time < heap[smallest].time) {
                smallest = left;
            }

            if (right < size && heap[right].time < heap[smallest].time) {
                smallest = right;
            }

            if (smallest == index) {
                break;
            }

            swapItems(index, smallest);
            index = smallest;
        }
    }

public:
    // проверяет, пустая ли очередь
    bool empty() {
        return heap.empty();
    }

    // добавляет новый элемент в очередь
    void push(QueueItem item) {
        heap.push_back(item);
        siftUp((int)heap.size() - 1);
    }

    // возвращает элемент с минимальным временем
    QueueItem top() {
        if (heap.empty()) {
            return QueueItem();
        }

        return heap[0];
    }

    // удаляет элемент с минимальным временем
    void pop() {
        if (heap.empty()) {
            return;
        }

        heap[0] = heap[(int)heap.size() - 1];
        heap.pop_back();

        if (!heap.empty()) {
            siftDown(0);
        }
    }
};

// метод считает минимальное время достижения всех узлов графа
// от стартового узла start
IsochroneResult IsochroneSolver::computeResult(
    Graph& graph,
    Node* start,
    double speed,
    double maxTime
) {
    IsochroneResult result;

    vector<Node*> nodes = graph.getNodes();

    // сначала считаем, что все узлы недостижимы
    for (int i = 0; i < nodes.size(); i++) {
        result.times[nodes[i]] = numeric_limits<double>::infinity();
    }

    // если стартовый узел не найден, возвращаем результат
    if (start == nullptr) {
        return result;
    }

    // время достижения стартового узла равно 0
    result.times[start] = 0;

    // используем свою приоритетную очередь
    MyPriorityQueue q;

    // добавляем стартовый узел в очередь
    q.push(QueueItem(0, start));

    // пока очередь не пустая, продолжаем обработку
    while (!q.empty()) {
        QueueItem item = q.top();
        q.pop();

        double currentTime = item.time;
        Node* currentNode = item.node;

        if (currentNode == nullptr) {
            continue;
        }

        // если время больше максимального предела, пропускаем узел
        if (currentTime > maxTime) {
            continue;
        }

        // если в таблице уже есть более короткое время,
        // этот вариант можно не рассматривать
        if (currentTime > result.times[currentNode]) {
            continue;
        }

        vector<Edge*> edges = currentNode->getEdges();

        // проверяем все соседние узлы
        for (int i = 0; i < edges.size(); i++) {
            Edge* edge = edges[i];
            Node* nextNode = edge->getTo();

            if (nextNode == nullptr) {
                continue;
            }

            // считаем новое время до соседнего узла
            double newTime = currentTime + edge->getTime(speed);

            // если нашли более быстрый путь, обновляем время
            // и добавляем узел в очередь
            if (newTime < result.times[nextNode] && newTime <= maxTime) {
                result.times[nextNode] = newTime;
                q.push(QueueItem(newTime, nextNode));
            }
        }
    }

    return result;
}