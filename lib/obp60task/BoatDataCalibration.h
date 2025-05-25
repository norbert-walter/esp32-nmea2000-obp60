// Functions lib for data instance calibration

#ifndef _BOATDATACALIBRATION_H
#define _BOATDATACALIBRATION_H

#include "Pagedata.h"
#include <string>
#include <unordered_map>

#define MAX_CALIBRATION_DATA 3 // maximum number of calibration data instances

typedef struct {
    double offset; // calibration offset
    double slope; // calibration slope
    double smooth; // smoothing factor
    double value; // calibrated data value
    bool isCalibrated; // is data instance value calibrated?
} TypeCalibData;

class CalibrationDataList {
public:
    static std::unordered_map<std::string, TypeCalibData> calibMap; // list of calibration data instances

    void readConfig(GwConfigHandler* config, GwLog* logger);
    void calibrateInstance(GwApi::BoatValue* boatDataValue, GwLog* logger);
    void smoothInstance(GwApi::BoatValue* boatDataValue, GwLog* logger);

private:
};

extern CalibrationDataList calibrationData; // this list holds all calibration data

#endif