#include "VRSceneManager.h"
#include "VRSceneLoader.h"
#include "core/setup/VRSetup.h"
#include "core/setup/windows/VRWindow.h"
#include "core/utils/VRRate.h"
#include "core/utils/VRProfiler.h"
#include "core/utils/coreDumpHandler.h"
#include "VRScene.h"
#include "core/objects/VRCamera.h"
#include "core/objects/VRLight.h"
#include "core/objects/VRLightBeacon.h"
#include "core/gui/VRGuiManager.h"
#include "core/utils/VRTimer.h"
#include "core/utils/VRGlobals.h"
#include "core/gui/VRGuiSignals.h"
#include "core/gui/VRGuiFile.h"
#include "core/utils/VRFunction.h"
#include "addons/Semantics/Reasoning/VROntology.h"
#include <OpenSG/OSGSceneFileHandler.h>
#include <gtkmm/main.h>
#include <GL/glut.h>
#include "core/utils/system/VRSystem.h"
#include "core/scene/VRProjectsList.h"
#include <boost/filesystem.hpp>
#include <time.h>

typedef boost::recursive_mutex::scoped_lock PLock;

OSG_BEGIN_NAMESPACE

VRSceneManager* main_instance = 0;

VRSceneManager::VRSceneManager() {
    cout << "Init VRSceneManager..";
    main_instance = this;
	original_workdir = boost::filesystem::current_path().string();
	examples = VRProjectsList::create();
	projects = VRProjectsList::create();
    searchExercisesAndFavorites();

    on_scene_load = VRSignal::create();
    on_scene_close = VRSignal::create();

    VROntology::setupLibrary();
    cout << " done" << endl;

    sceneUpdateCb = VRThreadCb::create( "update scene", boost::bind(&VRSceneManager::updateSceneThread, this, _1) );
    //initThread(sceneUpdateCb, "update scene", true, 1); // TODO
}

VRSceneManager::~VRSceneManager() {
    main_instance = 0;
    cout << "VRSceneManager::~VRSceneManager" << endl;
}

VRSceneManagerPtr VRSceneManager::create() { return VRSceneManagerPtr( new VRSceneManager()); }
VRSceneManager* VRSceneManager::get() { return main_instance; }

void VRSceneManager::loadScene(string path, bool write_protected, string encryptionKey) {
    if (!exists(path)) { cout << "loadScene " << path << " not found" << endl; return; }
    path = canonical(path);
    if (current) if (current->getPath() == path) return;
    cout << "loadScene " << path << endl;

    newEmptyScene(path);
    VRSceneLoader::get()->loadScene(path, encryptionKey);
    current->setFlag("write_protected", write_protected);
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
    if (auto pEntry = projects->getEntry(path)) {
        pEntry->setTimestamp(toString(time(0)));
        storeFavorites();
    }
}

string VRSceneManager::getOriginalWorkdir() { return original_workdir; }

void VRSceneManager::closeScene() {
    if (current == 0) return;
    VRProfiler::get()->setActive(false);
    on_scene_close->triggerPtr<VRDevice>();
    current = 0;

    auto setup = VRSetup::getCurrent();
    setup->resetViewports();
    setup->clearSignals();
    VRTransform::dynamicObjects.clear();

    for (auto w : setup->getWindows()) {
        cout << "VRSceneManager::closeScene!" << endl;
        w.second->setContent(false); // deactivate windows
        w.second->clear(Color3f(0.2,0.2,0.2)); // render last time
    }
    setWorkdir(original_workdir);
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
}

void VRSceneManager::setWorkdir(string path) {
	if (path == "") return;
	if (exists(path)) path = canonical(path);
	boost::filesystem::current_path(path);
	clearDumpFiles();
}

void VRSceneManager::newEmptyScene(string path) {
    closeScene();
    VRScenePtr scene = VRScenePtr( new VRScene() );
    VRSetup::getCurrent()->setupLESCCAVELights(scene);
    scene->setPath(path);
    setWorkdir(scene->getWorkdir());
    scene->setName(scene->getFileName());
    current = scene;
}

void VRSceneManager::newScene(string path) {
    if (exists(path)) path = canonical(path);
    newEmptyScene(path);

    VRLightPtr headlight = VRLight::create("light");
    headlight->setType("point");
    VRLightBeaconPtr headlight_B = VRLightBeacon::create("Headlight_beacon");
    headlight->setBeacon(headlight_B);
    current->add(headlight);

    VRCameraPtr cam = VRCamera::create("Default");
    cam->activate();
    headlight->addChild(cam);

    VRTransformPtr user;
    auto setup = VRSetup::getCurrent();
    if (setup) user = setup->getUser();
    if (user) user->addChild(headlight_B);
    else cam->addChild(headlight_B);

    cam->setFrom(Vec3d(3,2,3));
    cam->setAt(Vec3d(0,0,0));

    string initScript = "\timport VR\n\n\tif hasattr(VR, 'scene'): VR.scene.destroy()\n\tVR.scene = VR.Object('scene', 'light')\n\n\t";
    current->newScript("init", initScript);
    setScene(current);
}

