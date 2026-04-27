#include "OBPDataOperations.h"

// --- Class CalibrationData ---------------
CalibrationData::CalibrationData(GwLog* log)
{
    logger = log;
}

void CalibrationData::readConfig(GwConfigHandler* config)
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
            LOG_DEBUG(GwLog::LOG, "No calibration data for instance no. %d", i + 1);
            continue;
        }

        calibrationMap[instance] = { 0.0f, 1.0f, 1.0f, 0.0f, false };
        offset = (config->getString(calOffset, "")).toDouble();
        slope = (config->getString(calSlope, "")).toDouble();
        smooth = (config->getString(calSmooth, "")).toInt(); // user input is int; further math is done with double

        if (slope == 0.0) {
            slope = 1.0; // eliminate adjustment if user selected "0" -> that would set the calibrated value to "0"
        }

        // Convert calibration values from user input format to internal standard SI format
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

        } else if (instance == "AWA" || instance == "COG" || instance == "HDM" || instance == "HDT" || instance == "PRPOS" || instance == "RPOS" || instance == "TWA" || instance == "TWD") {
            offset *= DEG_TO_RAD; // Convert deg to rad

        } else if (instance == "DBS" || instance == "DBT") {
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

        // transform smoothing factor from [0.01..10] to [0.3..0.95] and invert for exponential smoothing formula
        if (smooth <= 0) {
            smooth = 0;
        } else {
            if (smooth > 10) {
                smooth = 10;
            }
            smooth = 0.3 + ((smooth - 0.01) * (0.95 - 0.3) / (10 - 0.01));
        }
        smooth = 1 - smooth;

        calibrationMap[instance].offset = offset;
        calibrationMap[instance].slope = slope;
        calibrationMap[instance].smooth = smooth;
        calibrationMap[instance].isCalibrated = false;
        LOG_DEBUG(GwLog::LOG, "Calibration data type added: %s, offset: %f, slope: %f, smoothing: %f", instance.c_str(),
            calibrationMap[instance].offset, calibrationMap[instance].slope, calibrationMap[instance].smooth);
    }
    // LOG_DEBUG(GwLog::LOG, "All calibration data read");
}

// Handle calibrationMap and calibrate all boat data values
void CalibrationData::handleCalibration(BoatValueList* boatValueList)
{
    GwApi::BoatValue* bValue;

    for (const auto& cMap : calibrationMap) {
        std::string instance = cMap.first.c_str();
        bValue = boatValueList->findValueOrCreate(String(instance.c_str()));

        calibrateInstance(bValue);
        smoothInstance(bValue);
    }
}

// Calibrate single boat data value
// Return calibrated boat value or DBL_MAX, if no calibration was performed
bool CalibrationData::calibrateInstance(GwApi::BoatValue* boatDataValue)
{
    std::string instance = boatDataValue->getName().c_str();
    double offset = 0;
    double slope = 1.0;
    double dataValue = 0;
    std::string format = "";

    // we test this earlier, but for safety reasons ...
    if (calibrationMap.find(instance) == calibrationMap.end()) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", instance.c_str());
        return false;
    }

    calibrationMap[instance].isCalibrated = false; // reset calibration flag until properly calibrated

    if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
        return false;
    }

    offset = calibrationMap[instance].offset;
    slope = calibrationMap[instance].slope;
    dataValue = boatDataValue->value;
    format = boatDataValue->getFormat().c_str();
    // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: value: %f, format: %s", instance.c_str(), dataValue, format.c_str());

    if (format == "formatWind") { // instance is of type angle
        dataValue = (dataValue * slope) + offset;
        dataValue = WindUtils::to2PI(dataValue); // wind angle data is stored in [0..2PI] format

    } else if (format == "formatCourse") { // instance is of type direction
        dataValue = (dataValue * slope) + offset;
        dataValue = WindUtils::to2PI(dataValue);

    } else if (format == "kelvinToC") { // instance is of type temperature
        dataValue = ((dataValue - 273.15) * slope) + offset + 273.15;

    } else {
        dataValue = (dataValue * slope) + offset;
    }

    boatDataValue->value = dataValue; // update boat data value with calibrated value
    calibrationMap[instance].value = dataValue; // store the calibrated value in the list
    calibrationMap[instance].isCalibrated = true;

    // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Offset: %f, Slope: %f, Result: %f", instance.c_str(), offset, slope, calibrationMap[instance].value);
    return true;
}

