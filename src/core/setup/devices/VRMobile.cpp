#include "VRMobile.h"
#include "VRSignal.h"
#include "core/utils/VRFunction.h"
#include "core/utils/toString.h"
#include "core/networking/VRNetworkManager.h"
#include "core/networking/VRSocket.h"
#include "core/objects/VRTransform.h"
#include "addons/CEF/CEF.h"
//#include "addons/WebKit/VRPyWebKit.h"
#include <boost/bind.hpp>

OSG_BEGIN_NAMESPACE;
using namespace std;

void VRMobile::callback(HTTP_args* args) { // TODO: implement generic button trigger of device etc..
    //args->print();

    if (args->params->count("button") == 0) return;
    if (args->params->count("state") == 0) return;

    if (args->params->count("message")) setMessage((*args->params)["message"]);

    int button = toInt((*args->params)["button"]);
    int state = toInt((*args->params)["state"]);

    change_button(button, state);
}

VRMobile::VRMobile() : VRDevice("mobile") {
    //enableAvatar("cone");
    //enableAvatar("ray");
    clearSignals();

    soc = new VRSocket("Mobile_server");
    cb = new VRHTTP_cb( "Mobile_server_callback", boost::bind(&VRMobile::callback, this, _1) );
    soc->setCallback(cb);
    soc->setType("http receive");
    setPort(5500);
}

void VRMobile::clearSignals() {
    VRDevice::clearSignals();

    addSignal( 0, 0)->add( getDrop() );
    addSignal( 0, 1)->add( addDrag( getBeacon(), 0) );
}

void VRMobile::setPort(int port) { this->port = port; soc->setPort(port); }
int VRMobile::getPort() { return port; }

void VRMobile::updateMobilePage() {
    string page = "<html><body>";
    for (auto w : websites) page += "<a href=\"" + w.first + "\">" + w.first + "</a>";
    page += "</body></html>";
    soc->addHTTPPage(getName(), page);
}

void VRMobile::addWebSite(string uri, string website) {
    websites[uri] = website;
    soc->addHTTPPage(uri, website);
    updateMobilePage();
}

void VRMobile::remWebSite(string uri) {
    if (websites.count(uri)) websites.erase(uri);
    soc->remHTTPPage(uri);
    updateMobilePage();
}

void VRMobile::updateClients(string path) { CEF::reload(path); }

OSG_END_NAMESPACE;
