#include "VRTextureMosaic.h"

using namespace OSG;

void VRTextureMosaic::add(VRTexturePtr tex, Vec2i pos) {
    auto s = getSize();
    if (s[0] == 0) {
        setImage( tex->getImage() );
        return;
    }

    resize( Vec3i(pos[0]+tex->getSize()[0], max(pos[1]+tex->getSize()[1], s[1]), 1), Vec3i() );
    paste(tex, Vec3i(pos[0], pos[1], 0) );
}

VRTextureMosaic::VRTextureMosaic() {}
VRTextureMosaic::~VRTextureMosaic() {}

VRTextureMosaicPtr VRTextureMosaic::create() { return shared_ptr<VRTextureMosaic>(new VRTextureMosaic() ); }
VRTextureMosaicPtr VRTextureMosaic::ptr() { return static_pointer_cast<VRTextureMosaic>( shared_from_this() ); }