// Smooth single boat data value
// Return smoothed boat value or DBL_MAX, if no smoothing was performed
bool CalibrationData::smoothInstance(GwApi::BoatValue* boatDataValue)
{
    std::string instance = boatDataValue->getName().c_str();
    double oldValue = 0;
    double dataValue = boatDataValue->value;
    double smoothFactor = 0;

    // we test this earlier, but for safety reason ...
    if (calibrationMap.find(instance) == calibrationMap.end()) {
        // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", instance.c_str());
        return false;
    }

    calibrationMap[instance].isCalibrated = false; // reset calibration flag until properly calibrated

    if (!boatDataValue->valid) { // no valid boat data value, so we don't need to do anything
        return false;
    }

    smoothFactor = calibrationMap[instance].smooth;

    if (lastValue.find(instance) != lastValue.end()) {
        oldValue = lastValue[instance];
        dataValue = oldValue + (smoothFactor * (dataValue - oldValue)); // exponential smoothing algorithm
    }
    lastValue[instance] = dataValue; // store the new value for next cycle; first time, store only the current value and return

    boatDataValue->value = dataValue; // update boat data value with smoothed value
    calibrationMap[instance].value = dataValue; // store the smoothed value in the list
    calibrationMap[instance].isCalibrated = true;

    // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: smooth: %f, oldValue: %f, result: %f", instance.c_str(), smoothFactor, oldValue, calibrationMap[instance].value);

    return true;
}
// --- End Class CalibrationData ---------------

// --- Class HstryBuf ---------------
HstryBuf::HstryBuf(const String& name, int size, BoatValueList* boatValues, GwLog* log)
    : logger(log)
    , boatDataName(name)
{
    hstryBuf.resize(size);
    boatValue = boatValues->findValueOrCreate(name);
}

void HstryBuf::init(const String& format, int updFreq, int mltplr, double minVal, double maxVal)
{
    hstryBuf.setMetaData(boatDataName, format, updFreq, mltplr, minVal, maxVal);
    hstryMin = minVal;
    hstryMax = maxVal;
    if (!boatValue->valid) {
        boatValue->setFormat(format);
        boatValue->value = std::numeric_limits<double>::max(); // mark current value invalid
    }
}

void HstryBuf::add(double value)
{
    if (value >= hstryMin && value <= hstryMax) {
        hstryBuf.add(value);
        // LOG_DEBUG(GwLog::DEBUG, "HstryBuf::add:  name: %s, value: %.3f", hstryBuf.getName(), value);
    }
}

void HstryBuf::handle(bool useSimuData, CommonData& common)
{
    std::unique_ptr<GwApi::BoatValue> tmpBVal; // Temp variable to get formatted and converted data value from OBP60Formatter

    if (boatValue->valid) {
        add(boatValue->value);
    } else if (useSimuData) { // add simulated value to history buffer
        tmpBVal = std::unique_ptr<GwApi::BoatValue>(new GwApi::BoatValue(boatDataName)); // create temporary boat value for retrieval of simulation value
        tmpBVal->setFormat(boatValue->getFormat());
        tmpBVal->value = boatValue->value;
        tmpBVal->valid = boatValue->valid;
        double simSIValue = formatValue(tmpBVal.get(), common).value; // simulated value is generated at <formatValue>; here: retreive SI value
        add(simSIValue);
    } else {
        // here we will add invalid (DBL_MAX) value; this will mark periods of missing data in buffer together with a timestamp
    }
}
// --- End Class HstryBuf ---------------

