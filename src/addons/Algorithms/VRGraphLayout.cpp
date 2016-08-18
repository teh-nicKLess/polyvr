#include "VRGraphLayout.h"
#include "core/math/Octree.h"

using namespace OSG;

VRGraphLayout::VRGraphLayout() {}

void VRGraphLayout::setGraph(graph<Vec3f>& g) { this->g = g; }
graph<Vec3f>& VRGraphLayout::getGraph() { return g; }
void VRGraphLayout::setAlgorithm(ALGORITHM a, int position) { algorithms[position] = a; }
void VRGraphLayout::clearAlgorithms() { algorithms.clear(); }

void VRGraphLayout::applySprings(float eps) {
    for (auto& n : g.getEdges()) {
        for (auto& e : n) {
            auto f1 = getFlag(e.from);
            auto f2 = getFlag(e.to);
            Vec3f& n1 = g.getNodes()[e.from];
            Vec3f& n2 = g.getNodes()[e.to];

            Vec3f d = n2-n1;
            float x = (d.length() - radius)*eps; // displacement
            if (abs(x) < eps) continue;

            if (x > radius*eps) x = radius*eps; // numerical safety ;)
            if (x < -radius*eps) x = -radius*eps;

            Vec3f g; // TODO: not yet working!
            g = gravity*x*0.1;
            switch (e.connection) {
                case graph<Vec3f>::SIMPLE:
                    if (f1 != FIXED) n1 += d*x + g;
                    if (f2 != FIXED) n2 += -d*x + g;
                    break;
                case graph<Vec3f>::HIERARCHY:
                    if (f2 != FIXED) n2 += -d*x + g;
                    else if (f1 != FIXED) n1 += d*x + g;
                    break;
                case graph<Vec3f>::SIBLING:
                    if (x < 0) { // push away siblings
                        if (f1 != FIXED) n1 += d*x + g;
                        if (f2 != FIXED) n2 += -d*x + g;
                    }
                    break;
            }
        }
    }
}

void VRGraphLayout::applyOccupancy(float eps) {
    /**

    - go through all nodes
        - put all positions in an octree

    - go through all nodes again
        - do a range search around node position
        - push all found neighbors away


    */

    Octree o(10*eps);
    int i=0;
    for (auto& n : g.getNodes()) {
        o.add( OcPoint(n, (void*)i) );
        i++;
    }
}

void VRGraphLayout::compute(int N, float eps) {
    for (int i=0; i<N; i++) { // steps
        for (auto a : algorithms) {
            switch(a.second) {
                case SPRINGS:
                    applySprings(eps);
                    break;
                case OCCUPANCYMAP:
                    applyOccupancy(eps);
                    break;
            }
        }
    }
}

VRGraphLayout::FLAG VRGraphLayout::getFlag(int i) { return flags.count(i) ? flags[i] : NONE; }
void VRGraphLayout::fixNode(int i) { flags[i] = FIXED; }

void VRGraphLayout::setRadius(float r) { radius = r; }
void VRGraphLayout::setGravity(Vec3f v) { gravity = v; }
