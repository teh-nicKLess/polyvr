#ifndef VRSTORAGE_H_INCLUDED
#define VRSTORAGE_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include <map>
#include <unordered_map>
#include <vector>
#include "VRFunctionFwd.h"
#include "core/objects/VRObjectFwd.h"

namespace xmlpp{ class Element; }

ptrFctFwd(VRStore, xmlpp::Element*);
ptrFctFwd(VRStorageFactory, OSG::VRStoragePtr&);
ptrFctFwd(VRStorage, OSG::VRStorageContextPtr&);

OSG_BEGIN_NAMESPACE;
using namespace std;

struct VRStorageBin {
    VRStoreCbPtr f1; // load
    VRStoreCbPtr f2; // save
};

class VRStorageContext {
    public:
        bool onlyReload = false;

        static VRStorageContextPtr create(bool onlyReload);
};

class VRStorage {
    private:
        string type = "Node";
        int persistency = 666;
        bool overrideCallbacks = false;
        vector<VRStorageCbPtr> f_setup_before; // called before loading
        vector<VRStorageCbPtr> f_setup; // called after loading
        vector<VRUpdateCbPtr> f_setup_after; // setup after tree loaded
        map<string, VRStorageBin> storage;
        static map<string, VRStorageFactoryCbPtr> factory;

        template<class T> static void typeFactoryCb(VRStoragePtr& s);

        void save_strstr_map_cb(map<string, string>* t, string tag, xmlpp::Element* e);
        void load_strstr_map_cb(map<string, string>* t, string tag, xmlpp::Element* e);

        template<typename T> void save_cb(T* t, string tag, xmlpp::Element* e);
        template<typename T> void save_on_cb(T* t, string tag, xmlpp::Element* e);
        template<typename T> void load_cb(T* t, string tag, xmlpp::Element* e);
        template<typename T> void save_vec_cb(vector<T>* v, string tag, xmlpp::Element* e);
        template<typename T> void load_vec_cb(vector<T>* v, string tag, xmlpp::Element* e);
        template<typename T> void save_vec_vec_cb(vector<vector<T>>* v, string tag, xmlpp::Element* e);
        template<typename T> void load_vec_vec_cb(vector<vector<T>>* v, string tag, xmlpp::Element* e);
        template<typename T> void save_vec_on_cb(vector<T>* v, string tag, xmlpp::Element* e);
        template<typename T> void save_obj_vec_cb(vector<std::shared_ptr<T> >* v, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_obj_vec_cb(vector<std::shared_ptr<T> >* v, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_obj_cb(std::shared_ptr<T>* v, string tag, xmlpp::Element* e);
        template<typename T> void load_obj_cb(std::shared_ptr<T>* v, string tag, xmlpp::Element* e);

        template<typename T> void save_str_map_cb(map<string, T*>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_int_map_cb(map<int, T*>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_int_map2_cb(map<int, T>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_str_objmap_cb(map<string, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_int_objmap_cb(map<int, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_str_objumap_cb(unordered_map<string, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void save_int_objumap_cb(unordered_map<int, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_str_map_cb(map<string, T*>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_int_map_cb(map<int, T*>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_int_map2_cb(map<int, T>* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_str_objmap_cb(map<string, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_int_objmap_cb(map<int, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_str_objumap_cb(unordered_map<string, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);
        template<typename T> void load_int_objumap_cb(unordered_map<int, std::shared_ptr<T> >* mt, string tag, bool under, xmlpp::Element* e);

    public:

        void storeMap(string tag, map<string, string>& m);
        template<typename T> void store(string tag, T* t);
        template<typename T> void storeVec(string tag, vector<T>& v);
        template<typename T> void storeVecVec(string tag, vector<vector<T>>& v);
        template<typename T> void storeObj(string tag, std::shared_ptr<T>& o);
        template<typename T> void storeObjVec(string tag, vector<std::shared_ptr<T> >& v, bool under = false);
        template<typename T> void storeMap(string tag, map<string, T*>* mt, bool under = false);
        template<typename T> void storeMap(string tag, map<string, std::shared_ptr<T> >* mt, bool under = false);
        template<typename T> void storeMap(string tag, map<int, T>& mt);
        template<typename T> void storeMap(string tag, map<int, T*>* mt, bool under = false);
        template<typename T> void storeMap(string tag, map<int, std::shared_ptr<T> >* mt, bool under = false);
        template<typename T> void storeMap(string tag, unordered_map<string, std::shared_ptr<T> >* mt, bool under = false);
        template<typename T> void storeMap(string tag, unordered_map<int, std::shared_ptr<T> >* mt, bool under = false);
        template<typename T> void storeObjName(string tag, T* o, string* t);
        template<typename T> void storeObjNames(string tag, vector<T>* o, vector<string>* t);

        void setStorageType(string t);
        void regStorageSetupBeforeFkt(VRStorageCbPtr u);
        void regStorageSetupFkt(VRStorageCbPtr u);
        void regStorageSetupAfterFkt(VRUpdateCbPtr u);

        static xmlpp::Element* getChild(xmlpp::Element* e, string c);
        static xmlpp::Element* getChildI(xmlpp::Element* e, int i);
        static vector<xmlpp::Element*> getChildren(xmlpp::Element* e);

    public:
        VRStorage();
        ~VRStorage();

        virtual void save(xmlpp::Element* e, int p = 0);
        virtual void load(xmlpp::Element* e, VRStorageContextPtr context = 0);
        xmlpp::Element* saveUnder(xmlpp::Element* e, int p = 0, string t = "");
        xmlpp::Element* loadChildFrom(xmlpp::Element* e, string t = "", VRStorageContextPtr context = 0);
        xmlpp::Element* loadChildIFrom(xmlpp::Element* e, int i, VRStorageContextPtr context = 0);

        static int getPersistency(xmlpp::Element* e);
        static VRStoragePtr createFromStore(xmlpp::Element* e, bool verbose = true);
        template<class T> static void regStorageType(string t);

        void setPersistency(int p);
        int getPersistency();
        void setOverrideCallbacks(bool b);

        string getDescription();

        // only single object
        bool saveToFile(string path, bool createDirs = true);
        bool loadFromFile(string path, VRStorageContextPtr context = 0);
};

OSG_END_NAMESPACE;

#endif // VRSTORAGE_H_INCLUDED
