#include "VRDefShading.h"

#include <OpenSG/OSGDeferredShadingStage.h>
#include <OpenSG/OSGDirectionalLight.h>
#include <OpenSG/OSGPointLight.h>
#include <OpenSG/OSGSpotLight.h>
#include <OpenSG/OSGTextureObjChunk.h>
#include <OpenSG/OSGImage.h>

#include <OpenSG/OSGImageForeground.h>
#include <OpenSG/OSGPolygonForeground.h>
#include <OpenSG/OSGGeometry.h>
#include <OpenSG/OSGSimpleGeometry.h>
#include <OpenSG/OSGSimpleMaterial.h>
#include <OpenSG/OSGTextureObjChunk.h>

#include <OpenSG/OSGShaderProgramChunk.h>
#include <OpenSG/OSGShaderProgram.h>
#include <OpenSG/OSGShaderShadowMapEngine.h>
#include <OpenSG/OSGTrapezoidalShadowMapEngine.h>
#include <OpenSG/OSGProjectionCameraDecorator.h>

#include <OpenSG/OSGFrameBufferObject.h>
#include <OpenSG/OSGTextureBuffer.h>
#include <OpenSG/OSGRenderBuffer.h>

#include "core/utils/VROptions.h"
#include "core/utils/VRFunction.h"
#include "core/math/pose.h"
#include "core/objects/object/OSGCore.h"
#include "core/objects/VRLight.h"
#include "core/objects/VRLightBeacon.h"
#include "core/objects/VRCamera.h"
#include "core/objects/OSGCamera.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/material/VRTexture.h"
#include "core/scene/VRSceneManager.h"
#include "core/setup/windows/VRView.h"

#define WITH_PHOTOMETRIC

OSG_BEGIN_NAMESPACE;
using namespace std;

VRDefShading::VRDefShading() {
    defaultShadowType = ST_TRAPEZOID;
    shadowRes = 1024;
    shadowColor = 0.3;
}

VRDefShading::~VRDefShading() {}

void VRDefShading::init() {
    shadowMapWidth = shadowRes;
    shadowMapHeight = shadowRes;

    string resDir = VRSceneManager::get()->getOriginalWorkdir() + "/shader/DeferredShading/";
    dsGBufferVPFile = resDir + "DSGBuffer.vp.glsl";
    dsGBufferFPFile = resDir + "DSGBuffer.fp.glsl";
    dsAmbientVPFile = resDir + "DSAmbient.vp.glsl";
    dsAmbientFPFile = resDir + "DSAmbient.fp.glsl";
    dsDirLightVPFile = resDir + "DSDirLight.vp.glsl";
    dsDirLightFPFile = resDir + "DSDirLight.fp.glsl";
    dsDirLightShadowFPFile = resDir + "DSDirLightShadow.fp.glsl";
    dsPointLightVPFile = resDir + "DSPointLight.vp.glsl";
    dsPointLightFPFile = resDir + "DSPointLight.fp.glsl";
    dsPointLightShadowFPFile = resDir + "DSPointLightShadow.fp.glsl";
    dsPhotometricLightVPFile = resDir + "DSPointLight.vp.glsl";
    dsPhotometricLightFPFile = resDir + "DSPhotometricLight.fp.glsl";
    dsPhotometricLightShadowFPFile = resDir + "DSPhotometricLightShadow.fp.glsl";
    dsSpotLightVPFile = resDir + "DSSpotLight.vp.glsl";
    dsSpotLightFPFile = resDir + "DSSpotLight.fp.glsl";
    dsSpotLightShadowFPFile = resDir + "DSSpotLightShadow.fp.glsl";

    dsStage = DeferredShadingStage::create();
    dsStage->editMFPixelFormats()->push_back(Image::OSG_RGBA_PF          ); // positions (RGB) + ambient (A) term buffer
    dsStage->editMFPixelTypes  ()->push_back(Image::OSG_FLOAT32_IMAGEDATA);
    dsStage->editMFPixelFormats()->push_back(Image::OSG_RGBA_PF           ); // normals (RGB) + isLit (A) buffer
    dsStage->editMFPixelTypes  ()->push_back(Image::OSG_FLOAT32_IMAGEDATA);
    dsStage->editMFPixelFormats()->push_back(Image::OSG_RGB_PF); // diffuse (RGB) buffer
    dsStage->editMFPixelTypes  ()->push_back(Image::OSG_FLOAT32_IMAGEDATA);

    //dsStage->editMFPixelFormats()->push_back(Image::OSG_RGB_PF); // diffuse2 (RGB) buffer
    //dsStage->editMFPixelTypes  ()->push_back(Image::OSG_UINT8_IMAGEDATA);

    dsStage->setGBufferProgram(NULL);

    // ambient shader
    ShaderProgramMTRecPtr      vpAmbient = ShaderProgram::createVertexShader  ();
    ShaderProgramMTRecPtr      fpAmbient = ShaderProgram::createFragmentShader();
    ShaderProgramChunkMTRecPtr shAmbient = ShaderProgramChunk::create();
    vpAmbient->readProgram(dsAmbientVPFile.c_str());
    fpAmbient->readProgram(dsAmbientFPFile.c_str());
    fpAmbient->addUniformVariable<Int32>("texBufNorm", 1);
    shAmbient->addShader(vpAmbient);
    shAmbient->addShader(fpAmbient);
    dsStage->setAmbientProgram(shAmbient);

    // fbo -> TODO
    /*FrameBufferObjectRefPtr fbo = FrameBufferObject::create();
    ImageMTRecPtr img = Image::create();
    tex = TextureObjChunk::create();
    TextureBufferRefPtr texBuf = TextureBuffer::create();
    RenderBufferRefPtr depthBuf = RenderBuffer::create();

    fbo->editMFDrawBuffers()->push_back(GL_COLOR_ATTACHMENT0_EXT);
    fbo->setPostProcessOnDeactivate(true);
    dsStage->setRenderTarget(fbo);
    tex->setImage(img);
    texBuf->setTexture(tex);
    depthBuf->setInternalFormat(GL_DEPTH_COMPONENT24); // 16 24 32
    fbo->setColorAttachment(texBuf, 0);
    fbo->setDepthAttachment(depthBuf);

    img->set(Image::OSG_RGBA_PF, 1200, 800);
    fbo->setWidth (1200);
    fbo->setHeight(800);*/

    initiated = true;
}

