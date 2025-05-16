// Functions lib for data instance calibration

#ifndef _BOATDATACALIBRATION_H
#define _BOATDATACALIBRATION_H

#include "Pagedata.h"
#include "WString.h"

typedef struct {
    String instance; // data type/instance to be calibrated
    double offset; // calibration offset
    double slope; // calibration slope
    double value; // calibrated data value
    bool isCalibrated; // is data instance value calibrated?
} CalibData;

const int maxCalibrationData = 3; // maximum number of calibration data instances

class CalibrationDataList {
public:
    CalibData list[maxCalibrationData]; // list of calibration data instances

    static void readConfig(GwConfigHandler* config, GwLog* logger);
    static int getInstanceListNo(String instance);
    static void calibrateInstance(String instance, GwApi::BoatValue* boatDataValue, GwLog* logger);

private:
};

extern CalibrationDataList calibrationData; // this list holds all calibration data

#endif