// --- Class HstryBuffers ---------------
HstryBuffers::HstryBuffers(int size, BoatValueList* boatValues, GwLog* log)
    : size(size)
    , boatValueList(boatValues)
    , logger(log) { };

// Create history buffer for boat data type
void HstryBuffers::addBuffer(const String& name)
{
    if (HstryBuffers::getBuffer(name) != nullptr) { // buffer for this data type already exists
        return;
    }
    if (bufferParams.find(name) == bufferParams.end()) { // requested boat data type is not supported in list of <bufferParams>
        return;
    }

    // Initialize metadata for buffer
    String valueFormat = bufferParams[name].format; // Data format of boat data type
    // String valueFormat = boatValueList->findValueOrCreate(name)->getFormat().c_str(); // Unfortunately, format is not yet available during system initialization
    int hstryUpdFreq = bufferParams[name].hstryUpdFreq; // Update frequency for history buffers in ms
    int mltplr = bufferParams[name].mltplr; // default multiplier which transforms original <double> value into buffer type format
    double bufferMinVal = bufferParams[name].bufferMinVal; // Min value for this history buffer
    double bufferMaxVal = bufferParams[name].bufferMaxVal; // Max value for this history buffer

    hstryBuffers[name] = std::unique_ptr<HstryBuf>(new HstryBuf(name, size, boatValueList, logger));
    hstryBuffers[name]->init(valueFormat, hstryUpdFreq, mltplr, bufferMinVal, bufferMaxVal);
    LOG_DEBUG(GwLog::DEBUG, "HstryBuffers: new buffer added: name: %s, format: %s, multiplier: %d, min value: %.2f, max value: %.2f", name, valueFormat, mltplr, bufferMinVal, bufferMaxVal);
}

// Handle all registered history buffers
void HstryBuffers::handleHstryBufs(bool useSimuData, CommonData& common)
{
    for (auto& bufMap : hstryBuffers) {
        auto& buf = bufMap.second;
        buf->handle(useSimuData, common);
    }
}

RingBuffer<uint16_t>* HstryBuffers::getBuffer(const String& name)
{
    auto it = hstryBuffers.find(name);
    if (it != hstryBuffers.end()) {
        return &it->second->hstryBuf;
    } else {
        return nullptr;
    }
}
// --- End Class HstryBuffers ---------------

// --- Class WindUtils --------------
double WindUtils::to2PI(double a)
{
    a = fmod(a, M_TWOPI);
    if (a < 0.0) {
        a += M_TWOPI;
    }
    return a;
}

double WindUtils::toPI(double a)
{
    a += M_PI;
    a = to2PI(a);
    a -= M_PI;

    return a;
}

double WindUtils::to360(double a)
{
    a = fmod(a, 360.0);
    if (a < 0.0) {
        a += 360.0;
    }
    return a;
}

double WindUtils::to180(double a)
{
    a += 180.0;
    a = to360(a);
    a -= 180.0;

    return a;
}

void WindUtils::toCart(const double* phi, const double* r, double* x, double* y)
{
    *x = *r * sin(*phi);
    *y = *r * cos(*phi);
}

void WindUtils::toPol(const double* x, const double* y, double* phi, double* r)
{
    *phi = (M_PI / 2) - atan2(*y, *x);
    *phi = to2PI(*phi);
    *r = sqrt(*x * *x + *y * *y);
}

void WindUtils::addPolar(const double* phi1, const double* r1,
    const double* phi2, const double* r2,
    double* phi, double* r)
{
    double x1, y1, x2, y2;
    toCart(phi1, r1, &x1, &y1);
    toCart(phi2, r2, &x2, &y2);
    x1 += x2;
    y1 += y2;
    toPol(&x1, &y1, phi, r);
}

