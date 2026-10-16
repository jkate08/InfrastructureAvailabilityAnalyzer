#define NOMINMAX

#include <windows.h>
#include <string>
#include <wrl.h>
#include <cstdio>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>
#include <map>

#include "Point.h"
#include "Node.h"
#include "Edge.h"
#include "Graph.h"
#include "IsochroneSolver.h"
#include "OSMReader.h"
#include "TransportMode.h"

#include <WebView2.h>

using namespace std;
using namespace Microsoft::WRL;

// объекты WebView2
ComPtr<ICoreWebView2Controller> webviewController;
ComPtr<ICoreWebView2> webview;

HWND g_hwnd = nullptr;

// дорожный граф, построенный по данным OpenStreetMap
Graph g_roadGraph;

// ближайший узел графа к точке, выбранной пользователем на карте
Node* startNode = nullptr;

// координаты точки, выбранной пользователем
double g_lat = 0.0;
double g_lng = 0.0;

// была ли уже выбрана точка на карте
bool g_hasSelectedPoint = false;

// сообщение, которое будет отправлено в JavaScript после загрузки карты
wstring g_statusMessage;

wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    wstring exePath(path);
    size_t pos = exePath.find_last_of(L"\\/");

    if (pos == wstring::npos) {
        return L"";
    }

    return exePath.substr(0, pos + 1);
}

string WideToUtf8(const wstring& text) {
    if (text.empty()) {
        return "";
    }

    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        (int)text.size(),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    string result(sizeNeeded, 0);

    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        (int)text.size(),
        &result[0],
        sizeNeeded,
        nullptr,
        nullptr
    );

    return result;
}

// формируем путь к файлу map.html
wstring GetHtmlPath() {
    wstring exeDir = GetExeDirectory();
    wstring htmlPath = exeDir + L"Resources\\map.html";

    return L"file:///" + htmlPath;
}

// отправляет строку из C++ в JavaScript
void SendMessageToJs(const wstring& text) {
    if (webview) {
        webview->PostWebMessageAsString(text.c_str());
    }
}

// расчёт расстояния между двумя географическими точками
// используем формулу гаверсинуса
double DistanceMeters(double lat1, double lon1, double lat2, double lon2) {
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

// поиск ближайшего узла дорожного графа к точке,
// на которую пользователь нажал на карте
Node* FindNearestRoadNode(double lat, double lng) {
    Node* nearest = nullptr;
    double bestDistance = numeric_limits<double>::infinity();

    vector<Node*> nodes = g_roadGraph.getNodes();

    for (int i = 0; i < nodes.size(); i++) {
        Point position = nodes[i]->getPosition();

        double nodeLng = position.getX();
        double nodeLat = position.getY();

        double distance = DistanceMeters(lat, lng, nodeLat, nodeLng);

        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = nodes[i];
        }
    }

    return nearest;
}

// возвращает цвет точки в зависимости от времени достижения
string GetColorByTime(double time) {
    if (time <= 300.0) {
        return "green";
    }

    if (time <= 600.0) {
        return "yellow";
    }

    if (time <= 900.0) {
        return "red";
    }

    return "gray";
}

// создаёт объект режима движения
TransportMode* CreateTransportMode(const wstring& modeName) {
    if (modeName == L"car") {
        return new CarMode();
    }

    return new WalkMode();
}

// перестраивает дорожный граф под нужный режим
// carMode = false — граф для пешехода
// carMode = true — граф для автомобиля
bool RebuildRoadGraph(bool carMode) {
    try {
        wstring exeDir = GetExeDirectory();
        wstring exportPathW = exeDir + L"export.json";

        string exportPath = WideToUtf8(exportPathW);

        OSMReader reader(exportPath);
        reader.loadRoadGraph(g_roadGraph, carMode);

        return true;
    }
    catch (...) {
        return false;
    }
}

