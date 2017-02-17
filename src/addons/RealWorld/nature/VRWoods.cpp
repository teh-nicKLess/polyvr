#include "VRWoods.h"
#include "VRTree.h"

#include "core/scene/VRSceneManager.h"
#include "core/objects/object/VRObject.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/geometry/VRGeoData.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/material/VRTextureGenerator.h"
#include "core/objects/VRLod.h"
#include "core/math/Octree.h"
#include "core/utils/toString.h"
#include "core/utils/VRStorage_template.h"

using namespace OSG;

VRLodLeaf::VRLodLeaf(string name, Octree* o, int l) : VRTransform(name), oLeaf(o), lvl(l) {}
VRLodLeaf::~VRLodLeaf() {}
VRLodLeafPtr VRLodLeaf::ptr() { return static_pointer_cast<VRLodLeaf>( shared_from_this() ); }

VRLodLeafPtr VRLodLeaf::create(string name, Octree* o, int lvl) {
    auto l = VRLodLeafPtr(new VRLodLeaf(name, o, lvl));
    l->lod = VRLod::create("lod");
    l->lod->setPersistency(0);
    l->addChild(l->lod);
    auto lvl0 = VRObject::create("lvl");
    lvl0->setPersistency(0);
    l->levels.push_back(lvl0);
    l->lod->addChild(lvl0);
    return l;
}

void VRLodLeaf::addLevel(float dist) {
    auto lvl = VRObject::create("lvl");
    lvl->setPersistency(0);
    levels.push_back(lvl);
    lod->addChild(lvl);
    lod->addDistance(dist);
}

void VRLodLeaf::add(VRObjectPtr obj, int lvl) {
    levels[lvl]->addChild(obj);
}

void VRLodLeaf::set(VRObjectPtr obj, int lvl) {
    levels[lvl]->clearChildren();
    levels[lvl]->addChild(obj);
}

Octree* VRLodLeaf::getOLeaf() { return oLeaf; }
int VRLodLeaf::getLevel() { return lvl; }

// --------------------------------------------------------------------------------------------------

VRLodTree::VRLodTree(string name, float size) : VRObject(name) { octree = Octree::create(size,size); }
VRLodTree::~VRLodTree() {}
VRLodTreePtr VRLodTree::ptr() { return static_pointer_cast<VRLodTree>( shared_from_this() ); }
VRLodTreePtr VRLodTree::create(string name) { return VRLodTreePtr(new VRLodTree(name)); }

void VRLodTree::reset(float size) {
    leafs.clear();
    objects.clear();
    rootLeaf = 0;
    if (size > 0) octree = Octree::create(size,size);
    else {
        auto s = octree->getSize();
        octree = Octree::create(s,s);
    }
    clearChildren();
}

VRLodLeafPtr VRLodTree::addLeaf(Octree* o, int lvl) {
    if (leafs.count(o)) return leafs[o];
    auto l = VRLodLeaf::create("lodLeaf", o, lvl);
    l->setPersistency(0);
    if (lvl > 0) l->addLevel( o->getSize()*2 );
    l->setFrom(o->getLocalCenter());
    leafs[o] = l;

    /**
    add lod leaf to tree, handle following cases:
        - octree leaf has parent
            - parent lod leaf does not exist
                -> call addLeaf on the parent octree leaf, may result in further recursion
            - parent lod leaf exists
                -> pass
            (at this point the leaf parent should exist!)
            -> add leaf to leaf parent
        - octree leaf has no parent
            - first lod leaf (no root leaf set), created in tree constructor
                -> pass
            - tree grows bigger, new root leaf
                -> switch parent of old root leaf to new root leaf
                -> update local position of old root leaf
            -> remember as root leaf
            -> add as child to tree
    */

    if (auto p = o->getParent()) {
        if (!leafs.count(p)) addLeaf(p, lvl+1);
        leafs[p]->add(l,0);
    } else {
        if (rootLeaf) {
            l->add(rootLeaf,0);
            rootLeaf->setFrom(rootLeaf->getOLeaf()->getLocalCenter());
        }
        rootLeaf = l;
        addChild(l);
    }
    return l;
}

