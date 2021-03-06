#ifndef VRGUITREEEXPLORER_H_INCLUDED
#define VRGUITREEEXPLORER_H_INCLUDED

#include "VRGuiFwd.h"
#include "core/utils/VRFunctionFwd.h"
#include <OpenSG/OSGConfig.h>
#include <gtkmm.h>
#include <gtkmm/treemodel.h>

using namespace std;
OSG_BEGIN_NAMESPACE;

class VRGuiTreeExplorer {
    private:
        Gtk::WindowPtr win;
        Gtk::TextView* m_TextView;
        Gtk::TreeView* m_TreeView;
        Gtk::Entry* searchEntry;
        string cols;

        shared_ptr< VRFunction<VRGuiTreeExplorer*> > cb;
        string info;

        Glib::RefPtr<Gtk::TreeStore> m_refTreeModel;
        Glib::RefPtr<Gtk::TextBuffer> infoBuffer;
        map<int, Gtk::TreeModel::iterator> rows;
        Gtk::TreeModel::iterator selected;

        void on_search_edited();
        void on_row_select();

    public:
        VRGuiTreeExplorer(string cols, string title);
        ~VRGuiTreeExplorer();

        static VRGuiTreeExplorerPtr create(string cols, string title);

        int add(int parent, int N, ...);

        void move(int, int);
        void remove(int);

        void setInfo(string s);
        void setSelectCallback( shared_ptr< VRFunction<VRGuiTreeExplorer*> > cb );

        Gtk::TreeModel::iterator getSelected();
        template<class T> T get(Gtk::TreeModel::iterator itr, int i);
};

OSG_END_NAMESPACE;

#endif // VRGUITREEEXPLORER_H_INCLUDED
