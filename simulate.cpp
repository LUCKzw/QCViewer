#include "simulate.h"
#include <cmath>
#include <string>
#include <iostream>//XXX
#include "utility.h"


struct gateMatrix{
	unsigned int dim;
	complex<float> * data;
};

gateMatrix getGateMatrix(Gate*);//defined below

/*
 Registers: stored as a bitstring, register i at bit i etc.
*/

//Sets the bit at position reg to 1
unsigned int SetRegister (unsigned int bits, unsigned int reg) {
  return bits | 1<<reg;
}

//Sets the bit at position reg to 0
unsigned int UnsetRegister (unsigned int bits, unsigned int reg) {
  return bits & ~(1<<reg);
}

//Returns the value of the bit at position reg
unsigned int GetRegister (unsigned int bits, unsigned int reg) {
  return (bits & (1 << reg)) >> reg;
}

/*
	This function takes an input bitstring and the mapping of targets and returns
	the input to the gate, for the purpose of indexing into its matrix.
*/
unsigned int ExtractInput (index_t bitString, vector<int>* targetMap) {
  unsigned int input = 0;
  for (unsigned int i = 0; i < targetMap->size(); i++) {
    if (GetRegister (bitString, (*targetMap)[i])) {
      input = SetRegister (input, i);
    }
  }
  return input;
}

/*
	This function takes an input string, the mapping of targets, and an output for those targets
	and produces the output bitstring
*/
index_t BuildBitString (index_t orig, vector<int>* targetMap, unsigned int ans) {
  unsigned int output = orig;
  for (unsigned int i = 0; i < targetMap->size (); i++) {
    if (GetRegister (ans, i)) {
			output = SetRegister (output, (*targetMap)[i]);
    } else {
      output = UnsetRegister (output, (*targetMap)[i]);
    }
  }
  return output;
}

/*
	 This function takes an entire input basis vector, the matrix to apply and
   the targets, which describe which bits map to which inputs of the matrix.
   It returns a (usually very sparse) quantum state.
*/
State* ApplyU (index_t bits, vector<int>* targets, Gate *g) {
  unsigned int input = ExtractInput (bits, targets);
  // now, go through all rows of the output from the correct column of U
  State *answer = new State;
	gateMatrix matrix = getGateMatrix(g);
  for (unsigned int i = 0; i < matrix.dim; i++) {
    if (matrix.data[input*matrix.dim+i] != complex<float_t>(0)){
			*answer += State(matrix.data[input*matrix.dim+i], BuildBitString (bits, targets, i));
		}
  }
  return answer;
}

/*
	This function takes a computational basis element and a gate
	and simulates. It returns a quantum state (usually very sparse.)
*/
State* ApplyGateToBasis (index_t bitString, Gate *g) {
  // First, make sure all of the controls are satisfied.
  bool ctrl = true;
  for (unsigned int i = 0; i < g->controls.size(); i++) {
    Control c = g->controls[i];
    int check = GetRegister (bitString, c.wire);
    if (!(!c.polarity == check)) {
      ctrl = false; // control line not satisfied.
      break;
    }
  }
  if (ctrl) {
    return ApplyU (bitString, &g->targets, g);
  } else {
    State *answer = new State (1, bitString); // with amplitude 1 the input bitString is unchanged
    return answer;
  }
}

/*
	This function takes an input quantum state and a gate.
	It splits the input superposition into basis states (in the computational basis,
	i.e. into class bit strings.) It runs the gate on each of these, and collates
	the answers (weighed appropriately) into the answer.
	NULL on error (couldn't find a unitary.)
*/
State ApplyGate (State* in, Gate* g) {
  map<index_t, complex<float_t> >::iterator it;
  State answer;
	answer.dim = in->dim;
  for (it = in->data.begin(); it != in->data.end(); it++) {
    State* tmp = ApplyGateToBasis (it->first, g);
    if (tmp == NULL){
			 return NULL;
		}
    complex<float_t> foo = (*it).second;
    *tmp *= foo;
    answer += *tmp;
    delete tmp;
  }
  return answer;
}

gateMatrix getGateMatrix(Gate *g){
	gateMatrix ret;
	if (g->name.compare("H") == 0){
		ret.data = new complex<float>[4];
		ret.data[0]=  1/sqrt(2) ; ret.data[2]=  1/sqrt(2);
		ret.data[1]=  1/sqrt(2) ; ret.data[3]= -1/sqrt(2);
		ret.dim = 2;
	} else if (g->name.compare("T") == 0||g->name.compare("X") == 0){
		ret.data = new complex<float>[4];
		ret.data[0]=  0 ; ret.data[2]=  1;
		ret.data[1]=  1 ; ret.data[3]=  0;
		ret.dim = 2;
	} else if (g->name.compare("Y") == 0){
		ret.data = new complex<float>[4];
		ret.data[0]=  0                   ; ret.data[2]=  -complex<float>(0,1);
		ret.data[1]=  complex<float>(0,1) ; ret.data[3]=  0;
		ret.dim = 2;
	} else if (g->name.compare("Z") == 0){
		ret.data = new complex<float>[4];
		ret.data[0]=  1 ; ret.data[2]=   0;
		ret.data[1]=  0 ; ret.data[3]=  -1;
		ret.dim = 2;
	} else if (g->name.compare("R") == 0){
		ret.data = new complex<float>[4];
		ret.data[0]=  1 ; ret.data[2]=   0;
		ret.data[1]=  0 ; ret.data[3]=  exp(complex<float>(0,g->setting));
		ret.dim = 2;
	} else if (g->name.compare("F") == 0){
		ret.data = new complex<float>[16];
		ret.data[0 ]=  1 ; ret.data[4 ]=  0; ret.data[8 ]=  0 ; ret.data[12]=  0;
		ret.data[1 ]=  0 ; ret.data[5 ]=  0; ret.data[9 ]=  1 ; ret.data[13]=  0;
		ret.data[2 ]=  0 ; ret.data[6 ]=  1; ret.data[10]=  0 ; ret.data[14]=  0;
		ret.data[3 ]=  0 ; ret.data[7 ]=  0; ret.data[11]=  0 ; ret.data[15]=  1;
		ret.dim = 4;
	}
	return ret;
}
