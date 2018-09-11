#include "VRRoadIntersection.h"
#include "VRRoadNetwork.h"
#include "VRRoad.h"
#include "../traffic/VRTrafficLights.h"
#include "../VRWorldGenerator.h"
#include "../terrain/VRTerrain.h"
#include "core/utils/toString.h"
#include "core/math/polygon.h"
#include "core/math/triangulator.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/scene/VRObjectManager.h"
#include "addons/Semantics/Reasoning/VREntity.h"
#include "addons/Semantics/Reasoning/VRProperty.h"

using namespace OSG;


VRRoadIntersection::RoadFront::RoadFront(VRRoadPtr road) : road(road) {}


VRRoadIntersection::VRRoadIntersection() : VRRoadBase("RoadIntersection") {}
VRRoadIntersection::~VRRoadIntersection() {}

VRRoadIntersectionPtr VRRoadIntersection::create() {
    auto i = VRRoadIntersectionPtr( new VRRoadIntersection() );
    i->hide("SHADOW");
    return i;
}

void VRRoadIntersection::computeLanes(GraphPtr graph) {
    auto w = world.lock();
    if (!w) return;
    auto roads = w->getRoadNetwork();

	VREntityPtr node = entity->getEntity("node");
	if (!node) { cout << "Warning in VRRoadNetwork::computeIntersectionLanes, intersection node is NULL!" << endl; return; }
	string iN = entity->getName();
	string nN = node->getName();

	//inLanes.clear();
	//outLanes.clear();
	laneMatches.clear();
	nextLanes.clear();

	auto getInAndOutLanes = [&]() {
        for (auto roadFront : roadFronts) {
            auto road = roadFront->road;
            VREntityPtr roadEntry = road->getNodeEntry(node);
            int reSign = toInt( roadEntry->get("sign")->value );
            roadFront->dir = reSign;
            for (VREntityPtr lane : road->getEntity()->getAllEntities("lanes")) {
                if (!lane->is_a("Lane")) continue;
                int direction = toInt( lane->get("direction")->value );
                if (direction*reSign == 1) roadFront->inLanes.push_back(lane);
                if (direction*reSign == -1) roadFront->outLanes.push_back(lane);
            }
        }
	};

	auto computeLaneMatches = [&]() {
	    auto checkDefaultMatch = [&](int i, int j, int Nin, int Nout, int reSignIn, int reSignOut, VRRoadPtr road1, VRRoadPtr road2) {
            if (Nin == Nout && i != j && reSignIn != reSignOut) return false;
            if (Nin == Nout && i != Nout-j-1 && reSignIn == reSignOut) return false;
            if (Nin > Nout) {
                auto getRoadConnectionAngle = [&](VRRoadPtr road1, VRRoadPtr road2) {
                    auto& data1 = road1->getEdgePoint( node );
                    auto& data2 = road2->getEdgePoint( node );
                    return data1.n.dot(data2.n);
                };
                auto getRoadTurnLeft = [&](VRRoadPtr road1, VRRoadPtr road2) {
                    auto& data1 = road1->getEdgePoint( node );
                    auto& data2 = road2->getEdgePoint( node );
                    Vec3d w = data1.n.cross(data2.n);
                    float a = asin( w.length() );
                    if (w[1] < 0) a = -a;
                    return a;
                };
                bool parallel  = bool( getRoadConnectionAngle(road1, road2) < -0.8 );
                bool left  = bool( getRoadTurnLeft(road1, road2) < 0);

                if (reSignIn<0) i=Nin-i-1;  //making sure indexes stay consistent, independent of road-direction
                if (reSignOut<0) j=Nout-j-1;
                if (parallel && Nin<=Nout+1 && i!=Nout-j-1) return false; //matching way-through
                if (parallel &&  Nin>Nout+1 && i!=Nout-j) return false; //matching way-through
                if (!parallel && (i!=Nin-1 || j!=0 || !left) && ((i!=0 || j!=Nout-1) || left)) return false; //one left turn, one right turn
                if (!parallel && i>0 && i<Nin-1) return false; //everything not being most left or most right lane
            }
            if (Nin == 1 || Nout == 1) return true;
            return true;
        };

	    auto checkContinuationMatch = [](int i, int j, int Nin, int Nout, int reSignIn, int reSignOut) {
	        if (Nout == Nin && i == j) return true;
	        //if (Nout > Nin && i == j-1) return true;
	        //if (Nout > Nin && i< Nin/2 && j< Nout/2 && i == Nout-j-1) return true; // TODO
            //if (Nout > Nin && i>= Nin/2 && j> Nout/2 && i == j) return true;
            if (Nout > Nin && reSignIn<0 && i == j) return true;
            if (Nout > Nin && reSignIn>0 && i == j-1) return true;
	        if (Nout < Nin && Nin-i-1 == j) return true; // TODO
            //if (Nin == Nout && i != j && reSignIn != reSignOut) return false;
            //if (Nin == Nout && i != Nout-j-1 && reSignIn == reSignOut) return false;
            //if (Nin == 1 || Nout == 1) return true;
            return false;
        };

        auto checkForkMatch = [&](int i, int j, int Nin, int Nout, int reSignIn, int reSignOut, VRRoadPtr road1, VRRoadPtr road2) {
            auto getRoadConnectionAngle = [&](VRRoadPtr road1, VRRoadPtr road2) {
                auto& data1 = road1->getEdgePoint( node );
                auto& data2 = road2->getEdgePoint( node );
                return data1.n.dot(data2.n);
            };
            bool parallel  = bool( getRoadConnectionAngle(road1, road2) < -0.5 );
            if (parallel && Nin == Nout) { ///CaseA - eg 4in 4out
                if (i == Nout-j-1 && reSignIn==reSignOut) {return true;}
                if (i == j && reSignIn != reSignOut) {return true;}
            }
            if (parallel && Nin != Nout){ ///CaseB - eg 2in 3out
                auto getRoadTurnLeft = [&](VRRoadPtr road1, VRRoadPtr road2) {
                    auto& data1 = road1->getEdgePoint( node );
                    auto& data2 = road2->getEdgePoint( node );
                    Vec3d w = data1.n.cross(data2.n);
                    float a = asin( w.length() );
                    if (w[1] < 0) a = -a;
                    return a;
                };
                bool left  = bool( getRoadTurnLeft(road1, road2) < 0);
                //if (reSignIn<0) left = !left;

                if (reSignIn<0) i=Nin-i-1;  //making sure indexes stay consistent, independent of road-direction
                if (reSignOut<0) j=Nout-j-1;
                Vec3d X = node->getVec3("position");

                if (Nin < Nout &&  left && i == j) { return true; }
                if (Nin < Nout && !left && i == Nout-j-1) { return true; }
                if (Nin > Nout &&  left && j == Nin-i-1) { return true; }
                if (Nin > Nout && !left && i == j) { return true; }
            }
            if (Nout == 1 || Nin == 1){
                //return true;
            }
            //match case A - split ways one left one right
            //match case B - split both ways, two left two right
            //cout << toString(node->getValue<int>("graphID", -1)) << endl;
            return false;
        };

        for (auto roadFront1 : roadFronts) {
            for (auto roadFront2 : roadFronts) {
                if (roadFront1 == roadFront2) continue;
                auto road1 = roadFront1->road;
                auto road2 = roadFront2->road;
                int reSign1 = roadFront1->dir;
                int reSign2 = roadFront2->dir;
                int Nin = roadFront1->inLanes.size();
                int Nout = roadFront2->outLanes.size();
                for (int i=0; i<Nin; i++) {
                    auto laneIn = roadFront1->inLanes[i];
                    for (int j=0; j<Nout; j++) {
                        auto laneOut = roadFront2->outLanes[j];
                        bool match = false;
                        switch (type) {
                            case CONTINUATION: match = checkContinuationMatch(i,j,Nin, Nout, reSign1, reSign2); break;
                            case FORK: match = checkForkMatch(i,j,Nin, Nout, reSign1, reSign2, road1, road2); break;
                            //case MERGE: break;
                            //case UPLINK: break;
                            default: match = checkDefaultMatch(i,j,Nin, Nout, reSign1, reSign2, road1, road2); break;
                        }
                        if (match) laneMatches.push_back(make_pair(laneIn, laneOut));
                    }
                }
            }
        }
	};

	auto mergeMatchingLanes = [&]() { ///CONTINUATION
	    map<VREntityPtr, Vec3d> displacementsA; // map roads to displace!
	    map<VREntityPtr, Vec3d> displacementsB; // map roads to displace!
	    map<VREntityPtr, bool> processedLanes; // keep list of already processed lanes
        for (auto match : laneMatches) {
            auto laneIn = match.first;
            auto laneOut = match.second;
            auto roadIn = laneIn->getEntity("road");
            auto roadOut = laneOut->getEntity("road");
            int Nin = roadIn->getAllEntities("lanes").size();
            int Nout = roadOut->getAllEntities("lanes").size();
            auto nodes1 = laneIn->getEntity("path")->getAllEntities("nodes");
            auto nodes2 = laneOut->getEntity("path")->getAllEntities("nodes");
            VREntityPtr nodeEnt1 = *nodes1.rbegin();
            VREntityPtr nodeEnt2 = nodes2[0];
            Vec3d X = nodeEnt2->getEntity("node")->getVec3("position") - nodeEnt1->getEntity("node")->getVec3("position");
            float D = X.length();

            if (Nin >= Nout) {
                auto node1 = nodes1[nodes1.size()-2]->getEntity("node");
                auto norm1 = nodes1[nodes1.size()-2]->getVec3("direction");
                auto node2 = nodeEnt2->getEntity("node");
                auto norm2 = nodeEnt2->getVec3("direction");
                auto name2 = node2->getName();

                auto nodeToDelete = nodeEnt1->getEntity("node");
                auto tempID = nodeToDelete->getValue<int>("graphID", -1);
                //cout << tempID << " node removed -- NIN>NOUT" << endl;

                nodeEnt1->set("node", name2);
                auto rGraph = roads->getGraph();
                if (D > 0) displacementsB[roadIn] = X;
                roads->connectGraph({node1,node2}, {norm1,norm2}, laneIn);
                rGraph->remNode(tempID);
            }
            if (Nin < Nout) {
                auto node1 = nodeEnt1->getEntity("node");
                auto norm1 = nodeEnt1->getVec3("direction");
                auto node2 = nodes2[1]->getEntity("node");
                auto norm2 = nodes2[1]->getVec3("direction");

                auto nodeToDelete = nodeEnt2->getEntity("node");
                auto tempID=nodeToDelete->getValue<int>("graphID", -1);
                //cout << tempID << " node removed -- NIN<NOUT" << endl;

                nodeEnt2->set("node", node1->getName());

                if (D > 0) displacementsA[roadOut] = -X;
                auto rGraph = roads->getGraph();
                roads->connectGraph({node1,node2}, {norm1,norm2}, laneOut);
                rGraph->remNode(tempID);
            }

            processedLanes[laneIn] = true;
            processedLanes[laneOut] = true;
        }
        auto graph = roads->getGraph();

        //if (displacementsA.size() == 0 && displacementsB.size() == 0) return;
        int n = 0;
        vector<vector<int>> inl;
        vector<vector<int>> oul;
        for (auto rfront : roadFronts) {// shift whole road fronts!
            auto road = rfront->road;
            auto rEnt = road->getEntity();
            //if (!displacementsA.count(rEnt) || !displacementsB.count(rEnt)) continue;
            Vec3d X1 = displacementsA[rEnt];
            Vec3d X2 = displacementsB[rEnt];
            float offsetter1 = X1.dot(rfront->pose.x())*rfront->dir;
            float offsetter2 = X2.dot(rfront->pose.x())*rfront->dir;
            if (rfront->dir>0) {
                road->setOffsetOut(offsetter2);
            } else {
                road->setOffsetIn(offsetter1);
            }
            n++;
            vector<int> inIDs;
            vector<int> ouIDs;
            for (auto laneIn : rfront->inLanes) {
                auto nodes = laneIn->getEntity("path")->getAllEntities("nodes");
                auto node1 = nodes[nodes.size()-2]->getEntity("node");
                auto node2 = nodes[nodes.size()-1]->getEntity("node");
                auto edgeID = graph->getEdgeID(node1->getValue<int>("graphID", -1),node2->getValue<int>("graphID", -1));
                inIDs.push_back(edgeID);
                //cout << "---inID " << toString(edgeID) << endl;

                if (processedLanes.count(laneIn)) continue;
                //auto nodes = laneIn->getEntity("path")->getAllEntities("nodes");
                VREntityPtr node = (*nodes.rbegin())->getEntity("node");
                auto p = node->getVec3("position");
                p += X2;
                node->setVec3("position", p, "Position");
                graph->setPosition(node->getValue<int>("graphID", 0), Pose::create(p));
            }

            for (auto laneOut : rfront->outLanes) {
                auto nodes = laneOut->getEntity("path")->getAllEntities("nodes");
                auto node1 = nodes[0]->getEntity("node");
                auto node2 = nodes[1]->getEntity("node");
                auto edgeID = graph->getEdgeID(node1->getValue<int>("graphID", -1),node2->getValue<int>("graphID", -1));
                ouIDs.push_back(edgeID);
                //cout << "---ouIDs " << toString(edgeID) << endl;

                if (processedLanes.count(laneOut)) continue;
                VREntityPtr node = laneOut->getEntity("path")->getAllEntities("nodes")[0]->getEntity("node");
                auto p = node->getVec3("position");
                p += X1;
                node->setVec3("position", p, "Position");
                graph->setPosition(node->getValue<int>("graphID", 0), Pose::create(p));
            }
            inl.push_back(inIDs);
            oul.push_back(ouIDs);
        }
        for (int i = 0; i < inl.size(); i++) {
            if (inl[i].size()>1) graph->getEdge(inl[i][0]).relations.clear();
            for (int k = 1; k < inl[i].size(); k++) {
                graph->getEdge(inl[i][k]).relations.clear();
                graph->addRelation(inl[i][k],inl[i][k-1]);
                //cout << "rel: in " << toString(i) << " " << toString(inl[i][k]) << " -- " << toString(inl[i][k-1]) << endl;
            }
        }
        for (int i = 0; i < oul.size(); i++) {
            if (oul[i].size()>1) graph->getEdge(oul[i][0]).relations.clear();
            for (int k = 1; k < oul[i].size(); k++) {
                graph->getEdge(oul[i][k]).relations.clear();
                graph->addRelation(oul[i][k],oul[i][k-1]);
                //cout << "rel: out " << toString(i) << " " << toString(oul[i][k]) << " -- " << toString(oul[i][k-1]) << endl;
            }
        }
	};

	auto bridgeMatchingLanes = [&]() { ///DEFAULT
        for (auto match : laneMatches) {
            auto laneIn = match.first;
            auto laneOut = match.second;

            float width = laneIn->getValue<float>("width", 0.5);
            bool pedestrianIn = laneIn->getValue<bool>("pedestrian", false);
            auto nodes1 = laneIn->getEntity("path")->getAllEntities("nodes");
            VREntityPtr node1 = *nodes1.rbegin();

            bool pedestrianOut = laneOut->getValue<bool>("pedestrian", false);
            auto node2 = laneOut->getEntity("path")->getAllEntities("nodes")[0];
            auto lane = addLane(1, width, pedestrianIn || pedestrianOut);
            auto nodes = { node1->getEntity("node"), node2->getEntity("node") };
            auto norms = { node1->getVec3("direction"), node2->getVec3("direction") };
            auto lPath = addPath("Path", "lane", nodes, norms);
            lane->add("path", lPath->getName());
            nextLanes[laneIn].push_back(lane);
            nextLanes[lane].push_back(laneOut);
            roads->connectGraph(nodes, norms, lane);
        }
	};

    auto bridgeForkingLanes = [&]() { ///FORK
        //cout << "VRRoadNetwork::computeLanes bridgeForkingLanes\n";
        map<VREntityPtr, Vec3d> displacements; // map roads to displace!
	    map<VREntityPtr, bool> processedLanes; // keep list of already processed lanes
        int zz = 0;
        VREntityPtr roadOne;
        //cout << toString(laneMatches.size()) << endl;
        for (auto match : laneMatches) {
            auto laneIn = match.first;
            auto laneOut = match.second;
            auto roadIn = laneIn->getEntity("road");
            auto roadOut = laneOut->getEntity("road");
            int Nin = roadIn->getAllEntities("lanes").size();
            int Nout = roadOut->getAllEntities("lanes").size();
            auto nodes1 = laneIn->getEntity("path")->getAllEntities("nodes");
            auto nodes2 = laneOut->getEntity("path")->getAllEntities("nodes");
            VREntityPtr nodeEnt1 = *nodes1.rbegin();    //last node of roadIn
            VREntityPtr nodeEnt2 = nodes2[0];           //first node of roadOut
            Vec3d X = nodeEnt2->getEntity("node")->getVec3("position") - nodeEnt1->getEntity("node")->getVec3("position");
            float D = X.length();
            //cout <<toString(X.length()) << endl;
            if (Nin >= Nout) {
                auto node1 = nodeEnt1->getEntity("node");    //last node of roadIn
                auto norm1 = nodeEnt1->getVec3("direction");
                auto node2 = nodes2[1]->getEntity("node");  //second node of roadOut
                auto norm2 = nodes2[1]->getVec3("direction");
                //cout << "in" << toString(node1->getValue<int>("graphID", -1)) << endl;
                auto nodeToDelete = nodeEnt2->getEntity("node");
                auto tempID=nodeToDelete->getValue<int>("graphID", -1);
                //cout << tempID << " node removed -- NIN<NOUT" << endl;

                nodeEnt2->set("node", node1->getName());  //set first node of roadOut as last node of roadIn
                if (D > 0) {
                    if (abs(X.length())>abs(displacements[roadOut].length())) displacements[roadOut] = -X;
                    displacements[roadOut] = -X;
                }
                roads->connectGraph({node1,node2}, {norm1,norm2}, laneOut);
                roadOne = roadIn;
                auto rGraph = roads->getGraph();
                rGraph->remNode(tempID);
            }
            if (Nin < Nout) {
                auto node1 = nodes1[nodes1.size()-2]->getEntity("node");
                auto norm1 = nodes1[nodes1.size()-2]->getVec3("direction");
                auto node2 = nodeEnt2->getEntity("node");;
                auto norm2 = nodeEnt2->getVec3("direction");
                //cout << "ot" << toString(node2->getValue<int>("graphID", -1)) << endl;
                auto nodeToDelete = nodeEnt1->getEntity("node");
                auto tempID=nodeToDelete->getValue<int>("graphID", -1);
                //cout << tempID << " node removed -- NIN<NOUT" << endl;

                nodeEnt1->set("node", node2->getName()); //set last node of roadIn as first node of roadOut
                if (D > 0) {
                    if (abs(X.length())>abs(displacements[roadIn].length())) displacements[roadIn] = X;
                    displacements[roadIn] = X;
                }
                roads->connectGraph({node1,node2}, {norm1,norm2}, laneIn);
                roadOne = roadOut;
                auto rGraph = roads->getGraph();
                rGraph->remNode(tempID);
            }
            processedLanes[laneIn] = true;
            processedLanes[laneOut] = true;
            zz++;
        }
        auto graph = roads->getGraph();

        //if (displacements.size() == 0) return;

        vector<vector<int>> inl;
        vector<vector<int>> oul;
        for (auto rfront : roadFronts) {// shift whole road fronts!
            auto road = rfront->road;
            auto rEnt = road->getEntity();

            Vec3d Xa = displacements[rEnt];
            float offsetter = Xa.dot(rfront->pose.x())*rfront->dir;
            if (laneMatches.size() % 2 == 0 && laneMatches.size() < 5){
                if(offsetter > 0) offsetter = road->getWidth()/rEnt->getAllEntities("lanes").size();
                if(offsetter < 0) offsetter = - road->getWidth()/rEnt->getAllEntities("lanes").size();
            }//if (rEnt->getAllEntities("lanes").size()==1) offsetter*=0.5; //hack for merges where only one street comes, might need special case though
            if (rfront->dir>0) {
                road->setOffsetOut(offsetter);
            } else {
                road->setOffsetIn(offsetter);
            }

            vector<int> inIDs;
            vector<int> ouIDs;
            for (auto laneIn : rfront->inLanes) {
                auto nodes = laneIn->getEntity("path")->getAllEntities("nodes");
                auto node1 = nodes[nodes.size()-2]->getEntity("node");
                auto node2 = nodes[nodes.size()-1]->getEntity("node");
                auto edgeID = graph->getEdgeID(node1->getValue<int>("graphID", -1),node2->getValue<int>("graphID", -1));
                inIDs.push_back(edgeID);
                /*if (processedLanes.count(laneIn)) continue;
                //auto nodes = laneIn->getEntity("path")->getAllEntities("nodes");
                VREntityPtr node = (*nodes.rbegin())->getEntity("node");
                auto p = node->getVec3("position");
                p += Xa;
                node->setVec3("position", p, "Position");
                graph->setPosition(node->getValue<int>("graphID", 0), Pose::create(p));
                cout << "------bridgeForkingLanes -  inLane - " << node->getValue<int>("graphID", 0) << p <<"\n";*/
            }

            for (auto laneOut : rfront->outLanes) {
                auto nodes = laneOut->getEntity("path")->getAllEntities("nodes");
                auto node1 = nodes[0]->getEntity("node");
                auto node2 = nodes[1]->getEntity("node");
                auto edgeID = graph->getEdgeID(node1->getValue<int>("graphID", -1),node2->getValue<int>("graphID", -1));
                ouIDs.push_back(edgeID);
                /*if (processedLanes.count(laneOut)) continue;
                VREntityPtr node = laneOut->getEntity("path")->getAllEntities("nodes")[0]->getEntity("node");
                auto p = node->getVec3("position");
                p += Xa;
                node->setVec3("position", p, "Position");
                graph->setPosition(node->getValue<int>("graphID", 0), Pose::create(p));
                cout << "------bridgeForkingLanes - outLane - " << node->getValue<int>("graphID", 0) << p <<"\n";*/
            }
            inl.push_back(inIDs);
            oul.push_back(ouIDs);
        }
        for (int i = 0; i < inl.size(); i++) {
            if (inl[i].size()>1) graph->getEdge(inl[i][0]).relations.clear();
            for (int k = 1; k < inl[i].size(); k++) {
                graph->getEdge(inl[i][k]).relations.clear();
                graph->addRelation(inl[i][k],inl[i][k-1]);
            }
        }
        for (int i = 0; i < oul.size(); i++) {
            if (oul[i].size()>1) graph->getEdge(oul[i][0]).relations.clear();
            for (int k = 1; k < oul[i].size(); k++) {
                graph->getEdge(oul[i][k]).relations.clear();
                graph->addRelation(oul[i][k],oul[i][k-1]);
            }
        }
	};

    getInAndOutLanes();
    computeLaneMatches();

    switch (type) {
        case CONTINUATION: mergeMatchingLanes(); break;
        case FORK: bridgeForkingLanes(); break;
        //case MERGE: break;
        //case UPLINK: break;
        default: bridgeMatchingLanes(); break;
    }
}

