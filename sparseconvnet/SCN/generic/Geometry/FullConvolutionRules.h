// Copyright 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the license found in the
// LICENSE file in the root directory of this source tree.

#ifndef FULLDECONVOLUTIONRULES_H
#define FULLDECONVOLUTIONRULES_H
#include "RectangularRegions.h"

template <uInt dimension>
void FullConvolution_InputSgToRulesAndOutputSg(
    SparseGrid<dimension> &inputGrid, SparseGrid<dimension> &outputGrid,
    RuleBook &rules, long *size, long *stride, long *inputSpatialSize,
    long *outputSpatialSize) {
  rules.resize(volume<dimension>(size));

  // Swap Input.. and OutputRegionCalculator v.s. a normal Convolution
  for (auto const &inIter : inputGrid.mp) {
    auto outRegion =
        InputRegionCalculator<dimension>(inIter.first, size, stride);
    for (auto j : outRegion) {
      auto inRegion =
          OutputRegionCalculator<dimension>(j, size, stride, outputSpatialSize);
      uInt rulesOffset = outRegion.offset(j);
      auto outIter = outputGrid.mp.find(j);
      if (outIter == outputGrid.mp.end()) {
        outIter =
            outputGrid.mp.insert(std::make_pair(j, outputGrid.ctr++)).first;
      }
      rules[rulesOffset].push_back(inIter.second + inputGrid.ctr);
      rules[rulesOffset].push_back(outIter->second);
    }
  }
}

template <uInt dimension>
uInt FullConvolution_InputSgsToRulesAndOutputSgs(
    SparseGrids<dimension> &input_SGs, SparseGrids<dimension> &output_SGs,
    RuleBook &rules, long *filterSize, long *filterStride,
    long *input_spatialSize, long *output_spatialSize) {
  rules.clear();
  output_SGs.clear();
  uInt batchSize = input_SGs.size();
  output_SGs.resize(batchSize);
  uInt output_nActive = 0;
  for (uInt i = 0; i < batchSize; i++) {
    auto &iSG = input_SGs[i];
    auto &oSG = output_SGs[i];
    oSG.ctr = output_nActive;
    FullConvolution_InputSgToRulesAndOutputSg<dimension>(
        iSG, oSG, rules, filterSize, filterStride, input_spatialSize,
        output_spatialSize);
    output_nActive = oSG.ctr;
    oSG.ctr = 0;
  }
  return output_nActive;
}

template <uInt dimension>
uInt FullConvolution_InputSgsToRulesAndOutputSgs_OMP(
    SparseGrids<dimension> &input_SGs, SparseGrids<dimension> &output_SGs,
    RuleBook &rules, long *filterSize, long *filterStride,
    long *input_spatialSize, long *output_spatialSize) {
  rules.clear();
  rules.resize(volume<dimension>(filterSize));
  output_SGs.clear();
  uInt batchSize = input_SGs.size();
  output_SGs.resize(batchSize);
  std::vector<RuleBook> rbs(batchSize);
  {
    uInt i;
#pragma omp parallel for private(i)
    for (i = 0; i < batchSize; i++)
      FullConvolution_InputSgToRulesAndOutputSg<dimension>(
          input_SGs[i], output_SGs[i], rbs[i], filterSize, filterStride,
          input_spatialSize, output_spatialSize);
  }
  uInt output_nActive = 0;
  for (uInt i = 0; i < batchSize; i++) {
    // Parallel assignment:
    // output_nActive     <-  output_nActive+output_SGs[i].ctr
    // output_SGs[i].ctr  <-  output_nActive
    uInt tmp = output_nActive;
    output_nActive += output_SGs[i].ctr;
    output_SGs[i].ctr = tmp;
  }
  {
    uInt i;
#pragma omp parallel for private(i)
    for (i = 0; i < rules.size(); i++) {
      auto &R = rules[i];
      for (uInt j = 0; j < batchSize; j++) {
        auto &r = rbs[j][i];
        auto offset = output_SGs[j].ctr;
        for (uInt k = 0; k < r.size();) {
          R.push_back(r[k++]);
          R.push_back(r[k++] + offset);
        }
      }
    }
  }
  return output_nActive;
}
#endif /* FULLDECONVOLUTIONRULES_H */
