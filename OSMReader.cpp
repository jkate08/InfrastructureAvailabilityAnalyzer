#include "OSMReader.h"

#include <fstream>
#include <cmath>
#include <unordered_map>
#include <stdexcept>
#include <string>

using namespace std;
using json = nlohmann::json;

OSMReader::OSMReader(string filename) {
    this->filename = filename;
    readFile();
}

// получает значение тега OSM
// если тега нет или он не строковый — возвращает пустую строку
static string getTagValue(json tags, string key) {
    if (!tags.contains(key)) {
        return "";
    }

    if (!tags[key].is_string()) {
        return "";
    }

    return tags[key];
}

// проверяет, означает ли тег запрет для автомобиля
static bool isForbiddenForCar(string value) {
    if (value == "no") {
        return true;
    }

    if (value == "private") {
        return true;
    }

    if (value == "agricultural") {
        return true;
    }

    if (value == "forestry") {
        return true;
    }

    if (value == "delivery") {
        return true;
    }

    if (value == "customers") {
        return true;
    }

    // destination - дорога не для сквозного проезда.
    if (value == "destination") {
        return true;
    }

    return false;
}

// чтение JSON-файла export.json
void OSMReader::readFile() {
    ifstream file(filename);

    if (!file.is_open()) {
        throw runtime_error("cannot open export.json");
    }

    file >> data;

    coords.clear();

    int count = (int)data["elements"].size();

    // сначала сохраняем все OSM-точки с координатами
    for (int i = 0; i < count; i++) {
        json element = data["elements"][i];

        if (!element.contains("type")) {
            continue;
        }

        if (element["type"] == "node") {
            if (!element.contains("id")) {
                continue;
            }

            if (!element.contains("lat")) {
                continue;
            }

            if (!element.contains("lon")) {
                continue;
            }

            long long id = element["id"];
            double lat = element["lat"];
            double lon = element["lon"];

            // сохраняем как lon, lat
            coords[id] = make_pair(lon, lat);
        }
    }
}

// расчёт расстояния между двумя географическими точками
// используется формула гаверсинуса
double OSMReader::getDistance(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    double R = 6371000.0;
    double pi = 3.1415926535;

    double dLat = (lat2 - lat1) * pi / 180.0;
    double dLon = (lon2 - lon1) * pi / 180.0;

    lat1 = lat1 * pi / 180.0;
    lat2 = lat2 * pi / 180.0;

    double a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1) * cos(lat2) *
        sin(dLon / 2) * sin(dLon / 2);

    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return R * c;
}

// проверяет, является ли объект OSM дорогой,
// которую можно использовать в графе
bool OSMReader::isRoad(json way) {
    if (!way.contains("tags")) {
        return false;
    }

    if (!way["tags"].contains("highway")) {
        return false;
    }

    string type = way["tags"]["highway"];

    // исключаем неподходящие типы дорог
    if (type == "construction") {
        return false;
    }

    if (type == "proposed") {
        return false;
    }

    if (type == "raceway") {
        return false;
    }

    if (type == "abandoned") {
        return false;
    }

    if (type == "platform") {
        return false;
    }

    return true;
}

// проверяет, можно ли автомобилю ехать по конкретной дороге OSM
bool OSMReader::isCarRoad(json way) {
    if (!way.contains("tags")) {
        return false;
    }

    json tags = way["tags"];

    if (!tags.contains("highway")) {
        return false;
    }

    string type = getTagValue(tags, "highway");

    // сначала проверяем прямые запреты для автомобилей
    if (isForbiddenForCar(getTagValue(tags, "motorcar"))) {
        return false;
    }

    if (isForbiddenForCar(getTagValue(tags, "motor_vehicle"))) {
        return false;
    }

    if (isForbiddenForCar(getTagValue(tags, "vehicle"))) {
        return false;
    }

    if (isForbiddenForCar(getTagValue(tags, "access"))) {
        return false;
    }

    // исключаем типы дорог, по которым машина обычно не может двигаться
    // именно эти типы чаще всего встречаются в парках, скверах и пешеходных зонах
    if (type == "footway") {
        return false;
    }

    if (type == "path") {
        return false;
    }

    if (type == "pedestrian") {
        return false;
    }

    if (type == "steps") {
        return false;
    }

    if (type == "cycleway") {
        return false;
    }

    if (type == "bridleway") {
        return false;
    }

    if (type == "track") {
        return false;
    }

    if (type == "corridor") {
        return false;
    }

    if (type == "elevator") {
        return false;
    }

    if (type == "platform") {
        return false;
    }

    // исключаем частные заезды
    if (getTagValue(tags, "service") == "driveway") {
        return false;
    }

    // если прямо на дороге указано, что это парк или зелёная зона,
    // автомобиль туда не пускаем
    if (getTagValue(tags, "leisure") == "park") {
        return false;
    }

    if (getTagValue(tags, "amenity") == "park") {
        return false;
    }

    if (getTagValue(tags, "landuse") == "grass") {
        return false;
    }

    if (getTagValue(tags, "landuse") == "forest") {
        return false;
    }

    if (getTagValue(tags, "landuse") == "recreation_ground") {
        return false;
    }

    if (getTagValue(tags, "natural") == "wood") {
        return false;
    }

    return true;
}

