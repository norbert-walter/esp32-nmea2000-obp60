#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include <cmath>
#include <math.h>
#include <unordered_map>

CalibrationDataList calibrationData;
std::unordered_map<std::string, TypeCalibData> CalibrationDataList::calibMap; // list of calibration data instances

void CalibrationDataList::readConfig(GwConfigHandler* config, GwLog* logger)
// Initial load of calibration data into internal list
// This method is called once at init phase of <obp60task> to read the configuration values
{
    std::string instance;
    double offset;
    double slope;
    double smooth;

    String calInstance = "";
    String calOffset = "";
    String calSlope = "";
    String calSmooth = "";

    // Load user format configuration values
    String lengthFormat = config->getString(config->lengthFormat); // [m|ft]
    String distanceFormat = config->getString(config->distanceFormat); // [m|km|nm]
    String speedFormat = config->getString(config->speedFormat); // [m/s|km/h|kn]
    String windspeedFormat = config->getString(config->windspeedFormat); // [m/s|km/h|kn|bft]
    String tempFormat = config->getString(config->tempFormat); // [K|C|F]

    // Read calibration settings for data instances
    for (int i = 0; i < MAX_CALIBRATION_DATA; i++) {
        calInstance = "calInstance" + String(i + 1);
        calOffset = "calOffset" + String(i + 1);
        calSlope = "calSlope" + String(i + 1);
        calSmooth = "calSmooth" + String(i + 1);

        instance = std::string(config->getString(calInstance, "---").c_str());
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "no calibration data for instance no. %d", i + 1);
            continue;
        }
        calibMap[instance] = { 0.0f, 1.0f, 1.0f, 0.0f, false };
        offset = (config->getString(calOffset, "")).toFloat();
        slope = (config->getString(calSlope, "")).toFloat();
        smooth = (config->getString(calSmooth, "")).toInt(); // user input is int; further math is done with double

        // Convert calibration values to internal standard formats
        if (instance == "AWS" || instance == "TWS") {
            if (windspeedFormat == "m/s") {
                // No conversion needed
            } else if (windspeedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                offset *= 2 + (offset / 2); // Convert Bft to m/s (approx) -> to be improved
            }

        } else if (instance == "AWA" || instance == "COG" || instance == "TWA" || instance == "TWD" || instance == "HDM" || instance == "PRPOS" || instance == "RPOS") {
            offset *= M_PI / 180; // Convert deg to rad

        } else if (instance == "DBT") {
            if (lengthFormat == "m") {
                // No conversion needed
            } else if (lengthFormat == "ft") {
                offset /= 3.28084; // Convert ft to m
            }

        } else if (instance == "SOG" || instance == "STW") {
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

        // transform smoothing factor from {0.01..10} to {0.3..0.95} and invert for exponential smoothing formula
        if (smooth <= 0) {
            smooth = 0;
        } else {
            if (smooth > 10) {
                smooth = 10;
            }
            smooth = 0.3 + ((smooth - 0.01) * (0.95 - 0.3) / (10 - 0.01));
        }
        smooth = 1 - smooth;

        calibMap[instance].offset = offset;
        calibMap[instance].slope = slope;
        calibMap[instance].smooth = smooth;
        calibMap[instance].isCalibrated = false;
        LOG_DEBUG(GwLog::LOG, "stored calibration data: %s, offset: %f, slope: %f, smoothing: %f", instance.c_str(),
            calibMap[instance].offset, calibMap[instance].slope, calibMap[instance].smooth);
    }
    LOG_DEBUG(GwLog::LOG, "all calibration data read");
}

void CalibrationDataList::calibrateInstance(GwApi::BoatValue* boatDataValue, GwLog* logger)
// Method to calibrate the boat data value
{
    std::string instance = boatDataValue->getName().c_str();
    double offset = 0;
    double slope = 1.0;
    double dataValue = 0;
    std::string format = "";

    if (calibMap.find(instance) == calibMap.end()) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not found in calibration data list", instance.c_str());
        return;
    } else if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
        calibMap[instance].isCalibrated = false;
        return;
    } else {
        offset = calibMap[instance].offset;
        slope = calibMap[instance].slope;
        dataValue = boatDataValue->value;
        format = boatDataValue->getFormat().c_str();
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: value: %f, format: %s", instance.c_str(), dataValue, format.c_str());

        if (format == "formatWind") { // instance is of type angle
            dataValue = (dataValue * slope) + offset;
            dataValue = fmod(dataValue, 2 * M_PI);
            if (dataValue > (M_PI)) {
                dataValue -= (2 * M_PI);
            } else if (dataValue < (M_PI * -1)) {
                dataValue += (2 * M_PI);
            }
        } else if (format == "formatCourse") { // instance is of type direction
            dataValue = (dataValue * slope) + offset;
            dataValue = fmod(dataValue, 2 * M_PI);
            if (dataValue < 0) {
                dataValue += (2 * M_PI);
            }
        } else if (format == "kelvinToC") { // instance is of type temperature
            dataValue = ((dataValue - 273.15) * slope) + offset + 273.15;
        } else {

            dataValue = (dataValue * slope) + offset;
        }

        calibMap[instance].isCalibrated = true;
        boatDataValue->value = dataValue;

        calibrationData.smoothInstance(boatDataValue, logger); // smooth the boat data value
        calibMap[instance].value = boatDataValue->value; // store the calibrated + smoothed value in the list

        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Offset: %f, Slope: %f, Result: %f", instance.c_str(), offset, slope, calibMap[instance].value);
    }
}

void CalibrationDataList::smoothInstance(GwApi::BoatValue* boatDataValue, GwLog* logger)
// Method to smoothen the boat data value
{
    static std::unordered_map<std::string, double> lastValue; // array for last values of smoothed boat data values

    std::string instance = boatDataValue->getName().c_str();
    double oldValue = 0;
    double dataValue = boatDataValue->value;
    double smoothFactor = 0;

    if (!boatDataValue->valid) { // no valid boat data value, so we don't want to smoothen value
        return;
    } else if (calibMap.find(instance) == calibMap.end()) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: smooth factor for %s not found in calibration data list", instance.c_str());
        return;
    } else {
        smoothFactor = calibMap[instance].smooth;

        if (lastValue.find(instance) != lastValue.end()) {
            oldValue = lastValue[instance];
            dataValue = oldValue + (smoothFactor * (dataValue - oldValue)); // exponential smoothing algorithm
        }
        lastValue[instance] = dataValue; // store the new value for next cycle; first time, store only the current value and return
        boatDataValue->value = dataValue; // set the smoothed value to the boat data value

        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Smoothing factor: %f, Smoothed value: %f", instance.c_str(), smoothFactor, dataValue);
    }
}

#endif