// загрузка данных OSM из export.json
// создаётся дорожный граф и файл osm_data.js
// с количеством узлов и рёбер для отображения на карте
void LoadOSMData() {
    try {
        wstring exeDir = GetExeDirectory();

        wstring exportPathW = exeDir + L"export.json";
        wstring resourcesDirW = exeDir + L"Resources\\";
        wstring dataJsPathW = resourcesDirW + L"osm_data.js";

        CreateDirectoryW(resourcesDirW.c_str(), nullptr);

        string exportPath = WideToUtf8(exportPathW);

        // читаем export.json и строим граф
        // при запуске строим обычный пешеходный граф,
        // чтобы пользователь мог выбрать точку на карте
        OSMReader reader(exportPath);
        reader.loadRoadGraph(g_roadGraph, false);

        int roadNodesCount = (int)g_roadGraph.getNodes().size();
        int roadEdgesCount = (int)g_roadGraph.getEdges().size();

        string jsContent;
        jsContent += "window.ROAD_NODES_COUNT = ";
        jsContent += to_string(roadNodesCount);
        jsContent += ";\n";

        jsContent += "window.ROAD_EDGES_COUNT = ";
        jsContent += to_string(roadEdgesCount);
        jsContent += ";\n";

        FILE* file = nullptr;
        _wfopen_s(&file, dataJsPathW.c_str(), L"wb");

        if (!file) {
            g_statusMessage =
                L"{\"type\":\"error\",\"text\":\"Не удалось создать файл Resources/osm_data.js\"}";
            return;
        }

        fwrite(jsContent.data(), 1, jsContent.size(), file);
        fclose(file);

        // сообщение для HTML-карты о том, что данные загружены
        wstring message;
        message += L"{\"type\":\"data_ready\"";
        message += L",\"roadNodes\":";
        message += to_wstring(roadNodesCount);
        message += L",\"roadEdges\":";
        message += to_wstring(roadEdgesCount);
        message += L"}";

        g_statusMessage = message;
    }
    catch (...) {
        g_statusMessage =
            L"{\"type\":\"error\",\"text\":\"Не удалось загрузить export.json. Проверьте, что файл лежит рядом с exe и содержит highway.\"}";
    }
}

// обработка клика по карте
// пользователь выбирает точку, а программа ищет ближайший узел графа
void HandleClick(double lat, double lng) {
    g_lat = lat;
    g_lng = lng;
    g_hasSelectedPoint = true;

    startNode = FindNearestRoadNode(lat, lng);

    if (!startNode) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Не найден дорожный граф OSM. Проверьте, что export.json содержит way[highway].\"}"
        );
        return;
    }

    Point position = startNode->getPosition();

    // отправляем в JavaScript координаты ближайшего узла.
    wstring message;
    message += L"{\"type\":\"point_saved\"";
    message += L",\"nearestLat\":";
    message += to_wstring(position.getY());

    message += L",\"nearestLng\":";
    message += to_wstring(position.getX());
    message += L"}";

    SendMessageToJs(message);
}

