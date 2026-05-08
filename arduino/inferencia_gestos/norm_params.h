// Parâmetros de normalização (mean/std por feature)
// Gerado por 02_conversao_tflite.ipynb

#pragma once

const float kNormMean[6] = {0.07509518f, -0.17964466f, -0.21709919f, 2.62790179f, -0.09379248f, 2.93608856f};
const float kNormStd[6] = {0.43974835f, 0.33774292f, 0.90554374f, 26.58422661f, 31.20078087f, 35.28195953f};

const int kNumSamples  = 119;
const int kNumFeatures = 6;
const int kNumClasses  = 3;

const char* kClassLabels[] = {"punch", "flex", "idle"};
