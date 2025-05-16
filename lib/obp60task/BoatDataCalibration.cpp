#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include <cmath>
#include <math.h>

CalibrationDataList calibrationData;

void CalibrationDataList::readConfig(GwConfigHandler* config, GwLog* logger)
// Initial load of calibration data into internal list
// This method is called once at init phase of <OBP60task> to read the configuration values
{
    String instance;
    double offset;
    double slope;

    String calInstance = "";
    String calOffset = "";
    String calSlope = "";

    // Load user format configuration values
    String lengthFormat = config->getString(config->lengthFormat); // [m|ft]
    String distanceFormat = config->getString(config->distanceFormat); // [m|km|nm]
    String speedFormat = config->getString(config->speedFormat); // [m/s|km/h|kn]
    String windspeedFormat = config->getString(config->windspeedFormat); // [m/s|km/h|kn|bft]
    String tempFormat = config->getString(config->tempFormat); // [K|C|F]

    // Read calibration settings for data instances
    for (int i = 0; i < maxCalibrationData; i++) {
        calInstance = "calInstance" + String(i+1);
        calOffset = "calOffset" + String(i+1);
        calSlope = "calSlope" + String(i+1);
        calibrationData.list[i] = { "---", 0.0f, 1.0f, 0.0f, false };

        instance = config->getString(calInstance, "---");
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "no calibration data for instance no. %d", i+1);
            continue;
        }
        offset = (config->getString(calOffset, "")).toFloat();
        slope = (config->getString(calSlope, "")).toFloat();

        // Convert calibration values to internal standard formats
        if (instance == "AWS" || instance == "TWS") {
            if (windspeedFormat == "m/s") {
                // No conversion needed
            } else if (windspeedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                offset *= 0.5; // Convert Bft to m/s (approx) -> to be improved
            }

        } else if (instance == "AWA" || instance == "TWA" ||instance == "TWD" || instance == "HDM" ||
                   instance == "PRPOS" || instance == "RPOS") {
            offset *= M_PI / 180; // Convert deg to rad

        } else if (instance == "DBT") {
            if (lengthFormat == "m") {
                // No conversion needed
            } else if (lengthFormat == "ft") {
                offset /= 3.28084; // Convert ft to m
            }
 
        } else if (instance == "STW") {
            if (speedFormat == "m/s") {
                // No conversion needed
            } else if (speedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (speedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            }

        } else if (instance == "WTemp") {
            if (tempFormat == "K" || tempFormat == "C") {
                // No conversion needed
            } else if (tempFormat == "F") {
                offset *= 9.0 / 5.0; // Convert °F to K
                slope *= 9.0 / 5.0; // Convert °F to K
            }
        }
        calibrationData.list[i].instance = instance;
        calibrationData.list[i].offset = offset;
        calibrationData.list[i].slope = slope;
        calibrationData.list[i].isCalibrated = false;
        LOG_DEBUG(GwLog::LOG, "stored calibration data: %s, offset: %f, slope: %f", calibrationData.list[i].instance.c_str(), calibrationData.list[i].offset, calibrationData.list[i].slope);
    }
    LOG_DEBUG(GwLog::LOG, "all calibration data read");
}

int CalibrationDataList::getInstanceListNo(String instance)
// Method to get the index of the requested instance in the list
{
    // Check if instance is in the list
    for (int i = 0; i < maxCalibrationData; i++) {
        if (calibrationData.list[i].instance == instance) {
            return i;
        }
    }
    return -1; // instance not found
}

/* void CalibrationDataList::updateBoatDataValidity(String instance)
{
    for (int i = 0; i < maxCalibrationData; i++) {
        if (calibrationData.list[i].instance == instance) {
            // test for boat data value validity - to be implemented
            calibrationData.list[i].isValid = true;
            return;
        }
    }
} */

void CalibrationDataList::calibrateInstance(String instance, GwApi::BoatValue* boatDataValue, GwLog* logger)
// Method to calibrate the boat data value
{
    double offset = 0;
    double slope = 1.0;
    double dataValue = 0;

    int listNo = getInstanceListNo(instance);
    if (listNo < 0) {
        LOG_DEBUG(GwLog::LOG, "BoatDataCalibration: %s not found in calibration data list", instance.c_str());
        return;
    } else {
        offset = calibrationData.list[listNo].offset;
        slope = calibrationData.list[listNo].slope;

        if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
            return;
        } else {
            dataValue = boatDataValue->value;
            LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: name: %s: value: %f format: %s", boatDataValue->getName().c_str(), boatDataValue->value, boatDataValue->getFormat().c_str());

            if (boatDataValue->getFormat() == "formatWind") { // instance is of type angle
                dataValue = (dataValue * slope) + offset;
                dataValue = fmod(dataValue, 2 * M_PI);
                if (dataValue > (M_PI)) {
                    dataValue -= (2 * M_PI);
                } else if (dataValue < (M_PI * -1)) {
                    dataValue += (2 * M_PI);
                }
            } else if (boatDataValue->getFormat() == "formatCourse") { // instance is of type direction
                dataValue = (dataValue * slope) + offset;
                dataValue = fmod(dataValue, 2 * M_PI);
                if (dataValue < 0) {
                    dataValue += (2 * M_PI);
                }
            } else if (boatDataValue->getFormat() == "kelvinToC") { // instance is of type temperature
                dataValue = ((dataValue - 273.15) * slope) + offset + 273.15;
            } else {
                dataValue = (dataValue * slope) + offset;
            }

            calibrationData.list[listNo].value = dataValue;
            calibrationData.list[listNo].isCalibrated = true;
            boatDataValue->value = dataValue;
            LOG_DEBUG(GwLog::LOG, "BoatDataCalibration: %s: Offset: %f Slope: %f Result: %f", instance.c_str(), offset, slope, boatDataValue->value);
        }
    }
}

#endif