// запуск расчёта зон доступности
void HandleRunMode(TransportMode* transport, bool carMode) {
    double maxTimeLimit = 900.0; // 15 минут

    if (transport == nullptr) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Не выбран режим движения\"}"
        );
        return;
    }

    if (!g_hasSelectedPoint) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Сначала выберите точку на карте\"}"
        );
        return;
    }

    // перед расчётом перестраиваем граф под выбранный режим:
    // для пешехода — со всеми пешеходными дорожками,
    // для машины — без парков, тропинок, footway, path, pedestrian и т.д.
    startNode = nullptr;

    if (!RebuildRoadGraph(carMode)) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Не удалось перестроить дорожный граф. Проверьте export.json.\"}"
        );
        return;
    }

    if (g_roadGraph.getNodes().empty()) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Дорожный граф пустой. В export.json нет подходящих дорог для выбранного режима.\"}"
        );
        return;
    }

    // после перестроения графа обязательно заново ищем ближайший узел,
    // потому что старый startNode относился к старому графу
    startNode = FindNearestRoadNode(g_lat, g_lng);

    if (!startNode) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Не найден ближайший дорожный узел для выбранного режима.\"}"
        );
        return;
    }

    double speed = transport->getSpeed();

    // запускаем алгоритм Дейкстры
    IsochroneResult result = IsochroneSolver::computeResult(
        g_roadGraph,
        startNode,
        speed,
        maxTimeLimit
    );

    // формируем JSON-строку с точками и сегментами для JavaScript
    wstring json;
    json += L"{\"type\":\"points_result\",\"points\":[";

    bool firstPoint = true;

    int greenCount = 0;
    int yellowCount = 0;
    int redCount = 0;

    map<Node*, double>::iterator it;

    // перебираем все рассчитанные времена достижения
    for (it = result.times.begin(); it != result.times.end(); it++) {
        Node* node = it->first;
        double time = it->second;

        if (time > maxTimeLimit) {
            continue;
        }

        Point position = node->getPosition();
        string color = GetColorByTime(time);

        if (color == "green") {
            greenCount++;
        }

        if (color == "yellow") {
            yellowCount++;
        }

        if (color == "red") {
            redCount++;
        }

        if (!firstPoint) {
            json += L",";
        }

        // добавляем точку в JSON
        json += L"{\"lat\":";
        json += to_wstring(position.getY());

        json += L",\"lng\":";
        json += to_wstring(position.getX());

        json += L",\"time\":";
        json += to_wstring(time);

        json += L",\"color\":\"";
        json += wstring(color.begin(), color.end());
        json += L"\"}";

        firstPoint = false;
    }

    json += L"],\"segments\":[";

    bool firstSegment = true;

    unordered_set<string> usedSegments;

    vector<Edge*> edges = g_roadGraph.getEdges();

    // формируем список серых линий между достижимыми узлами
    for (int i = 0; i < edges.size(); i++) {
        Edge* edge = edges[i];

        Node* from = edge->getFrom();
        Node* to = edge->getTo();

        if (from == nullptr || to == nullptr) {
            continue;
        }

        double fromTime = result.times[from];
        double toTime = result.times[to];

        // рисуем только те линии, у которых оба конца достижимы за 15 минут
        if (fromTime > maxTimeLimit || toTime > maxTimeLimit) {
            continue;
        }

        int id1 = from->getId();
        int id2 = to->getId();

        int minId;
        int maxId;

        if (id1 < id2) {
            minId = id1;
            maxId = id2;
        }
        else {
            minId = id2;
            maxId = id1;
        }

        string key = to_string(minId) + "-" + to_string(maxId);

        if (usedSegments.find(key) != usedSegments.end()) {
            continue;
        }

        usedSegments.insert(key);

        Point fromPosition = from->getPosition();
        Point toPosition = to->getPosition();

        if (!firstSegment) {
            json += L",";
        }

        // добавляем отрезок дороги в JSON
        json += L"{\"lat1\":";
        json += to_wstring(fromPosition.getY());

        json += L",\"lng1\":";
        json += to_wstring(fromPosition.getX());

        json += L",\"lat2\":";
        json += to_wstring(toPosition.getY());

        json += L",\"lng2\":";
        json += to_wstring(toPosition.getX());

        json += L"}";

        firstSegment = false;
    }

    // добавляем количество точек каждого цвета
    json += L"],\"greenCount\":";
    json += to_wstring(greenCount);

    json += L",\"yellowCount\":";
    json += to_wstring(yellowCount);

    json += L",\"redCount\":";
    json += to_wstring(redCount);

    json += L"}";

    // отправляем результат в JavaScript
    SendMessageToJs(json);
}