VRSignalPtr VRSceneManager::getSignal_on_scene_load() { return on_scene_load; }
VRSignalPtr VRSceneManager::getSignal_on_scene_close() { return on_scene_close; }

void VRSceneManager::setScene(VRScenePtr scene) {
    if (!scene) return;
    current = scene;
    VRSetup::getCurrent()->setScene(scene);
    scene->setActiveCamera();
    VRProfiler::get()->setActive(true);

    on_scene_load->triggerPtr<VRDevice>();
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
}

void VRSceneManager::storeFavorites() {
    string path = original_workdir + "/examples/.config";
    projects->saveToFile(path);
}

void VRSceneManager::addFavorite(string path) {
    if (projects->hasEntry(path)) return;
    projects->addEntry( VRProjectEntry::create(path, ""));
    storeFavorites();
}

void VRSceneManager::remFavorite(string path) {
    projects->remEntry(path);
    storeFavorites();
}

void VRSceneManager::searchExercisesAndFavorites() {
    projects->clear();
    examples->clear();

    // examples
	vector<string> files = VRGuiFile::listDir("examples");
	for (string file : files) {
		int N = file.size(); if (N < 6) continue;

		string ending = file.substr(N - 4, N - 1);
		if (ending != ".xml" && ending != ".pvr") continue;

		string path = canonical("examples/" + file);
		examples->addEntry( VRProjectEntry::create(path, "") );
	}

    // favorites
    ifstream file( "examples/.cfg" ); // check for old config file
    if (file.is_open()) {
        string line, path;
        while ( getline (file,line) ) {
            ifstream f(line.c_str());
            if (!f.good()) continue;
            f.close();
            line = canonical(line);
            projects->addEntry( VRProjectEntry::create(line, "") );
        }
        file.close();
        storeFavorites();
        remove("examples/.cfg"); // remove old config file
        return;
    }

    projects->loadFromFile("examples/.config");
}

VRProjectsListPtr VRSceneManager::getFavoritePaths() { return projects; }
VRProjectsListPtr VRSceneManager::getExamplePaths() { return examples; }

VRScenePtr VRSceneManager::getCurrent() { return current; }

void VRSceneManager::updateSceneThread(VRThreadWeakPtr tw) {
    updateScene();
    sleep(1);
}

void VRSceneManager::updateScene() {
    if (!current) return;
    VRSetup::getCurrent()->updateActivatedSignals();
    current->update();
}

void VRSceneManager::update() {
    // statistics
    VRProfiler::get()->swap();
    static VRRate FPS; int fps = FPS.getRate();
    VRTimer timer; timer.start();

    VRTimer t1; t1.start();
    VRGuiManager::get()->updateGtk(); // update GUI
    VRGlobals::GTK1_FRAME_RATE.update(t1);
    VRGlobals::UPDATE_LOOP1.update(timer);

    VRTimer t4; t4.start();
    updateCallbacks();
    VRGlobals::SMCALLBACKS_FRAME_RATE.update(t4);
    VRGlobals::UPDATE_LOOP2.update(timer);

    VRTimer t5; t5.start();
    if (auto setup = VRSetup::getCurrent()) {
        setup->updateTracking(); // tracking
        setup->updateDevices(); // device beacon update
    }

    VRGlobals::SMCALLBACKS_FRAME_RATE.update(t5);
    VRGlobals::UPDATE_LOOP3.update(timer);

    VRTimer t6; t6.start();
    updateScene();
    VRGlobals::SCRIPTS_FRAME_RATE.update(t6);
    VRGlobals::UPDATE_LOOP4.update(timer);

    if (auto setup = VRSetup::getCurrent()) {
        VRTimer t2; t2.start();
        setup->updateWindows(); // rendering
        setup.reset(); // updateGtk may close application, reset setup to avoid memory leak
        VRGlobals::WINDOWS_FRAME_RATE.update(t2);
        VRGlobals::UPDATE_LOOP5.update(timer);
        VRGuiManager::get()->updateGtk();
        VRGlobals::UPDATE_LOOP6.update(timer);
    }

    // sleep
    if (current) current->allowScriptThreads();
    VRGlobals::CURRENT_FRAME++;
    VRGlobals::FRAME_RATE.fps = fps;
    VRTimer t7; t7.start();
    osgSleep(max(16-timer.stop(),0));
    VRGlobals::SLEEP_FRAME_RATE.update(t7);
    VRGlobals::UPDATE_LOOP7.update(timer);
    if (current) current->blockScriptThreads();
}

OSG_END_NAMESPACE
