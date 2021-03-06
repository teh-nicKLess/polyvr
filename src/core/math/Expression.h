#ifndef EXPRESSION_H_INCLUDED
#define EXPRESSION_H_INCLUDED

#include <string>
#include <map>

#include <OpenSG/OSGVector.h>

using namespace std;
OSG_BEGIN_NAMESPACE;

class Expression {
    public:
        struct ValueBase {
            virtual ~ValueBase();
            virtual string toString() = 0;
            virtual ValueBase* add(ValueBase* n) = 0;
            virtual ValueBase* sub(ValueBase* n) = 0;
            virtual ValueBase* mult(ValueBase* n) = 0;
            virtual ValueBase* div(ValueBase* n) = 0;
            virtual ValueBase* compE(ValueBase* n) = 0;
            virtual ValueBase* compL(ValueBase* n) = 0;
            virtual ValueBase* compLE(ValueBase* n) = 0;
            virtual ValueBase* compG(ValueBase* n) = 0;
            virtual ValueBase* compGE(ValueBase* n) = 0;
        };

        template<typename T> struct Value : ValueBase {
            T value;
            Value(const T& t);
            ~Value();
            string toString();

            // TODO: only declare here, use partial template implementations!
            ValueBase* add(ValueBase* n);
            ValueBase* sub(ValueBase* n);
            ValueBase* mult(ValueBase* n);
            ValueBase* div(ValueBase* n);
            ValueBase* compE(ValueBase* n);
            ValueBase* compL(ValueBase* n);
            ValueBase* compLE(ValueBase* n);
            ValueBase* compG(ValueBase* n);
            ValueBase* compGE(ValueBase* n);
        };

        //ValueBase* add(Value<Vec3d>* v1, Value<Vec3d>* v2) { return new Value<T>(v1->value + v2->value); }

        struct Node {
            string param;
            ValueBase* value = 0;
            Node* parent = 0;
            Node* left = 0;
            Node* right = 0;

            Node(string s);
            ~Node();

            void setValue(float f);
            void setValue(Vec3d v);
            void setValue(string s);

            string toString();
            string treeToString(string indent = "");
            void compute();
        };

    public:
        string data;
        Node* tree = 0;
        vector<Node*> nodes;
        map<char,int> OperatorHierarchy;
        bool prefixExpr = false;

        bool isMathToken(char c);
        void convToPrefixExpr();
        void buildTree();

    public:
        Expression(string s);
        ~Expression();

        bool isMathExpression();
        void computeTree();
        vector<Node*> getLeafs();
        string compute();
        string toString();
};

OSG_END_NAMESPACE;

#endif // EXPRESSION_H_INCLUDED