void VRRoadIntersection::computePatch() {
    auto roads = world.lock()->getRoadNetwork();
    VREntityPtr node = entity->getEntity("node");
    if (!node) return;
    patch = VRPolygon::create();
    for (auto roadFront : roadFronts) {
        auto road = roadFront->road;
        auto rNode = getRoadNode(road->getEntity());
        if (!rNode) continue;
        auto& endP = road->getEdgePoint( rNode );
        patch->addPoint(Vec2d(endP.p1[0], endP.p1[2]));
        patch->addPoint(Vec2d(endP.p2[0], endP.p2[2]));
    }
    if (patch->computeArea() < 1e-6) { patch.reset(); return; }
    for (auto p : intersectionPoints) patch->addPoint(Vec2d(p[0], p[2]));
    *patch = patch->getConvexHull();
    if (patch->size() <= 2) { patch.reset(); return; }

    median = patch->getBoundingBox().center();
    patch->translate(-median);
	perimeter = patch->shrink(roads->getMarkingsWidth()*0.5);
    patch->translate(median);
    if (auto t = terrain.lock()) t->elevatePolygon(patch, roads->getTerrainOffset());
    patch->translate(-median);
    if (auto t = terrain.lock()) t->elevatePoint(median, roads->getTerrainOffset()); // TODO: elevate each point of the polygon
}