int VRDefShading::addBuffer(int pformat, int ptype) {
    dsStage->editMFPixelFormats()->push_back(pformat);
    dsStage->editMFPixelTypes()->push_back(ptype);
    return dsStage->editMFPixelFormats()->size() - 1;
}

void VRDefShading::reload() {
    ShaderProgramMTRecPtr      vpAmbient = ShaderProgram::createVertexShader  ();
    ShaderProgramMTRecPtr      fpAmbient = ShaderProgram::createFragmentShader();
    ShaderProgramChunkMTRecPtr shAmbient = ShaderProgramChunk::create();
    vpAmbient->readProgram(dsAmbientVPFile.c_str());
    fpAmbient->readProgram(dsAmbientFPFile.c_str());
    shAmbient->addShader(vpAmbient);
    shAmbient->addShader(fpAmbient);
    dsStage->setAmbientProgram(shAmbient);

    for (auto li : lightInfos) {
        string vpFile = getLightVPFile(li.second.lightType);
        string fpFile = getLightFPFile(li.second.lightType, li.second.shadowType);
        li.second.lightVP->readProgram(vpFile.c_str());
        li.second.lightFP->readProgram(fpFile.c_str());
        li.second.lightFP->updateUniformVariable<Int32>("channel", channel);
        li.second.lightFP->updateUniformVariable<Int32>("isFirstLamp", 0);
    }

    if (lightInfos.size() > 0) {
        lightInfos.rbegin()->second.lightFP->updateUniformVariable<Int32>("isFirstLamp", 1);
    }
}

void VRDefShading::initDeferredShading(VRObjectPtr o) {
    init();
    stageObject = o;
    setDeferredShading(enabled);
}

void VRDefShading::setDeferredShading(bool b) {
    enabled = b;
    if (stageObject == 0) return;
    if (b) stageObject->setCore(OSGCore::create(dsStage), "defShading", true);
    else stageObject->setCore(OSGCore::create(Group::create()), "Object", true);
    for (auto li : lightInfos) {
        auto l = li.second.vrlight.lock();
        //if (b) l->setDeferred(b);
        l->setDeferred(b);
    }
}

bool VRDefShading::getDeferredShading() { return enabled; }

// channel : GL_RENDER GL_POSITION GL_NORMALIZE GL_DIFFUSE
void VRDefShading::setDeferredChannel(int c) { channel = c; reload(); }

void VRDefShading::setDSCamera(OSGCameraPtr cam) {
    if (initiated) dsStage->setCamera(cam->cam);
}

void VRDefShading::setBackground(BackgroundRecPtr bg) {
    if (initiated) dsStage->setBackground(bg);
}