void WindUtils::calcTwdSA(const double* AWA, const double* AWS, const double* AWD, const double* CTW, const double* STW, const double* HDT,
    double* TWA, double* TWS, double* TWD)
{
    double stw = -*STW;
    addPolar(AWD, AWS, CTW, &stw, TWD, TWS);

    // Normalize to [0..2PI]
    *TWD = to2PI(*TWD);
    *TWA = to2PI(*TWD - *HDT); // for internal storage we normalize wind angle to [0..2PI], as well
}

// calculate HDT from either HDM + VAR, HDM only, or COG
bool WindUtils::calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal, double* hdtVal)
{
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if (*hdtVal != DBL_MAX) {
        // HDT already available-> nothing to do
        return true;
    }

    if (*hdmVal != DBL_MAX) {
        *hdtVal = *hdmVal + (*varVal != DBL_MAX ? *varVal : 0.0); // Use corrected HDM if HDT is not available (or just HDM if VAR is not available)
        *hdtVal = to2PI(*hdtVal);
    } else if (*cogVal != DBL_MAX && *sogVal >= minSogVal) {
        *hdtVal = *cogVal; // Use COG as fallback if HDT and HDM are not available, and SOG is not data noise
    } else {
        *hdtVal = DBL_MAX; // Cannot calculate HDT without valid HDM or HDM+VAR or COG
        return false;
    }
    // LOG_DEBUG(GwLog::DEBUG, "WindUtils:calcHDT: HDT: %.1f, HDM %.1f, VAR %.1f, COG %.1f, SOG %.1f", *hdtVal * RAD_TO_DEG, *hdmVal * RAD_TO_DEG, *varVal * RAD_TO_DEG,
    //      *cogVal * RAD_TO_DEG, *sogVal * 3.6 / 1.852);

    return true;
}

// calculate wind direction (either TWD or AWD) from wind angle (TWA or AWA) and HDT
bool WindUtils::calcWD(const double* waVal, const double* hdtVal, double* wdVal)
{
    if (*waVal != DBL_MAX && *hdtVal != DBL_MAX) {
        *wdVal = *waVal + *hdtVal;
        *wdVal = to2PI(*wdVal);
    } else {
        *wdVal = DBL_MAX; // Cannot calculate wind direction WD without valid wind angle WA and HDT
        return false;
    }

    return true;
}

// calculate full set of true winds (TWA, TWS, TWD) apparent winds and more data
bool WindUtils::calcTrueWinds(const double* awaVal, const double* awsVal, const double* awd,
    const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
    double* twaVal, double* twsVal, double* twdVal)
{
    double stw, ctw;
    double twd, tws, twa;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if ((*awaVal == DBL_MAX) || (*awsVal == DBL_MAX)) {
        // Cannot calculate true winds at all without valid AWA, AWS
        return false;
    }

    if (*cogVal != DBL_MAX && *sogVal >= minSogVal) { // if SOG is data noise, we don't trust COG

        ctw = *cogVal; // Use COG for CTW if available
    } else {
        ctw = *hdtVal; // 2nd approximation for CTW; HDT must exist if we reach this part of code
    }

    if (*stwVal != DBL_MAX) {
        stw = *stwVal; // Use STW if available
    } else if (*sogVal != DBL_MAX) {
        stw = *sogVal;
    } else {
        // If STW and SOG are not available, we cannot calculate true wind
        return false;
    }
    // LOG_DEBUG(GwLog::DEBUG, "WindUtils:calcTrueWinds: HDT: %.1f, CTW %.1f, STW %.1f", *hdtVal * RAD_TO_DEG, ctw * RAD_TO_DEG, stw * 3.6 / 1.852);

    calcTwdSA(awaVal, awsVal, awd, &ctw, &stw, hdtVal, &twa, &tws, &twd);
    *twaVal = twa;
    *twsVal = tws;
    *twdVal = twd;

    return true;
}