VRGeometryPtr VRRoadIntersection::createGeometry() {
    if (!patch) return 0;
    if (type != DEFAULT) return 0;
    Triangulator tri;
    tri.add( *patch );
    VRGeometryPtr intersection = tri.compute();
    if (intersection->size() == 0) { cout << "VRRoadIntersection::createGeometry ERROR: no geometry created!\n"; return 0; }
    auto p = median; p[1] = 0;
    intersection->setFrom(p);
    intersection->applyTransformation();
	setupTexCoords( intersection, entity );
	addChild(intersection);
	return intersection;
}

VREntityPtr VRRoadIntersection::getRoadNode(VREntityPtr roadEnt) {
    /*string iN = intersectionEnt->getName();
    string rN = roadEnt->getName();
    auto res = ontology->process("q(n):Node(n);Road("+rN+");RoadIntersection("+iN+");has("+rN+".path.nodes,n);has("+iN+".path.nodes,n)");
    if (res.size() == 0) { cout << "Warning in createIntersectionGeometry, road " << roadEnt->getName() << " has no nodes!" << endl; return 0; }
    return res[0];*/

    for (auto rp : roadEnt->getAllEntities("path")) {
        for (auto rnE : rp->getAllEntities("nodes")) {
            for (auto ip : entity->getAllEntities("path")) {
                for (auto inE : ip->getAllEntities("nodes")) {
                    auto rn = rnE->getEntity("node");
                    auto in = inE->getEntity("node");
                    if (in == rn) return in;
                }
            }
        }
    }

    return 0;
}

