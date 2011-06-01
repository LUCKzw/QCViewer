#include "state.h"
#include "utility.h" // ipow
#include <complex>

using namespace std;

State::State () {}

State::State (complex<float_t> amp, index_t bits) {
  data[bits] = amp;
}

State::State (stateVec &v) {
  //assert (v.dim <= sizeof(index_t)*8);
  for (index_t i = 0; i < (index_t)ipow(2,v.dim); i++) {
    if (v.data[i] != complex<float_t>(0)) {
      data[i] = v.data[i];
    }
  }
}

complex<float_t> State::getAmplitude (index_t bits) {
  if (data.find(bits) == data.end())
     return 0;
  else
     return data[bits];
}

const State& State::operator+= (const State& r) {
  map<index_t, complex<float_t> >::const_iterator it;
  for (it = r.data.begin(); it != r.data.end(); it++) {
    data[it->first] += it->second;
  }
  return *this;
}