// Calculate true wind data and add to obp60task boat data list
bool WindUtils::handleWinds(bool calcWinds)
{
    double twd, tws, twa, awd;
    bool twCalculated = false;

    if (!calcWinds) { // don't calculate anything if true wind calculation has not been set in configuration
        return twCalculated;
    }

    double awaVal = awaBVal->valid ? awaBVal->value : DBL_MAX;
    double awsVal = awsBVal->valid ? awsBVal->value : DBL_MAX;
    double cogVal = cogBVal->valid ? cogBVal->value : DBL_MAX;
    double stwVal = stwBVal->valid ? stwBVal->value : DBL_MAX;
    double sogVal = sogBVal->valid ? sogBVal->value : DBL_MAX;
    double hdtVal = hdtBVal->valid ? hdtBVal->value : DBL_MAX;
    double hdmVal = hdmBVal->valid ? hdmBVal->value : DBL_MAX;
    double varVal = varBVal->valid ? varBVal->value : DBL_MAX;
    double twaVal = twaBVal->valid ? twaBVal->value : DBL_MAX;
    double twsVal = twsBVal->valid ? twsBVal->value : DBL_MAX;
    // LOG_DEBUG(GwLog::DEBUG, "WindUtils:handleWinds: AWA %.1f, AWS %.1f, AWD %.1f, COG %.1f, STW %.1f, SOG %.2f, HDT %.1f, HDM %.1f, VAR %.1f", awaVal * RAD_TO_DEG, awsVal * 3.6 / 1.852,
    //     awd * RAD_TO_DEG, cogVal * RAD_TO_DEG, stwVal * 3.6 / 1.852, sogVal * 3.6 / 1.852, hdtVal * RAD_TO_DEG, hdmVal * RAD_TO_DEG, varVal * RAD_TO_DEG);

    if (calcHDT(&hdmVal, &varVal, &cogVal, &sogVal, &hdtVal)) {
        hdtBVal->value = hdtVal;
        hdtBVal->valid = true;
    } else {
        // determination of HDT was not possible due to missing prerequisite data
        hdtBVal->valid = false;
        return false;
    }

    // calculate AWD if not existing and if possible
    if (!awdBVal->valid) {
        if (calcWD(&awaVal, &hdtVal, &awd)) {
            awdBVal->value = awd;
            awdBVal->valid = true;
        } else {
            awdBVal->valid = false;
            return false;
        }
    }

    // calculate TWD if not existing and if possible
    if (!twdBVal->valid) {
        // calculate TWD if it does not exist yet and TWA is available
        if (calcWD(&twaVal, &hdtVal, &twd)) {
            twdBVal->value = twd;
            twdBVal->valid = true;
        } else {
            twdBVal->valid = false;
        }
    }

    if (!twaBVal->valid || !twsBVal->valid || !twdBVal->valid) {
        // calculate true winds at least one of three true wind values does not exist
        twCalculated = calcTrueWinds(&awaVal, &awsVal, &awd, &cogVal, &stwVal, &sogVal, &hdtVal, &twa, &tws, &twd);
        if (twCalculated) { // Replace values only, if successfully calculated and not already available
            if (!twaBVal->valid) {
                twaBVal->value = twa;
                twaBVal->valid = true;
            }
            if (!twsBVal->valid) {
                twsBVal->value = tws;
                twsBVal->valid = true;
            }
            if (!twdBVal->valid) {
                twdBVal->value = twd;
                twdBVal->valid = true;
            }
        }
    }
    // LOG_DEBUG(GwLog::DEBUG, "WindUtils:handleWinds: twCalculated %d, TWD %.1f, TWA %.1f, TWS %.2f kn, AWD: %.1f", twCalculated, twdBVal->value * RAD_TO_DEG,
    //     twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852, awdBVal->value * RAD_TO_DEG);

    return twCalculated;
}
// --- End Class WindUtils --------------