VRLodLeafPtr VRLodTree::addObject(VRTransformPtr obj, Vec3f p, int lvl) {
    if (leafs.size() == 0) addLeaf(octree.get(), 0);
    if (!octree) return 0;
    objects[lvl].push_back(obj);
    auto oLeaf = octree->add(p, obj.get(), lvl, 0, true);
    auto leaf = addLeaf(oLeaf, lvl);
    if (lvl == 0) leaf->add(obj, 0);
    else          leaf->add(obj, 1);
    obj->setWorldPosition(p);
    obj->setDir(Vec3f(0,0,-1));
    obj->setUp(Vec3f(0,1,0));
    return leaf;
}

// --------------------------------------------------------------------------------------------------

struct OSG::VRWoodsTreeEntry : public VRName {
    Vec3f pos;
    string type;
    VRWoodsTreeEntry(string name = "") {
        setName("treeEntry");
        store("position", &pos);
        store("tree", &type);
    }
    static shared_ptr<VRWoodsTreeEntry> create(string name = "") { return shared_ptr<VRWoodsTreeEntry>(new VRWoodsTreeEntry(name)); }

    void set(Vec3f p, string t) {
        pos = p;
        type = t;
    }
};

VRWoods::VRWoods() : VRLodTree("woods", 5) {
    storeMap("templateTrees", &treeTemplates, true);
    storeMap("trees", &treeEntries, true);
    regStorageSetupFkt( VRFunction<int>::create("woods setup", boost::bind(&VRWoods::setup, this)) );
}

VRWoods::~VRWoods() {}
VRWoodsPtr VRWoods::create() { return VRWoodsPtr(new VRWoods()); }
VRWoodsPtr VRWoods::ptr() { return static_pointer_cast<VRWoods>( shared_from_this() ); }

void VRWoods::setup() {
    for (auto& t : treeEntries) {
        if (!treeTemplates.count(t.second->type)) { cout << "VRWoods::setup Warning, " << t.second->type << " is not a tree template!" << endl; continue; }
        auto tree = treeTemplates[t.second->type];
        tree->setFrom(t.second->pos);
        addTree(tree);
    }
    computeLODs();
}

VRTreePtr VRWoods::addTree(VRTreePtr t, bool updateLODs) {
    auto td = dynamic_pointer_cast<VRTree>( t->duplicate() );
    treeTemplates[t->getName()] = t;
    treeRefs[td.get()] = t;
    auto leaf = addObject(td, t->getFrom(), 0);

    auto te = VRWoodsTreeEntry::create();
    te->set( t->getFrom(), t->getName() );
    treeEntries[td->getName()] = te;

    if (updateLODs) {
        auto oLeafs = leaf->getOLeaf()->getAncestry();
        map<Octree*, VRLodLeafPtr> aLeafs;
        for (auto o : oLeafs) {
            if (leafs.count(o) == 0) continue;
            aLeafs[o] = leafs[o];
        }
        computeLODs(aLeafs);
    }

    return td;
}

void VRWoods::computeLODs() {
    computeLODs(leafs);
}