void VRRoadIntersection::addRoad(VRRoadPtr road) {
    auto roadFront = shared_ptr<RoadFront>( new RoadFront(road) );
    roadFronts.push_back(roadFront);
    entity->add("roads", road->getEntity()->getName());
    road->getEntity()->add("intersections", entity->getName());
}

VREntityPtr VRRoadIntersection::addTrafficLight( PosePtr p, string asset, Vec3d root, VREntityPtr lane, VREntityPtr signal) {
    if (!system) system = VRTrafficLights::create();

    float R = 0.05;
    VRTransformPtr geo = world.lock()->getAssetManager()->copy(asset, Pose::create());
    VRGeometryPtr red, orange, green;
    bool checked = false;
    if (geo) {
        red = dynamic_pointer_cast<VRGeometry>( geo->findFirst("red") );
        orange = dynamic_pointer_cast<VRGeometry>( geo->findFirst("yellow") );
        green = dynamic_pointer_cast<VRGeometry>( geo->findFirst("green") );
        if (!red) {
            red = dynamic_pointer_cast<VRGeometry>( geo->findFirst("Red") );
            orange = dynamic_pointer_cast<VRGeometry>( geo->findFirst("Orange") );
            green = dynamic_pointer_cast<VRGeometry>( geo->findFirst("Green") );
        }
        red->makeUnique();
        orange->makeUnique();
        green->makeUnique();
        red->setColors(0);
        orange->setColors(0);
        green->setColors(0);
        checked = true;
    } else {
        auto box = VRGeometry::create("trafficLight");
        box->setPrimitive("Box", "0.2 0.2 0.6 1 1 1");
        box->setColor("grey");
        geo = box;
        red = VRGeometry::create("trafficLight");
        red->setPrimitive("Sphere", "0.115 2");
        box->addChild(red);
        orange = VRGeometry::create("trafficLight");
        orange->setPrimitive("Sphere", "0.115 2");
        box->addChild(orange);
        green = VRGeometry::create("trafficLight");
        green->setPrimitive("Sphere", "0.115 2");
        box->addChild(green);
        red->translate(Vec3d(0, 0, 0.2));
        green->translate(Vec3d(0, 0, -0.2));
    }
    geo->move(R);
    VRGeometryPtr pole = addPole(root, p->pos(), R);

    //auto light = VRTrafficLight::create(nextLanes[lane], system); // TODO: better data for trafficlight groups
    auto light = VRTrafficLight::create(lane, system, p);
    light->addChild(geo);
    light->setupBulbs(red, orange, green);
    light->setEntity(signal);
    light->setUsingAsset(checked);
    addChild(light);
    addChild(pole);
    return 0;
}