// построение дорожного графа по данным OSM
void OSMReader::loadRoadGraph(Graph& graph, bool carMode) {
    graph.clear();

    // связь между id OSM-узла и нашим объектом Node
    unordered_map<long long, Node*> nodesByOsmId;

    int localId = 1;

    int elementsCount = (int)data["elements"].size();

    for (int i = 0; i < elementsCount; i++) {
        json way = data["elements"][i];

        if (!way.contains("type")) {
            continue;
        }

        if (way["type"] != "way") {
            continue;
        }

        if (!isRoad(way)) {
            continue;
        }

        // если выбран автомобильный режим,
        // пропускаем дороги, по которым машина ехать не должна
        if (carMode && !isCarRoad(way)) {
            continue;
        }

        if (!way.contains("nodes")) {
            continue;
        }

        int wayNodesCount = (int)way["nodes"].size();

        if (wayNodesCount < 2) {
            continue;
        }

        bool canGoForward = true;
        bool canGoBack = true;

        // проверяем односторонние дороги
        if (way.contains("tags")) {
            if (way["tags"].contains("oneway")) {
                string oneway = way["tags"]["oneway"];

                if (oneway == "yes" || oneway == "true" || oneway == "1") {
                    canGoBack = false;
                }

                if (oneway == "-1") {
                    canGoForward = false;
                    canGoBack = true;
                }
            }
        }

        // way состоит из последовательности точек
        // между соседними точками создаём рёбра графа
        for (int j = 0; j < wayNodesCount - 1; j++) {
            long long id1 = way["nodes"][j];
            long long id2 = way["nodes"][j + 1];

            if (coords.find(id1) == coords.end()) {
                continue;
            }

            if (coords.find(id2) == coords.end()) {
                continue;
            }

            // если узла ещё нет в графе — создаём его
            if (nodesByOsmId.find(id1) == nodesByOsmId.end()) {
                double lon = coords[id1].first;
                double lat = coords[id1].second;

                Node* node = new Node(localId, Point(lon, lat));
                localId++;

                nodesByOsmId[id1] = node;
                graph.addNode(node);
            }

            if (nodesByOsmId.find(id2) == nodesByOsmId.end()) {
                double lon = coords[id2].first;
                double lat = coords[id2].second;

                Node* node = new Node(localId, Point(lon, lat));
                localId++;

                nodesByOsmId[id2] = node;
                graph.addNode(node);
            }

            Node* node1 = nodesByOsmId[id1];
            Node* node2 = nodesByOsmId[id2];

            double lon1 = node1->getPosition().getX();
            double lat1 = node1->getPosition().getY();

            double lon2 = node2->getPosition().getX();
            double lat2 = node2->getPosition().getY();

            double distance = getDistance(lat1, lon1, lat2, lon2);

            if (distance <= 0) {
                continue;
            }

            // добавляем ребро в прямом направлении
            if (canGoForward) {
                Edge* edge = new Edge(node1, node2, distance);
                graph.addEdge(edge);
                node1->addEdge(edge);
            }

            // добавляем ребро в обратном направлении,
            // если дорога не является односторонней
            if (canGoBack) {
                Edge* edge = new Edge(node2, node1, distance);
                graph.addEdge(edge);
                node2->addEdge(edge);
            }
        }
    }
}