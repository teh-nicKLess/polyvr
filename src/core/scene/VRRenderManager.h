#ifndef VRRENDERMANAGER_H_INCLUDED
#define VRRENDERMANAGER_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include "core/objects/VRObjectFwd.h"
#include "core/setup/VRSetupFwd.h"
#include "core/utils/VRStorage.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRRenderManager : public VRStorage {
    private:
        bool frustumCulling = true;
        bool occlusionCulling = false;
        bool twoSided = true;

        bool deferredRendering = false;
        bool do_ssao = false;
        bool calib = false;
        bool do_hmdd = false;
        bool do_marker = false;
        int ssao_kernel = 4;
        int ssao_noise = 4;
        float ssao_radius = 0.02;

    protected:
        VRObjectPtr root = 0;

        vector<VRRenderStudioPtr> getRenderings();

    public:
        VRRenderManager();
        ~VRRenderManager();

        void addLight(VRLightPtr l);
        void setDSCamera(VRCameraPtr cam);

        void setFrustumCulling(bool b);
        void setOcclusionCulling(bool b);
        void setTwoSided(bool b);
        bool getFrustumCulling();
        bool getOcclusionCulling();
        bool getTwoSided();

        bool getDefferedShading();
        bool getSSAO();
        bool getHMDD();
        bool getMarker();

        void setDeferredShading(bool b);
        void setDeferredChannel(int channel);
        void setSSAO(bool b);
        void setSSAOradius(float r);
        void setSSAOkernel(int k);
        void setSSAOnoise(int n);
        void setCalib(bool b);
        void setHMDD(bool b);
        void setMarker(bool b);

        void update();
};

OSG_END_NAMESPACE;

#endif // VRRENDERMANAGER_H_INCLUDED