void VRWoods::computeLODs(map<Octree*, VRLodLeafPtr>& leafs) {
    auto simpleLeafMat = []() {
        auto m = VRMaterial::create("simpleLeafMat");
        m->setPointSize(3);
        m->setDiffuse(Vec3f(0.5,1,0));
        m->setAmbient(Vec3f(0.1,0.3,0));
        m->setSpecular(Vec3f(0.1,0.4,0));
        string wdir = VRSceneManager::get()->getOriginalWorkdir();
        m->readFragmentShader(wdir+"/shader/Trees/Shader_leafs_lod.fp");
        m->readVertexShader(wdir+"/shader/Trees/Shader_leafs_lod.vp");

        auto tg = VRTextureGenerator::create();
        tg->setSize(Vec3i(50,50,50), 1);
        float r = 0.85;
        float g = 1.0;
        float b = 0.8;
        tg->add(PERLIN, 1, Vec4f(r,g,b,0.9), Vec4f(r,g,b,1) );
        tg->add(PERLIN, 0.5, Vec4f(r,g,b,0.5), Vec4f(r,g,b,1) );
        tg->add(PERLIN, 0.25, Vec4f(r,g,b,0.8), Vec4f(r,g,b,1) );
        m->setTexture(tg->compose(0));

        return m;
    };

    auto simpleTrunkMat = []() {
        auto m = VRMaterial::create("brown");
        m->setDiffuse(Vec3f(0.6,0.3,0));
        string wdir = VRSceneManager::get()->getOriginalWorkdir();
        m->readFragmentShader(wdir+"/shader/Trees/Shader_trunc_lod.fp");
        m->readVertexShader(wdir+"/shader/Trees/Shader_trunc_lod.vp");
        return m;
    };

    // get all trees for each leaf layer
    map<VRLodLeaf*, vector<VRTree*> > trees;
    for (auto l : leafs) {
        auto& leaf = l.second;
        int lvl = leaf->getLevel();
        if (lvl == 0) continue;

        vector<void*> data = leaf->getOLeaf()->getAllData();
        for (auto v : data) trees[leaf.get()].push_back((VRTree*)v);
    }

    // create layer node geometries
    for (auto l : leafs) {
        auto& leaf = l.second;
        if (trees.count(leaf.get()) == 0) continue;
        int lvl = leaf->getLevel();
        if (lvl == 0) continue;

        Vec3f pos;
        for (auto t : trees[leaf.get()]) pos += t->getWorldPosition();
        pos *= 1.0/trees[leaf.get()].size();

        VRGeoData geoLeafs;
        VRGeoData geoTrunk;
        for (auto t : trees[leaf.get()]) {
            if (treeRefs.count(t) == 0) continue;
            auto tRef = treeRefs[t];
            if (!tRef || !t) continue;
            Vec3f offset = t->getWorldPosition() - pos;
            tRef->createHullTrunkLod(geoTrunk, lvl, offset);
            tRef->createHullLeafLod (geoLeafs, lvl, offset);
        }

        if (geoTrunk.size() > 0) {
            auto trunk = geoTrunk.asGeometry("trunk");
            if (!truncMat) truncMat = simpleTrunkMat();
            trunk->setMaterial(truncMat);

            leaf->set( trunk, 1 );
            trunk->setWorldPosition(pos);
            trunk->setDir(Vec3f(0,0,-1));
            trunk->setUp(Vec3f(0,1,0));

            if (geoLeafs.size() > 0) {
                auto leafs = geoLeafs.asGeometry("leafs");
                leafs->setPersistency(0);
                trunk->addChild( leafs );
                if (!leafMat) leafMat = simpleLeafMat();
                leafs->setMaterial(leafMat);
            }
        }
    }
}

void VRWoods::clear() {
    trees.clear();
    treeTemplates.clear();
    VRLodTree::reset();
}

