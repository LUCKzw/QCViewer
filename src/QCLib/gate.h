#ifndef GATE
#define GATE

#include <string>
#include <vector>
#include "state.h"

using namespace std;


//used to specify a control number and polarity
class Control
{
public:
    Control (int,bool);
    unsigned int wire;
    bool polarity;
};

/*
GATE
---------------
This is the general gate class from which all gates are derived.
Name is kept private since in the case of gates like the arbitray
rotation gate we may want the name to be dependent on the rot amount
*/

class Gate
{
public:
    enum gateType {RGATE, UGATE};
    enum dType {NOT, FRED, DEFAULT};
    virtual ~Gate();
    virtual Gate* clone()=0;
    virtual string getName() const=0;
    virtual State *applyToBasis(index_t)=0;
    gateType type; //used with enum gateType
    dType drawType;
    vector <Control> controls;
    vector <unsigned int> targets;
};

//A gate matrix struct for UGate
struct gateMatrix {
    unsigned int dim;
    complex<float_type> * data;
};

//A general unitary gate
class UGate : public Gate
{
public:
    UGate(string);
    Gate* clone();
    string getName() const;
    State *applyToBasis(index_t);
    void setName(string);
private:
    unsigned int ExtractInput (index_t);
    index_t BuildBitString (index_t, unsigned int);
    State* ApplyU(index_t);
    gateMatrix *matrix;
    string name;
};

//An arbitrary rotation gate
class RGate : public Gate
{
public:
    enum Axis { X, Y, Z };
    RGate(float_type, Axis);
    Gate* clone();
    string getName() const;
    State *applyToBasis(index_t);
    float_type get_rotVal () const; // XXX: remove float_type, consildate this stuff!!
    void set_rotVal (float_type);
    Axis get_axis () const;
    void set_axis (Axis);
private:
    float_type rot;
    index_t BuildBitString (index_t, unsigned int);
    State* ApplyU(index_t);
    string name;
    Axis axis;
};

void minmaxWire (vector<Control>* ctrl, vector<unsigned int>* targ, unsigned int *dstmin, unsigned int *dstmax);
#endif
