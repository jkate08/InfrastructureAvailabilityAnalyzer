#pragma once

#include <string>
#include <map>
#include <utility>

#include <nlohmann/json.hpp>

#include "Graph.h"
#include "Point.h"
#include "Node.h"
#include "Edge.h"

// класс OSMReader читает данные OpenStreetMap из файла export.json
// строит дорожный граф
class OSMReader {
private:
    std::string filename;
    nlohmann::json data;

    // хранит координаты OSM-узлов
    // ключ Ч id узла OSM
    // значение Ч пара координат lon, lat
    std::map<long long, std::pair<double, double>> coords;

    void readFile();

    double getDistance(
        double lat1,
        double lon1,
        double lat2,
        double lon2
    );

    bool isRoad(nlohmann::json way);

    // провер€ет, можно ли использовать дорогу в автомобильном режиме
    bool isCarRoad(nlohmann::json way);

public:
    OSMReader(std::string filename);

    // carMode = false Ч строим граф дл€ пешехода
    // carMode = true Ч строим граф дл€ автомобил€
    void loadRoadGraph(Graph& graph, bool carMode = false);
};