// обработка сообщений, которые приходят из JavaScript
void HandleMessageFromJs(const wstring& message) {
    try {
        if (message.substr(0, 6) == L"CLICK|") {
            size_t sep = message.find(L"|", 6);

            if (sep == wstring::npos) {
                SendMessageToJs(
                    L"{\"type\":\"error\",\"text\":\"Ошибка чтения координат точки\"}"
                );
                return;
            }

            double lat = stod(message.substr(6, sep - 6));
            double lng = stod(message.substr(sep + 1));

            HandleClick(lat, lng);
        }

        else if (message.substr(0, 4) == L"RUN|") {
            wstring mode = message.substr(4);

            // создаём объект режима движения
            TransportMode* transport = CreateTransportMode(mode);

            // определяем, выбран ли автомобильный режим
            bool carMode = false;

            if (mode == L"car") {
                carMode = true;
            }

            HandleRunMode(transport, carMode);

            // освобождаем память
            delete transport;
        }
    }
    catch (...) {
        SendMessageToJs(
            L"{\"type\":\"error\",\"text\":\"Ошибка обработки сообщения из карты\"}"
        );
    }
}

// обработка событий окна Windows
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        // при изменении размера окна меняем размер WebView
        if (webviewController) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            webviewController->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
        startNode = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // загружаем данные OSM и строим граф
    LoadOSMData();

    const wchar_t CLASS_NAME[] = L"MyMapWindow";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Анализатор доступности инфраструктуры",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        800,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 0;
    }

    g_hwnd = hwnd;

    ShowWindow(hwnd, SW_SHOW);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    return E_FAIL;
                }

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) {
                                return E_FAIL;
                            }

                            webviewController = controller;
                            controller->get_CoreWebView2(&webview);

                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            webviewController->put_Bounds(bounds);

                            ComPtr<ICoreWebView2Settings> settings;
                            webview->get_Settings(&settings);
                            settings->put_IsScriptEnabled(TRUE);
                            settings->put_IsWebMessageEnabled(TRUE);

                            webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        PWSTR message = nullptr;

                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                                            HandleMessageFromJs(wstring(message));
                                            CoTaskMemFree(message);
                                        }

                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                        if (!g_statusMessage.empty()) {
                                            SendMessageToJs(g_statusMessage);
                                        }

                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            ComPtr<ICoreWebView2Settings2> settings2;

                            HRESULT hr = settings.As(&settings2);

                            if (FAILED(hr)) {
                                MessageBoxW(
                                    hwnd,
                                    L"Не поддерживается ICoreWebView2Settings2. Обновите WebView2 Runtime.",
                                    L"Ошибка",
                                    MB_OK | MB_ICONERROR
                                );
                                return hr;
                            }

                            hr = settings2->put_UserAgent(L"IsochroneAnalyzer/1.0");

                            if (FAILED(hr)) {
                                MessageBoxW(
                                    hwnd,
                                    L"Не удалось установить User-Agent.",
                                    L"Ошибка",
                                    MB_OK | MB_ICONERROR
                                );
                                return hr;
                            }

                            ComPtr<ICoreWebView2_3> webview3;

                            hr = webview.As(&webview3);

                            if (FAILED(hr)) {
                                MessageBoxW(
                                    hwnd,
                                    L"Не поддерживается ICoreWebView2_3. Обновите WebView2 Runtime.",
                                    L"Ошибка",
                                    MB_OK | MB_ICONERROR
                                );
                                return hr;
                            }

                            wstring resourcesPath = GetExeDirectory() + L"Resources";

                            hr = webview3->SetVirtualHostNameToFolderMapping(
                                L"isochrone-analyzer.example",
                                resourcesPath.c_str(),
                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY
                            );

                            if (FAILED(hr)) {
                                MessageBoxW(
                                    hwnd,
                                    L"Не удалось подключить папку Resources.",
                                    L"Ошибка",
                                    MB_OK | MB_ICONERROR
                                );
                                return hr;
                            }

                            return webview->Navigate(
                                L"https://isochrone-analyzer.example/map.html"
                            );
                        }
                    ).Get()
                );

                return S_OK;
            }
        ).Get()
    );

    MSG msg = {};

    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();

    return 0;
}