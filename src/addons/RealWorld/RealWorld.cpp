#include "RealWorld.h"

#include <math.h>

#include <stdio.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>

#include "core/scene/VRSceneManager.h"
#include "core/scene/VRSceneLoader.h"
#include "OSM/OSMMapDB.h"
#include "MapCoordinator.h"
#include "MapManager.h"
//#include "TestMover.h"
#include "TextureManager.h"
#include "Modules/ModuleBuildings.h"
#include "Modules/ModuleStreets.h"
#include "Modules/ModuleFloor.h"
#include "Modules/ModuleTree.h"
#include "Modules/ModuleTerrain.h"
#include "Modules/ModuleWalls.h"
#include "Elevation.h"
#include "Config.h"
#include "traffic/ModuleTraffic.h"

#define PI 3.14159265

using namespace OSG;

RealWorld::RealWorld(VRObjectPtr root) {
    mapCoordinator = new MapCoordinator(Vec2f(49.005f, 8.395f), 0.002f); //Kreuzung Kriegsstr. und Karlstr.
    //mapCoordinator = new MapCoordinator(Vec2f(48.998969, 8.400171), 0.002f); // Tiergarten
    //mapCoordinator = new MapCoordinator(Vec2f(49.013606f, 8.418295f), 0.002f); // Fasanengarten, funktioniert nicht?

    texManager = new TextureManager();
    mapGeometryGenerator = new MapGeometryGenerator(texManager);
    world = new World();
    mapDB = new OSMMapDB();
    mapManager = new MapManager(Vec2f(0,0), mapGeometryGenerator, mapCoordinator, world, root);

    //mapCoordinator->altitude->showHeightArray(49.000070f, 8.401015f);
}

RealWorld::~RealWorld() {
    cout << "\nDESTRUCT REALWORLD\n";

    delete texManager;
    delete mapDB;
    delete world;
    delete mapGeometryGenerator;
    delete mapCoordinator;
    delete mapManager;
}

TrafficSimulation* RealWorld::getTrafficSimulation() { return trafficSimulation; }
void RealWorld::update(Vec3f pos) { mapManager->updatePosition( Vec2f(pos[0], pos[2]) ); }

void RealWorld::enableModule(string mod, bool b) {
    if (b) {
        if (mod == "Ground") mapManager->addModule(new ModuleFloor(mapCoordinator, texManager));
        if (mod == "Streets") mapManager->addModule(new ModuleStreets(mapDB, mapCoordinator, texManager));
        if (mod == "Buildings") mapManager->addModule(new ModuleBuildings(mapDB, mapCoordinator, texManager));
        if (mod == "Walls") mapManager->addModule(new ModuleWalls(mapDB, mapCoordinator, texManager));
        if (mod == "Terrain") mapManager->addModule(new ModuleTerrain(mapDB, mapCoordinator, texManager));
        if (mod == "Trees") mapManager->addModule(new ModuleTree(mapDB, mapCoordinator, texManager));
        if (mod == "Traffic") {
            auto tsim = new ModuleTraffic(mapDB, mapCoordinator, texManager);
            trafficSimulation = tsim->getTrafficSimulation();
            mapManager->addModule(tsim);
        }
    } else {
        // TODO
    }
}

void RealWorld::physicalize(bool b) {
    if (physicalized == b) return;
    physicalized = b;

    mapManager->physicalize(b);
}



