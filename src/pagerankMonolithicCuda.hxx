#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include "_main.hxx"
#include "transpose.hxx"
#include "dynamic.hxx"
#include "pagerank.hxx"
#include "pagerankCuda.hxx"

using std::vector;
using std::sqrt;
using std::partition;
using std::swap;
using std::min;
using std::max;




// PAGERANK-LOOP
// -------------

template <class T, class J>
int pagerankMonolithicCudaLoop(T *e, T *r0, T *eD, T *r0D, T *&aD, T *&rD, T *cD, const T *fD, const int *vfromD, const int *efromD, int i, const J& ns, int N, T p, T E, int L, int EF) {
  int n = sumAbs(ns);
  int R = reduceSizeCu<T>(n);
  size_t R1 = R * sizeof(T);
  T c0 = (1-p)/N;
  int l = 0;
  while (l<L) {
    pagerankSwitchedCu(aD, rD, cD, vfromD, efromD, i, ns, l, c0);  // assume contribtions (cD) is precalculated
    pagerankErrorCu(eD, aD+i, rD+i, n, EF);
    TRY( cudaMemcpy(e, eD, R1, cudaMemcpyDeviceToHost) );
    T el = pagerankErrorReduce(e, R, EF); ++l;                     // one iteration complete
    if (el<E || l>=L) break;                                       // check tolerance, iteration limit
    multiplyCu(cD+i, aD+i, fD+i, n);                               // update partial contributions (cD)
    swap(aD, rD);                                                  // final ranks always in (aD)
  }
  return l;
}




// PAGERANK (STATIC / INCREMENTAL)
// -------------------------------

// Find pagerank accelerated with CUDA (pull, CSR).
// @param x  original graph
// @param xt transpose graph (with vertex-data=out-degree)
// @param q  initial ranks (optional)
// @param o  options {damping=0.85, tolerance=1e-6, maxIterations=500}
// @returns {ranks, iterations, time}
template <class G, class H, class T=float>
PagerankResult<T> pagerankMonolithicCuda(const G& x, const H& xt, const vector<T> *q=nullptr, PagerankOptions<T> o={}) {
  int  N  = xt.order();                   if (N==0) return PagerankResult<T>::initial(xt, q);
  auto ks = pagerankCudaVertices(x, xt);
  auto ns = pagerankWave(xt, ks);
  return pagerankCuda(xt, ks, 0, ns, pagerankMonolithicCudaLoop<T, decltype(ns)>, q, o);
}
template <class G, class T=float>
PagerankResult<T> pagerankMonolithicCuda(const G& x, const vector<T> *q=nullptr, PagerankOptions<T> o={}) {
  auto xt = transposeWithDegree(x);
  return pagerankMonolithicCuda(x, xt, q, o);
}




// PAGERANK (DYNAMIC)
// ------------------

template <class G, class H, class T=float>
PagerankResult<T> pagerankMonolithicCudaDynamic(const G& x, const H& xt, const G& y, const H& yt, const vector<T> *q=nullptr, PagerankOptions<T> o={}) {
  int  N  = yt.order();                                      if (N==0) return PagerankResult<T>::initial(yt, q);
  auto [ks, n] = pagerankCudaDynamicVertices(x, xt, y, yt);  if (n==0) return PagerankResult<T>::initial(yt, q);
  auto ns = pagerankWave(yt, sliceIter(ks, 0, n));
  return pagerankCuda(yt, ks, 0, ns, pagerankMonolithicCudaLoop<T, decltype(ns)>, q, o);
}

template <class G, class T=float>
PagerankResult<T> pagerankMonolithicCudaDynamic(const G& x, const G& y, const vector<T> *q=nullptr, PagerankOptions<T> o={}) {
  auto xt = transposeWithDegree(x);
  auto yt = transposeWithDegree(y);
  return pagerankMonolithicCudaDynamic(x, xt, y, yt, q, o);
}