void VRRoadIntersection::computeTrafficLights() {
    //if (roads.size() == 2) return;
    //if (inLanes.size() == 1) return;

    /**

    - get traffic signals relevant for this intersection
        - check if intersection node has traffic signal
        - check each road going in and out of the intersection for traffic signals

    - place traffic signal geometries

    */

    struct signalData {
        shared_ptr<RoadFront> roadFront;
        VREntityPtr lane;
        VREntityPtr signal;
        VREntityPtr node;

        signalData(shared_ptr<RoadFront> rf, VREntityPtr l, VREntityPtr s) : roadFront(rf), lane(l), signal(s) {}
    };

    vector<signalData> signals;
    for (auto roadFront : roadFronts) {
        auto roadE = roadFront->road->getEntity();
        auto lanes = roadE->getAllEntities("lanes");
        for (auto laneE : lanes) {
            auto signs = laneE->getAllEntities("signs");
            for (auto signE : signs) {
                Vec3d pos = signE->getVec3("position");
                if (signE->is_a("TrafficLight")) signals.push_back( signalData(roadFront, laneE, signE) );
            }
        }
    }

    auto getLaneNode = [&](VREntityPtr lane) {
        PosePtr P;
        for (auto pathEnt : lane->getAllEntities("path")) {
            auto entries = pathEnt->getAllEntities("nodes");
            auto entry = entries[entries.size()-1];
            float width = toFloat( lane->get("width")->value );
            Vec3d p = entry->getEntity("node")->getVec3("position");
            Vec3d n = entry->getVec3("direction");
            P = Pose::create( p + Vec3d(0,3,0), Vec3d(0,-1,0), n);
        }
        return P;
    };

    auto node = entity->getEntity("node");
    for (auto s : signals) {
        auto signalNode = s.signal->getEntity("node");
        if (signalNode != node) continue;
        auto p = s.roadFront->pose;
        auto eP = s.roadFront->road->getEdgePoint( node );
        Vec3d root = eP.p1;

        auto lP = getLaneNode(s.lane);
        addTrafficLight(lP, "trafficLight", root, s.lane, s.signal);
    }


    /*auto node = entity->getEntity("node");
    for (auto road : inLanes) {
        auto eP = road.first->getEdgePoints( node );
        Vec3d root = eP.p1;
        for (auto lane : road.second) {
            for (auto pathEnt : lane->getAllEntities("path")) {
                auto entries = pathEnt->getAllEntities("nodes");
                auto entry = entries[entries.size()-1];
                float width = toFloat( lane->get("width")->value );

                Vec3d p = entry->getEntity("node")->getVec3f("position");
                Vec3d n = entry->getVec3f("direction");
                Vec3d x = n.cross(Vec3d(0,1,0));
                x.normalize();
                auto P = pose::create( p + Vec3d(0,3,0), Vec3d(0,-1,0), n);
                addTrafficLight(P, "trafficLight", root);
            }
        }
    }*/
}

