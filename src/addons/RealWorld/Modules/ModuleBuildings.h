#ifndef MODULEBUILDINGS_H
#define MODULEBUILDINGS_H

#include "BaseModule.h"
#include "map"
#include <OpenSG/OSGVector.h>

class OSMMapDB;

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRGeometry;
class VRMaterial;
struct BuildingData;
class Building;
class GeometryData;
class AreaBoundingBox;

class ModuleBuildings: public BaseModule {
    private:
        OSMMapDB* mapDB;
        map<string,VRGeometryPtr> b_geos;
        map<string,VRGeometryPtr> r_geos;
        GeometryData* b_geo_d = 0;
        GeometryData* r_geo_d = 0;
        VRMaterialPtr b_mat = 0;
        bool physics = false;

        int numberFromString(string s);

        void createBuildingPart(BuildingData* bData, string part, string filePath);
        void addBuildingWallLevel(Vec2f pos1, Vec2f pos2, int level, int bNum, float elevation);
        void addBuildingRoof(Building* building, float height, float elevation);
        void makeBuildingGeometry(Building* b); /** create one Building **/

    public:
        virtual string getName();
        virtual void loadBbox(AreaBoundingBox* bbox);
        virtual void unloadBbox(AreaBoundingBox* bbox);
        void physicalize(bool b);
        ModuleBuildings(OSMMapDB* mapDB, MapCoordinator* mapCoordinator, World* world);
};

OSG_END_NAMESPACE;

#endif // MODULEBUILDINGS_H

