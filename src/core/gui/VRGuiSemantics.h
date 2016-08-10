#ifndef VRGUISEMANTICS_H_INCLUDED
#define VRGUISEMANTICS_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include "core/scene/VRSceneManager.h"
#include "addons/Semantics/VRSemanticsFwd.h"
#include "VRGuiSignals.h"

#include "gtkmm/treemodel.h"

namespace Gtk {
    class Fixed;
    class Widget;
    class Label;
    class TreeView;
    class Separator;
}

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRGuiSemantics {
    private:
        struct ConceptWidget {
            Gtk::Widget* widget;
            Gtk::Label* label;
            Gtk::TreeView* treeview;
            VRConceptPtr concept;

            ConceptWidget(VRConceptPtr concept = 0);

            void on_select();
            void on_select_property();

            void setPropRow(Gtk::TreeModel::iterator iter, string name, string type, string color, int flag);
        };

        struct ConnectorWidget {
            Gtk::Separator* s1 = 0;
            Gtk::Separator* s2 = 0;
            Gtk::Separator* s3 = 0;
            Gtk::Fixed* canvas = 0;

            ConnectorWidget(Gtk::Fixed* canvas = 0);

            void set(int x1, int y1, int x2, int y2);
        };

        typedef shared_ptr<ConceptWidget> ConceptWidgetPtr;
        typedef shared_ptr<ConnectorWidget> ConnectorWidgetPtr;

        Gtk::Fixed* canvas = 0;
        map<string, ConceptWidgetPtr> concepts;
        map<string, ConnectorWidgetPtr> connectors;

        void on_new_clicked();
        void on_del_clicked();

        void on_treeview_select();
        void on_property_treeview_select();

        void clearCanvas();
        void drawCanvas(string name);

    public:
        VRGuiSemantics();

        void update();
};

OSG_END_NAMESPACE

#endif // VRGUISEMANTICS_H_INCLUDED