void VRRoadIntersection::computeMarkings() {
    if (!perimeter) return;
    string name = entity->getName();

    auto roads = world.lock()->getRoadNetwork();
    float markingsWidth = roads->getMarkingsWidth();
    float markingsWidthHalf = 0.5*markingsWidth;

    auto addLine = [&]( const string& type, Vec3d p1, Vec3d p2, Vec3d n1, Vec3d n2, float w, float dashLength, string color) {
		auto node1 = addNode( 0, p1 );
		auto node2 = addNode( 0, p2 );
		auto m = addPath(type, name, {node1, node2}, {n1,n2});
		m->set("width", toString(w)); //  width in meter
		if (dashLength == 0) m->set("style", "solid"); // simple line
		m->set("style", "dashed"); // dotted line
		m->set("color", color); // dotted line
		m->set("dashLength", toString(dashLength)); // dotted line
		entity->add("markings", m->getName());
		return m;
    };

    for (auto roadFront : roadFronts) if (!roadFront->road->hasMarkings()) return;

    bool isPedestrian = false;
    for (auto roadFront : roadFronts) for (auto lane : roadFront->inLanes) { if (lane->getValue<bool>("pedestrian", false)) isPedestrian = true; break; }

    int inCarLanes = 0;
    for (auto roadFront : roadFronts) {
        for (auto lane : roadFront->inLanes) {
            bool pedestrian = false;
            for (auto l : nextLanes[lane]) if (l->getValue<bool>("pedestrian", false)) pedestrian = true;
            inCarLanes += pedestrian?0:1;
        }
    }

    for (auto roadFront : roadFronts) {
        for (auto lane : roadFront->inLanes) {
            for (auto pathEnt : lane->getAllEntities("path")) {
                auto entry = pathEnt->getEntity("nodes",-1);
                auto node = entry->getEntity("node");
                if (lane->getValue<bool>("pedestrian", false)) continue;

                float W = lane->getValue<float>("width", 0.5);
                Vec3d p = node->getVec3("position");
                Vec3d n = entry->getVec3("direction");
                Vec3d x = n.cross(Vec3d(0,1,0));
                n.normalize();
                x.normalize();

                if (inCarLanes >= 2) { // stop lines
                    float D = 0.4;
                    float w = 0.35;
                    addLine( "StopLine", p-x*W*w+n*D*0.5, p+x*W*w+n*D*0.5, x, x, D, 0, "white");
                }

                // arrows
                vector<float> directions;
                for (auto e : node->getAllEntities("paths")){
                    if (e == entry) continue;
                    auto n2 = e->getEntity("path")->getEntity("nodes",-1)->getVec3("direction");
                    n2.normalize();
                    Vec3d w = n.cross(n2);
                    float a = asin( w.length() );
                    if (w[1] < 0) a = -a;
                    directions.push_back(a);
                }
                addArrows( lane, -5, directions, roads->getArrowStyle() );
            }
        }
    }

    if (!isPedestrian) {
         // border markings
        vector<Vec3d> points;
        for (int i=0; i<perimeter->size(); i++) {
            auto p = perimeter->getPoint(i);
            points.push_back( Vec3d(p[0], 0, p[1]) + median );
        }

        auto isRoadEdge = [&](const Vec3d& p1, const Vec3d& p2) {
            auto pm = (p1+p2)*0.5;
            for (auto rf : roadFronts) {
                float L = (pm-rf->pose.pos()).squareLength();
                if (L < rf->width*rf->width*0.1) return true; // ignore road edges
            }
            return false;
        };

        for (uint i=0; i<points.size(); i++) {
            auto p1 = points[i];
            auto p2 = points[(i+1)%points.size()];
            if (isRoadEdge(p1, p2)) continue;
            Vec3d n = p2-p1; n.normalize();
            p1 -= n*markingsWidthHalf;
            p2 += n*markingsWidthHalf;
            addLine( "RoadMarking", p1, p2, n, n, markingsWidth, 0, "white");
        }
    }
}