void VRWoods::test() {
    /*auto newCylinder = [](float s) {
        auto box = VRGeometry::create("box");
        string S = toString(s*0.5);
        box->setPrimitive("Cylinder", "1 "+S+" 16 1 1 1");
        return box;
    };*/

    auto simpleLeafMat = []() {
        auto m = VRMaterial::create("lmat");
        m->setPointSize(3);
        m->setDiffuse(Vec3f(0.5,1,0));
        return m;
    };

    auto simpleTrunkMat = []() {
        auto m = VRMaterial::create("brown");
        m->setDiffuse(Vec3f(0.6,0.3,0));
        return m;
    };

    auto newTree = [](VRMaterialPtr m) {
        auto t = VRTree::create();
        t->setup(5, 5, rand());
        t->addLeafs(4, 3);
        t->addLeafs(5, 3);
        //t->setLeafMaterial(m);
        return t;
    };

    /*auto simpleTest = [&]() {
        // add highest detail objects
        int N = 4;
        for (int i=0; i<N; i++) {
            for (int j=0; j<N; j++) {
                auto c = newCylinder(1);
                Vec3f p = Vec3f((i-N*0.5)*15,0,(j-N*0.5)*15);
                addObject(c, p, 0);
            }
        }

        // add lower detailed objects
        for (auto l : leafs) {
            auto& leaf = l.second;
            cout << "LEAF: " << leaf->getOLeaf()->getSize() << " " << leaf->getLevel() << endl;
            int lvl = leaf->getLevel();
            if (lvl == 0) continue;
            auto c = newCylinder(pow(2.0,lvl));
            leaf->add(c,1);
            //c->setWorldPosition(Vec3f());
        }
    };*/

    /*auto simpleTest2 = [&]() { // TODO: not perfectly working
        reset(1);

        for (int k=0; k<5; k++) {
            int N = pow(2,k);
            float s = 16.0/N;
            float o = 0.5*s;
            for (int i=0; i<N; i++) {
                for (int j=0; j<N; j++) {
                    auto c = newCylinder(s);
                    Vec3f p = Vec3f(o+s*i,0,o+s*j);
                    addObject(c, p, 4-k);
                }
            }
        }
    };*/

    auto simpleTest3 = [&]() {
        reset(5);
        auto green = simpleLeafMat();
        auto brown = simpleTrunkMat();

        float W = 100;
        float L = 100;
        int N = 100;


        // lowest level trees
        srand(time(0));
        vector<Vec3f> positions;
        for (int i=0; i<N; i++) {
            float x = (float(rand())/RAND_MAX)*W;
            float z = (float(rand())/RAND_MAX)*L;
            positions.push_back(Vec3f(x,0,z));
        }

        for (auto p : positions) {
            auto t = newTree(green);
            addObject(t, p, 0);
        }

        // get all trees for each layer leaf
        map<VRLodLeaf*, vector<VRTree*> > trees;
        for (auto l : leafs) {
            auto& leaf = l.second;
            int lvl = leaf->getLevel();
            if (lvl == 0) continue;

            vector<void*> data = leaf->getOLeaf()->getAllData();
            for (auto v : data) trees[leaf.get()].push_back((VRTree*)v);
        }

        // create layer node geometries
        for (auto l : leafs) {
            auto& leaf = l.second;
            int lvl = leaf->getLevel();
            if (lvl == 0) continue;

            Vec3f pos;
            for (auto t : trees[leaf.get()]) pos += t->getWorldPosition();
            pos *= 1.0/trees[leaf.get()].size();

            VRGeoData geoLeafs;
            VRGeoData geoTrunk;
            for (auto t : trees[leaf.get()]) {
                Vec3f offset = t->getWorldPosition() - pos;
                float amount = 0.1/lvl;
                t->createHullTrunkLod(geoTrunk, amount, offset);
                t->createHullLeafLod (geoLeafs, amount, offset);
            }
            auto trunk = geoTrunk.asGeometry("trunk");
            auto leafs = geoLeafs.asGeometry("leafs");
            trunk->addChild( leafs );
            leafs->setMaterial(green);
            trunk->setMaterial(brown);

            leaf->add( trunk, 1 );
            trunk->setWorldPosition(pos);
            trunk->setDir(Vec3f(0,0,-1));
            trunk->setUp(Vec3f(0,1,0));
        }
    };

    //simpleTest();
    //simpleTest2();
    simpleTest3();
}

/**

TODO:
 - get rid of transform of lodleaf?
 - optimize level zero structure
*/



