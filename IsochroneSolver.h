#pragma once

#include <map>
#include "Graph.h"

// структура для хранения результата расчёта
// для каждого узла хранится минимальное время достижения
struct IsochroneResult {
    std::map<Node*, double> times;
};

// расчёт зон доступности
class IsochroneSolver {
public:
    static IsochroneResult computeResult(
        Graph& graph,
        Node* start,
        double speed,
        double maxTime
    );
};