#include "OBPDataOperations.h"
//#include "BoatDataCalibration.h" // Functions lib for data instance calibration

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

    // we test this earlier, but for safety reason ...
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
    LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: value: %f, format: %s", instance.c_str(), dataValue, format.c_str());

    if (format == "formatWind") { // instance is of type angle
        dataValue = (dataValue * slope) + offset;
        // dataValue = WindUtils::toPI(dataValue);
        dataValue = WindUtils::to2PI(dataValue); // we should call <toPI> for format of [-180..180], but pages cannot display negative values properly yet

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

    LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Offset: %f, Slope: %f, Result: %f", instance.c_str(), offset, slope, calibrationMap[instance].value);
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
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", instance.c_str());
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

    LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: smooth: %f, oldValue: %f, result: %f", instance.c_str(), smoothFactor, oldValue, calibrationMap[instance].value);

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
        LOG_DEBUG(GwLog::DEBUG, "HstryBuf::add:  name: %s, value: %.3f", hstryBuf.getName(), value);
    }
}

void HstryBuf::handle(bool useSimuData, CommonData& common)
{
    // GwApi::BoatValue* tmpBVal;
    std::unique_ptr<GwApi::BoatValue> tmpBVal; // Temp variable to get formatted and converted data value from OBP60Formatter

    // create temporary boat value for calibration purposes and retrieval of simulation value
    // tmpBVal = new GwApi::BoatValue(boatDataName.c_str());
    tmpBVal = std::unique_ptr<GwApi::BoatValue>(new GwApi::BoatValue(boatDataName));
    tmpBVal->setFormat(boatValue->getFormat());
    tmpBVal->value = boatValue->value;
    tmpBVal->valid = boatValue->valid;

    if (boatValue->valid) {
        // Calibrate boat value before adding it to history buffer
        //calibrationData.calibrateInstance(tmpBVal.get(), logger);
        //add(tmpBVal->value);
        add(boatValue->value);

    } else if (useSimuData) { // add simulated value to history buffer
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
    , logger(log)
{

    // collect boat values for true wind calculation
    // should all have been already created at true wind object initialization
    // potentially to be moved to history buffer handling
    awaBVal = boatValueList->findValueOrCreate("AWA");
    hdtBVal = boatValueList->findValueOrCreate("HDT");
    hdmBVal = boatValueList->findValueOrCreate("HDM");
    varBVal = boatValueList->findValueOrCreate("VAR");
    cogBVal = boatValueList->findValueOrCreate("COG");
    sogBVal = boatValueList->findValueOrCreate("SOG");
    awdBVal = boatValueList->findValueOrCreate("AWD");
}

// Create history buffer for boat data type
void HstryBuffers::addBuffer(const String& name)
{
    if (HstryBuffers::getBuffer(name) != nullptr) { // buffer for this data type already exists
        return;
    }
    if (bufferParams.find(name) == bufferParams.end()) { // requested boat data type is not supported in list of <bufferParams>
        return;
    }

    hstryBuffers[name] = std::unique_ptr<HstryBuf>(new HstryBuf(name, size, boatValueList, logger));

    // Initialize metadata for buffer
    String valueFormat = bufferParams[name].format; // Data format of boat data type
    // String valueFormat = boatValueList->findValueOrCreate(name)->getFormat().c_str(); // Unfortunately, format is not yet available during system initialization
    int hstryUpdFreq = bufferParams[name].hstryUpdFreq; // Update frequency for history buffers in ms
    int mltplr = bufferParams[name].mltplr; // default multiplier which transforms original <double> value into buffer type format
    double bufferMinVal = bufferParams[name].bufferMinVal; // Min value for this history buffer
    double bufferMaxVal = bufferParams[name].bufferMaxVal; // Max value for this history buffer

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
    }
    return nullptr;
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

void WindUtils::calcTwdSA(const double* AWA, const double* AWS,
    const double* CTW, const double* STW, const double* HDT,
    double* TWD, double* TWS, double* TWA, double* AWD)
{
    *AWD = *AWA + *HDT;
    *AWD = to2PI(*AWD);
    double stw = -*STW;
    addPolar(AWD, AWS, CTW, &stw, TWD, TWS);

    // Normalize TWD and TWA to 0-360°/2PI
    *TWD = to2PI(*TWD);
    *TWA = toPI(*TWD - *HDT);
}

double WindUtils::calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal)
{
    double hdt;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if (*hdmVal != DBL_MAX) {
        hdt = *hdmVal + (*varVal != DBL_MAX ? *varVal : 0.0); // Use corrected HDM if HDT is not available (or just HDM if VAR is not available)
        hdt = to2PI(hdt);
    } else if (*cogVal != DBL_MAX && *sogVal >= minSogVal) {
        hdt = *cogVal; // Use COG as fallback if HDT and HDM are not available, and SOG is not data noise
    } else {
        hdt = DBL_MAX; // Cannot calculate HDT without valid HDM or HDM+VAR or COG
    }

    return hdt;
}

bool WindUtils::calcWinds(const double* awaVal, const double* awsVal,
    const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
    const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal, double* awdVal)
{
    double stw, hdt, ctw;
    double twd, tws, twa, awd;
    double minSogVal = 0.1; // SOG below this value (m/s) is assumed to be  data noise from GPS sensor

    if (*hdtVal != DBL_MAX) {
        hdt = *hdtVal; // Use HDT if available
    } else {
        hdt = calcHDT(hdmVal, varVal, cogVal, sogVal);
    }

    if (*cogVal != DBL_MAX && *sogVal >= minSogVal) { // if SOG is data noise, we don't trust COG

        ctw = *cogVal; // Use COG for CTW if available
    } else {
        ctw = hdt; // 2nd approximation for CTW; hdt must exist if we reach this part of the code
    }

    if (*stwVal != DBL_MAX) {
        stw = *stwVal; // Use STW if available
    } else if (*sogVal != DBL_MAX) {
        stw = *sogVal;
    } else {
        // If STW and SOG are not available, we cannot calculate true wind
        return false;
    }
    // LOG_DEBUG(GwLog::DEBUG, "WindUtils:calcWinds: HDT: %.1f, CTW %.1f, STW %.1f", hdt, ctw, stw);

    if ((*awaVal == DBL_MAX) || (*awsVal == DBL_MAX)) {
        // Cannot calculate true wind without valid AWA, AWS; other checks are done earlier
        return false;
    } else {
        calcTwdSA(awaVal, awsVal, &ctw, &stw, &hdt, &twd, &tws, &twa, &awd);
        *twdVal = twd;
        *twsVal = tws;
        *twaVal = twa;
        *awdVal = awd;

        return true;
    }
}

// Calculate true wind data and add to obp60task boat data list
bool WindUtils::addWinds()
{
    double twd, tws, twa, awd, hdt;
    bool twCalculated = false;
    bool awdCalculated = false;

    double awaVal = awaBVal->valid ? awaBVal->value : DBL_MAX;
    double awsVal = awsBVal->valid ? awsBVal->value : DBL_MAX;
    double cogVal = cogBVal->valid ? cogBVal->value : DBL_MAX;
    double stwVal = stwBVal->valid ? stwBVal->value : DBL_MAX;
    double sogVal = sogBVal->valid ? sogBVal->value : DBL_MAX;
    double hdtVal = hdtBVal->valid ? hdtBVal->value : DBL_MAX;
    double hdmVal = hdmBVal->valid ? hdmBVal->value : DBL_MAX;
    double varVal = varBVal->valid ? varBVal->value : DBL_MAX;
    LOG_DEBUG(GwLog::DEBUG, "WindUtils:addWinds: AWA %.1f, AWS %.1f, COG %.1f, STW %.1f, SOG %.2f, HDT %.1f, HDM %.1f, VAR %.1f", awaBVal->value * RAD_TO_DEG, awsBVal->value * 3.6 / 1.852,
        cogBVal->value * RAD_TO_DEG, stwBVal->value * 3.6 / 1.852, sogBVal->value * 3.6 / 1.852, hdtBVal->value * RAD_TO_DEG, hdmBVal->value * RAD_TO_DEG, varBVal->value * RAD_TO_DEG);

    // Check if TWD can be calculated from TWA and HDT/HDM
    if (twaBVal->valid) {
        if (!twdBVal->valid) {
            if (hdtVal != DBL_MAX) {
                hdt = hdtVal; // Use HDT if available
            } else {
                hdt = calcHDT(&hdmVal, &varVal, &cogVal, &sogVal);
            }
            twd = twaBVal->value + hdt;
            twd = to2PI(twd);
            twdBVal->value = twd;
            twdBVal->valid = true;
        }

    } else {
        // Calculate true winds and AWD; if true winds exist, use at least AWD calculation
        twCalculated = calcWinds(&awaVal, &awsVal, &cogVal, &stwVal, &sogVal, &hdtVal, &hdmVal, &varVal, &twd, &tws, &twa, &awd);

        if (twCalculated) { // Replace values only, if successfully calculated and not already available
            if (!twdBVal->valid) {
                twdBVal->value = twd;
                twdBVal->valid = true;
            }
            if (!twsBVal->valid) {
                twsBVal->value = tws;
                twsBVal->valid = true;
            }
            if (!twaBVal->valid) {
                //twaBVal->value = twa;
                twaBVal->value = to2PI(twa); // convert to [0..360], because pages cannot display negative values properly yet
                twaBVal->valid = true;
            }
            if (!awdBVal->valid) {
                awdBVal->value = awd;
                awdBVal->valid = true;
            }
        }
    }
    LOG_DEBUG(GwLog::DEBUG, "WindUtils:addWinds: twCalculated %d, TWD %.1f, TWA %.1f, TWS %.2f kn, AWD: %.1f", twCalculated, twdBVal->value * RAD_TO_DEG,
        twaBVal->value * RAD_TO_DEG, twsBVal->value * 3.6 / 1.852, awdBVal->value * RAD_TO_DEG);

    return twCalculated;
}
// --- End Class WindUtils --------------