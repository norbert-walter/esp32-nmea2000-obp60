// Function lib for boat data calibration, history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "Pagedata.h"
#include "obp60task.h"
#include <map>
#include <unordered_map>

// Calibration of boat data values, when user setting available
// supported boat data types are: AWA, AWS, COG, DBS, DBT, HDM, HDT, PRPOS, RPOS, SOG, STW, TWA, TWS, TWD, WTemp
class CalibrationData {
private:
    typedef struct {
        double offset; // calibration offset
        double slope; // calibration slope
        double smooth; // smoothing factor
        double value; // calibrated data value (for future use)
        bool isCalibrated; // is data instance value calibrated? (for future use)
    } tCalibrationData;

    std::unordered_map<std::string, tCalibrationData> calibrationMap; // list of calibration data instances
    std::unordered_map<std::string, double> lastValue; // array for last smoothed values of boat data values
    GwLog* logger;

    static constexpr int8_t MAX_CALIBRATION_DATA = 4; // maximum number of calibration data instances

public:
    CalibrationData(GwLog* log);
    void readConfig(GwConfigHandler* config);
    void handleCalibration(BoatValueList* boatValues); // Handle calibrationMap and calibrate all boat data values
    bool calibrateInstance(GwApi::BoatValue* boatDataValue); // Calibrate single boat data value
    bool smoothInstance(GwApi::BoatValue* boatDataValue); // Smooth single boat data value
};

class HstryBuf {
private:
    RingBuffer<uint16_t> hstryBuf; // Circular buffer to store history values
    String boatDataName;
    double hstryMin;
    double hstryMax;
    GwApi::BoatValue* boatValue;
    GwLog* logger;

    friend class HstryBuffers;

public:
    HstryBuf(const String& name, int size, BoatValueList* boatValues, GwLog* log);
    void init(const String& format, int updFreq, int mltplr, double minVal, double maxVal);
    void add(double value);
    void handle(bool useSimuData, CommonData& common);
};

class HstryBuffers {
private:
    std::map<String, std::unique_ptr<HstryBuf>> hstryBuffers;
    int size; // size of all history buffers
    BoatValueList* boatValueList;
    GwLog* logger;
    GwApi::BoatValue *awaBVal, *hdtBVal, *hdmBVal, *varBVal, *cogBVal, *sogBVal, *awdBVal; // boat values for true wind calculation

    struct HistoryParams {
        int hstryUpdFreq; // update frequency of history buffer (documentation only)
        int mltplr; // specifies actual value precision being storable:
                    // [10000: 0 - 6.5535 | 1000: 0 - 65.535 | 100: 0 - 650.35 | 10: 0 - 6503.5
        double bufferMinVal; // minimum valid data value
        double bufferMaxVal; // maximum valid data value
        String format; // format of data type
    };

    // Define buffer parameters for supported boat data type
    std::map<String, HistoryParams> bufferParams = {
        { "AWA", { 1000, 10000, 0.0, M_TWOPI, "formatWind" } },
        { "AWD", { 1000, 10000, 0.0, M_TWOPI, "formatCourse" } },
        { "AWS", { 1000, 1000, 0.0, 65.0, "formatKnots" } },
        { "COG", { 1000, 10000, 0.0, M_TWOPI, "formatCourse" } },
        { "DBS", { 1000, 100, 0.0, 650.0, "formatDepth" } },
        { "DBT", { 1000, 100, 0.0, 650.0, "formatDepth" } },
        { "DPT", { 1000, 100, 0.0, 650.0, "formatDepth" } },
        { "HDM", { 1000, 10000, 0.0, M_TWOPI, "formatCourse" } },
        { "HDT", { 1000, 10000, 0.0, M_TWOPI, "formatCourse" } },
        { "ROT", { 1000, 10000, -M_PI / 180.0 * 99.0, M_PI / 180.0 * 99.0, "formatRot" } }, // min/max is -/+ 99 degrees for "rate of turn"
        { "SOG", { 1000, 1000, 0.0, 65.0, "formatKnots" } },
        { "STW", { 1000, 1000, 0.0, 65.0, "formatKnots" } },
        { "TWA", { 1000, 10000, 0.0, M_TWOPI, "formatWind" } },
        { "TWD", { 1000, 10000, 0.0, M_TWOPI, "formatCourse" } },
        { "TWS", { 1000, 1000, 0.0, 65.0, "formatKnots" } },
        { "WTemp", { 1000, 100, 233.0, 650.0, "kelvinToC" } } // [-50..376] °C
    };

public:
    HstryBuffers(int size, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name);
    void handleHstryBufs(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    GwApi::BoatValue *twaBVal, *twsBVal, *twdBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *awdBVal, *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();
    GwLog* logger;

public:
    WindUtils(BoatValueList* boatValues, GwLog* log)
        : logger(log)
    {
        twaBVal = boatValues->findValueOrCreate("TWA");
        twsBVal = boatValues->findValueOrCreate("TWS");
        twdBVal = boatValues->findValueOrCreate("TWD");
        awaBVal = boatValues->findValueOrCreate("AWA");
        awsBVal = boatValues->findValueOrCreate("AWS");
        awdBVal = boatValues->findValueOrCreate("AWD");
        cogBVal = boatValues->findValueOrCreate("COG");
        stwBVal = boatValues->findValueOrCreate("STW");
        sogBVal = boatValues->findValueOrCreate("SOG");
        hdtBVal = boatValues->findValueOrCreate("HDT");
        hdmBVal = boatValues->findValueOrCreate("HDM");
        varBVal = boatValues->findValueOrCreate("VAR");
    };

    static double to2PI(double a);
    static double toPI(double a);
    static double to360(double a);
    static double to180(double a);
    void toCart(const double* phi, const double* r, double* x, double* y);
    void toPol(const double* x, const double* y, double* phi, double* r);
    void addPolar(const double* phi1, const double* r1,
        const double* phi2, const double* r2,
        double* phi, double* r);
    void calcTwdSA(const double* AWA, const double* AWS,
        const double* CTW, const double* STW, const double* HDT,
        double* TWD, double* TWS, double* TWA, double* AWD);
    static double calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal);
    bool calcWinds(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal, double* awdVal);
    bool addWinds();
};