void VRRoadIntersection::computeLayout(GraphPtr graph) {
    auto node = entity->getEntity("node");
    Vec3d pNode = node->getVec3("position");
    int N = roadFronts.size();
    int singleRoad;

    // sort roads
    auto compare = [&](shared_ptr<RoadFront> roadFront1, shared_ptr<RoadFront> roadFront2) -> bool {
        auto road1 = roadFront1->road;
        auto road2 = roadFront2->road;
        Vec3d norm1 = road1->getEdgePoint( node ).n;
        Vec3d norm2 = road2->getEdgePoint( node ).n;
        float K = norm1.cross(norm2)[1];
        return (K < 0);
    };

    auto intersect = [&](const Pnt3d& p1, const Vec3d& n1, const Pnt3d& p2, const Vec3d& n2) -> Vec3d {
        Vec3d d = p2-p1;
        Vec3d n3 = n1.cross(n2);
        float N3 = n3.dot(n3);
        if (N3 == 0) N3 = 1.0;
        float s = d.cross(n2).dot(n3)/N3;
        return Vec3d(p1) + n1*s;
    };

    auto getRoadConnectionAngle = [&](VRRoadPtr road1, VRRoadPtr road2) {
        auto& data1 = road1->getEdgePoint( node );
        auto& data2 = road2->getEdgePoint( node );
        return data1.n.dot(data2.n);
        //return data1.n.cross(data2.n);
    };

    auto resolveIntersectionType = [&]() {
        /*cout << " --- resolveIntersectionType " << N;
        for (auto r : roads) cout << "  " << r->getEntity()->getName();
        cout << endl;*/

        if (N == 2) {
            bool parallel  = bool( getRoadConnectionAngle(roadFronts[0]->road, roadFronts[1]->road) < -0.8 );
            if (parallel) type = CONTINUATION;
        }

        if (N == 3) {
            bool parallel01 = bool(getRoadConnectionAngle(roadFronts[0]->road, roadFronts[1]->road) < -0.82);
            bool parallel12 = bool(getRoadConnectionAngle(roadFronts[1]->road, roadFronts[2]->road) < -0.82);
            bool parallel02 = bool(getRoadConnectionAngle(roadFronts[2]->road, roadFronts[0]->road) < -0.82);
            if ((parallel01 && parallel12) || (parallel01 && parallel02) || (parallel12 && parallel02)) {type = FORK;}
            if (parallel01 && parallel02) {singleRoad = 0;}
            if (parallel01 && parallel12) {singleRoad = 1;}
            if (parallel12 && parallel02) {singleRoad = 2;}
            //type = FORK;
        }

        auto entity = getEntity();
        switch (type) {
            case CONTINUATION: entity->set("type", "continuation"); break;
            case FORK: entity->set("type", "fork"); break;
            case MERGE: entity->set("type", "merge"); break;
            default: entity->set("type", "intersection");
        }
    };

    auto resolveEdgeIntersections = [&]() {
        for (int r = 0; r<N; r++) { // compute intersection points
            auto road1 = roadFronts[r]->road;
            auto road2 = roadFronts[(r+1)%N]->road;
            auto& data1 = road1->getEdgePoint( node );
            auto& data2 = road2->getEdgePoint( node );
            Vec3d Pi = intersect(data1.p2, data1.n, data2.p1, data2.n);
            data1.p2 = Pi;
            data2.p1 = Pi;
            intersectionPoints.push_back(Pi);
        }
    };

    auto elevateRoadNodes = [&]() {
        if (patch) {
            for (uint i=0; i<N; i++) { // elevate road nodes to median intersection height
                auto r = roadFronts[i]->road;
                auto e1 = r->getNodeEntry(node);
                auto n = e1->getEntity("node");
                auto d = e1->getVec3("direction");
                auto p = n->getVec3("position");
                p[1] = median[1];
                n->setVector("position", toStringVector(p), "Position");

                // check if any road node is inside of the intersection!
                /*auto path = roads[i]->getEntity()->getEntity("path");
                for (auto e2 : path->getAllEntities("nodes")) {
                    if (e1 == e2) continue;
                    auto n = e2->getEntity("node");
                    auto np = n->getVec3f("position") - median;
                    if (patch->isInside(Vec2d(np[0],np[2]))) {
                        n->setVector("position", toStringVector(p-d*0.1), "Position");
                    }
                }*/
            }
        }
    };

    auto computeRoadFronts = [&]() {
        for (auto rf : roadFronts) { // compute road front
            auto road = rf->road;
            auto& data = road->getEdgePoint( node );
            Vec3d p1 = data.p1;
            Vec3d p2 = data.p2;
            Vec3d norm = data.n;
            float d1 = (p1-pNode).dot(norm);
            float d2 = (p2-pNode).dot(norm);
            float d = min(d1,d2);
            d1 = max(0.f,d1-d);
            d2 = max(0.f,d2-d);
            data.p1 = p1-norm*(d1);
            data.p2 = p2-norm*(d2);

            Vec3d pm = (data.p1 + data.p2)*0.5; // compute road node
            int nID = graph->addNode();
            graph->setPosition(nID, Pose::create(pm));
            auto n = addNode(nID, pm);
            data.entry->set("node", n->getName());
            n->add("paths", data.entry->getName());

            auto rd = data.entry->getVec3("direction");
            rf->pose = Pose(pm, norm);
            rf->width = road->getWidth();
            rf->dir = round(rd.dot(norm));
        }
    };

    auto computeIntersectionPaths = [&]() {
        vector<VREntityPtr> iPaths;
        for (uint i=0; i<N; i++) { // compute intersection paths
            auto road1 = roadFronts[i]->road;
            auto rEntry1 = road1->getNodeEntry(node);
            if (!rEntry1) continue;
            int s1 = toInt(rEntry1->get("sign")->value);
            Vec3d norm1 = rEntry1->getVec3("direction");
            auto& data1 = road1->getEdgePoint( node );
            VREntityPtr node1 = data1.entry->getEntity("node");
            if (s1 == 1) {
                for (uint j=0; j<N; j++) { // compute intersection paths
                    if (j == i) continue;
                    auto road2 = roadFronts[j]->road;
                    auto rEntry2 = road2->getNodeEntry(node);
                    if (!rEntry2) continue;
                    int s2 = toInt(rEntry2->get("sign")->value);
                    if (s2 != -1) continue;
                    Vec3d norm2 = rEntry2->getVec3("direction");
                    auto& data2 = road2->getEdgePoint( node );
                    VREntityPtr node2 = data2.entry->getEntity("node");
                    auto pathEnt = addPath("Path", "intersection", {node1, node2}, {norm1, norm2});
                    iPaths.push_back(pathEnt);
                }
            }
        }
        for (auto path : iPaths) entity->add("path", path->getName());
    };

    auto resolveSpacialCases = [&]() {
        if (type == CONTINUATION) { // special cases for 2 roads
            /*bool parallel = bool(getRoadConnectionAngle(roads[0], roads[1]) < -0.8);
            if (parallel) { // nearly parallel, but opposite directions
                ; // TODO
                return true; // special case
            }*/
            return true;
        }

        if (type == FORK || type == MERGE) {
            //cout << "------specialcase_fork\n";
            /*Vec3d n1;
            for (int i=0; i<roads.size(); i++) {
                auto& data = roads[i]->getEdgePoints( node );
                if (n1.cross(data.n).squareLength() > 1e-5) return false; // not parallel, no special case, normal intersection
                n1 = data.n;
            }*/
            //same direction for incoming and outgoing roads
            for (auto rf : roadFronts) {
                auto road = rf->road;
                auto roadOne = roadFronts[singleRoad]->road;
                bool pedestrian = false;
                for (auto lane : rf->inLanes) { if (lane->getValue<bool>("pedestrian", false)) pedestrian = true; break;} //exception might bug some lanes of highways are pedestrian
                for (auto lane : rf->inLanes) { if (lane->getValue<bool>("sidewalk", false)) pedestrian = true; break;} //exception might bug some lanes of highways are pedestrian
                if (pedestrian) return false;
                VREntityPtr rEntry = road->getNodeEntry( node );
                VREntityPtr rEntryRO = roadOne->getNodeEntry( node );

                auto norm1 = rEntry->getVec3("direction");
                auto norm2 = rEntryRO->getVec3("direction");
                if (norm1.dot(norm2)<0) norm2 = -norm2;
                rEntry->setVec3("direction", norm2, "Direction");
            }
            return true; // special case
        }

        return false; // no special case
    };

    sort( roadFronts.begin(), roadFronts.end(), compare );            // sort roads by how they are aligned next to each other
    resolveIntersectionType();
    if (!resolveSpacialCases()) resolveEdgeIntersections(); //
    computeRoadFronts();
    computeIntersectionPaths();
    computePatch();
    elevateRoadNodes();
}

vector<VRTrafficLightPtr> VRRoadIntersection::getTrafficLights(){
    vector<VRTrafficLightPtr> res;
    if (!system) return res;
    return system->getLights();
}

map<int, vector<VRTrafficLightPtr>> VRRoadIntersection::getTrafficLightMap(){
    map<int, vector<VRTrafficLightPtr>> res;
    if (!system) return res;
    return system->getMap();
}

void VRRoadIntersection::update() {
    if (!system) return;
    system->update();
}