void VRDefShading::addDSLight(VRLightPtr vrl) {
    if (!initiated) return;

    LightMTRecPtr light = vrl->getLightCore();
    string type = vrl->getLightType();
    bool shadows = vrl->getShadows();
    int ID = vrl->getID();

    LightInfo li;

    li.vrlight = vrl;
    li.lightVP = ShaderProgram     ::createVertexShader  ();
    li.lightFP = ShaderProgram     ::createFragmentShader();
    li.lightSH = ShaderProgramChunk::create              ();

    if (shadows) li.shadowType = defaultShadowType;
    else li.shadowType = ST_NONE;

    li.light = light;
    li.lightType = LightEngine::Point;
    if (type == "directional") li.lightType = LightEngine::Directional;
    if (type == "spot") li.lightType = LightEngine::Spot;
#ifdef WITH_PHOTOMETRIC
    if (type == "photometric") li.lightType = LightEngine::Photometric;
#endif // WITH_PHOTOMETRIC

    /**
    Not compiling? execute (with sudo!) the install script int the polyvr folder!
    sudo ./install
    */

    li.lightFP->addUniformVariable<Int32>("texBufPos",  0);
    li.lightFP->addUniformVariable<Int32>("texBufNorm", 1);
    li.lightFP->addUniformVariable<Int32>("texBufDiff", 2);
    li.lightFP->addUniformVariable<Int32>("texBufAmb",  3);
    li.lightFP->addUniformVariable<Int32>("texPhotometricMap", 4);
    li.lightFP->addUniformVariable<float>("shadowColor", shadowColor);
    li.lightFP->addUniformVariable<Vec3f>("lightUp", Vec3f(0,1,0));
    li.lightFP->addUniformVariable<Vec3f>("lightDirection", Vec3f(0,0,-1));
    li.lightFP->addUniformVariable<Int32>("channel", 0);
    li.lightFP->addUniformVariable<Int32>("isFirstLamp", 1);

    li.lightSH->addShader(li.lightVP);
    li.lightSH->addShader(li.lightFP);
    li.texChunk = TextureObjChunk::create();

    dsStage->editMFLights         ()->push_back(li.light  );
    dsStage->editMFLightPrograms  ()->push_back(li.lightSH);
    dsStage->editMFPhotometricMaps()->push_back(li.texChunk);

    auto tex = vrl->getPhotometricMap();
    if (tex) {
        li.texChunk->setImage(tex->getImage());
        li.texChunk->setInternalFormat(tex->getInternalFormat());
    }

    if (lightInfos.size() > 0) {
        lightInfos.rbegin()->second.lightFP->updateUniformVariable<Int32>("isFirstLamp", 0);
    }
    lightInfos[ID] = li;

    string vpFile = getLightVPFile(li.lightType);
    string fpFile = getLightFPFile(li.lightType, li.shadowType);
    li.lightVP->readProgram(vpFile.c_str());
    li.lightFP->readProgram(fpFile.c_str());
}

void VRDefShading::updateLight(VRLightPtr l) {
    int ID = l->getID();
    if (lightInfos.count(ID) == 0) return;
    auto& li = lightInfos[l->getID()];
    string type = l->getLightType();
    bool shadows = l->getShadows();

    li.lightType = LightEngine::Point;
    if (type == "directional") li.lightType = LightEngine::Directional;
    if (type == "spot") li.lightType = LightEngine::Spot;
#ifdef WITH_PHOTOMETRIC
    if (type == "photometric") li.lightType = LightEngine::Photometric;
#endif
    if (shadows) li.shadowType = defaultShadowType;
    else li.shadowType = ST_NONE;

    auto lItr = dsStage->editMFLights()->find(li.light);
    li.light = l->getLightCore();
    dsStage->editMFLights()->replace(lItr, li.light);
    dsStage->editMFLightPrograms();
    string vpFile = getLightVPFile(li.lightType);
    string fpFile = getLightFPFile(li.lightType, li.shadowType);
    li.lightVP->readProgram(vpFile.c_str());
    li.lightFP->readProgram(fpFile.c_str());

    auto tex = l->getPhotometricMap();
    if (tex) {
        dsStage->editMFPhotometricMaps();
        li.texChunk->setImage(tex->getImage());
    }
}

TextureObjChunkRefPtr VRDefShading::getTarget() { return fboTex; }

// file containing vertex shader code for the light type
const std::string& VRDefShading::getLightVPFile(LightTypeE lightType) {
    switch(lightType) {
        case LightEngine::Directional: return dsDirLightVPFile;
        case LightEngine::Point: return dsPointLightVPFile;
        case LightEngine::Spot: return dsSpotLightVPFile;
#ifdef WITH_PHOTOMETRIC
        case LightEngine::Photometric: return dsPhotometricLightVPFile;
#endif
        default: return dsUnknownFile;
    }
}

// file containing fragment shader code for the light type
const std::string& VRDefShading::getLightFPFile(LightTypeE lightType, ShadowTypeE shadowType) {
    bool ds = (shadowType != ST_NONE);
    switch(lightType) {
        case LightEngine::Directional: return ds ? dsDirLightShadowFPFile : dsDirLightFPFile;
        case LightEngine::Point: return ds ? dsPointLightShadowFPFile : dsPointLightFPFile;
        case LightEngine::Spot: return ds ? dsSpotLightShadowFPFile : dsSpotLightFPFile;
#ifdef WITH_PHOTOMETRIC
        case LightEngine::Photometric: return ds ? dsPhotometricLightShadowFPFile : dsPhotometricLightFPFile;
#endif
        default: return dsUnknownFile;
    }
}

void VRDefShading::subLight(int ID) {
    if (!lightInfos.count(ID)) return;
    auto& li = lightInfos[ID];
    auto lItr = dsStage->editMFLights()->find(li.light);
    auto lpItr = dsStage->editMFLightPrograms()->find(li.lightSH);
    auto pmItr = dsStage->editMFPhotometricMaps()->find(li.texChunk);
    dsStage->editMFLights()->erase(lItr);
    dsStage->editMFLightPrograms()->erase(lpItr);
    dsStage->editMFPhotometricMaps()->erase(pmItr);
    lightInfos.erase(ID);
}

OSG_END_NAMESPACE;
