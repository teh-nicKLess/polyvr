#ifndef VRWOODS_H_INCLUDED
#define VRWOODS_H_INCLUDED

#include "core/objects/VRLod.h"
#include "core/objects/VRTransform.h"
#include "core/math/VRMathFwd.h"
#include "addons/RealWorld/VRRealWorldFwd.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRLodLeaf : public VRTransform {
    private:
        VRLodPtr lod;
        vector<VRObjectPtr> levels;

    public:
        VRLodLeaf(string name);
        ~VRLodLeaf();

        static VRLodLeafPtr create(string name);
        VRLodLeafPtr ptr();

        void addLevel(float dist);
        void add(VRObjectPtr obj, int lvl);
};

class VRLodTree : public VRObject {
    private:
        OctreePtr octree;
        map<Octree*, VRLodLeafPtr> lods;

    public:
        VRLodTree(string name);
        ~VRLodTree();

        static VRLodTreePtr create(string name);
        VRLodTreePtr ptr();

        void addObject(VRObjectPtr obj, Vec3f p, int lvl);
};

class VRWoods : public VRLodTree {
    private:
        vector<VRTreePtr> trees;

        void computeFirstLevel();
        void computeSecondLevel();

        void initLOD();

    public:
        VRWoods();
        ~VRWoods();

        static VRWoodsPtr create();
        VRWoodsPtr ptr();

        void addTree(VRTreePtr t);

        void test();
};

OSG_END_NAMESPACE;

#endif // VRWOODS_H_INCLUDED
