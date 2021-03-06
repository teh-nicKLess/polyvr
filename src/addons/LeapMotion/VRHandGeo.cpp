#include "VRHandGeo.h"
#include <core/objects/geometry/OSGGeometry.h>
#include <core/utils/VRFunction.h>
#include <core/scene/VRScene.h>
#include <core/math/pose.h>
#include <OpenSG/OSGSimpleGeometry.h>

using namespace OSG;

VRHandGeo::VRHandGeo(string name) : VRGeometry(name), bones(5) {
    //setPrimitive("Box", "0.05 0.01 0.06 1 1 1"); // palm geometry
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 4; ++j) {
            VRGeometryPtr geo = VRGeometry::create(name + "_bone_" + to_string(i) + "_" + to_string(j));
            bones[i].push_back(geo);
        }
    }
    pinch = VRGeometry::create(name + "_pinch", true);
    pinch->setPrimitive("Sphere 0.025 2");

    updateCb = VRUpdateCb::create("handgeo_update", boost::bind(&VRHandGeo::updateHandGeo, this));
    VRScene::getCurrent()->addUpdateFkt(updateCb);
}

VRHandGeoPtr VRHandGeo::create(string name) {
    VRHandGeoPtr ptr = VRHandGeoPtr(new VRHandGeo(name));
    for (auto& b_ : ptr->bones) {
        for (auto& b : b_) {
            ptr->addChild(b);
        }
    }

    ptr->hide();
    return ptr;
}

void VRHandGeo::connectToLeap(VRLeapPtr leap) {
    if (!leap) return;
    function<void(VRLeapFramePtr)> cb = [this](VRLeapFramePtr frame) {
        update(frame);
    };
    leap->registerFrameCallback(cb);
}


void VRHandGeo::updateHandGeo() {

//    std::cout << "updateHandGeo" << endl;

    boost::mutex::scoped_lock lock(mutex);

    if (visible != isVisible()) { toggleVisible(); }

    if (handData && isVisible()) {

        // Palm
        setRelativePose(handData->pose, getParent());

        // Bones
        for (int i = 0; i < 5; ++i) {
            Vec3d p0 = (handData->joints[i][0] + handData->joints[i][1]) / 2;
            Vec3d p1 = (handData->joints[i][1] + handData->joints[i][2]) / 2;
            Vec3d p2 = (handData->joints[i][2] + handData->joints[i][3]) / 2;
            Vec3d p3 = (handData->joints[i][3] + handData->joints[i][4]) / 2;
            float l0 = (handData->joints[i][0] - handData->joints[i][1]).length();
            float l1 = (handData->joints[i][1] - handData->joints[i][2]).length();
            float l2 = (handData->joints[i][2] - handData->joints[i][3]).length();
            float l3 = (handData->joints[i][3] - handData->joints[i][4]).length();

            // The thumb does not have a base metacarpal bone and therefore contains a valid, zero length bone at that location.
            // TODO: maybe better with "if (i > 0 || l0 > 0)" to only allow zero length bone on thumb?
            if (l0 > 0) {
                bones[i][0]->setMesh(OSGGeometry::create(makeCylinderGeo(l0, 0.0075, 8, true, true, true)));
                auto p = Pose::create(p0, handData->bases[i][0].dir(), handData->bases[i][0].up());
                bones[i][0]->setRelativePose(p, getParent());
            } else {
                bones[i][0]->hide();
            }

            bones[i][1]->setMesh(OSGGeometry::create(makeCylinderGeo(l1, 0.0075, 8, true, true, true)));
            auto p = Pose::create(p1, handData->bases[i][1].dir(), handData->bases[i][1].up());
            bones[i][1]->setRelativePose(p, getParent());

            bones[i][2]->setMesh(OSGGeometry::create(makeCylinderGeo(l2, 0.0075, 8, true, true, true)));
            p = Pose::create(p2, handData->bases[i][2].dir(), handData->bases[i][2].up());
            bones[i][2]->setRelativePose(p, getParent());

            bones[i][3]->setMesh(OSGGeometry::create(makeCylinderGeo(l3, 0.0075, 8, true, true, true)));
            p = Pose::create(p3, handData->bases[i][3].dir(), handData->bases[i][3].up());
            bones[i][3]->setRelativePose(p, getParent());
        }

    }
}

void VRHandGeo::update(VRLeapFramePtr frame) {

    boost::mutex::scoped_lock lock(mutex);

    if (isLeft) {
        handData = frame->getLeftHand();
    } else {
        handData = frame->getRightHand();
    }
    if (!handData) {
        if (visible) {
            visible = false;
        }
    } else {
        if (!visible) visible = true;
    }

}

void VRHandGeo::setLeft() { isLeft = true; }

void VRHandGeo::setRight() { isLeft = false; }

