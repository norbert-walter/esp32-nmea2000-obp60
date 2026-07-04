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
    std::unordered_map<std::string, double> lastValue; // array for last smoothed value of boat data values
    GwLog* logger;

    static constexpr int8_t MAX_CALIBRATION_DATA = 4; // maximum number of calibration data instances

public:
    CalibrationData(GwLog* log);
    void readConfig(GwConfigHandler* config);
    void handleCalibration(BoatValueList* boatValues); // Handle calibrationMap and calibrate all boat data values
    bool calibrateInstance(GwApi::BoatValue* boatDataValue); // Calibrate single boat data value
    bool smoothInstance(GwApi::BoatValue* boatDataValue); // Smooth single boat data value
};

// Class for a single history buffer of boat values
class HstryBuf {
private:
    RingBuffer<uint16_t> hstryBuf; // Circular buffer to store history values
    String boatDataName;
    double hstryMin;
    double hstryMax;
    bool metaDataDefined = false;
    unsigned long bufUpdateTime;
    GwApi::BoatValue* boatValue;
    GwLog* logger;

    friend class HstryBuffers;

public:
    HstryBuf(const String& name, int size, BoatValueList* boatValues, GwLog* log);
    bool hasMetaData() const { return metaDataDefined; };
    void init(const String& format, int updFreq, double mltplr, double minVal, double maxVal);
    void add(double value);
    void handle(bool useSimuData, CommonData& common);
};

// Manage list of history buffers for supported boat data; used by boat data charts
class HstryBuffers {
private:
    std::map<String, std::unique_ptr<HstryBuf>> hstryBuffers;
    int size; // size of all history buffers
    BoatValueList* boatValueList;
    GwLog* logger;

    struct HistoryParams {
        int hstryUpdFreq; // update frequency of history buffer (documentation only)
        double mltplr; // specifies actual value precision being storable:
                       // [10000: 0 - 6.5535 | 1000: 0 - 65.535 | 100: 0 - 655.35 | 10: 0 - 6553.5 | 1: 0 - 65535 | 0.1: 0 - 655350]
        double bufferMinVal; // minimum valid data value
        double bufferMaxVal; // maximum valid data value
    };

    std::map<String, HistoryParams> bufferParams = {
        { "AWA", { 1000, 10000, 0.0, M_TWOPI } },
        { "AWD", { 1000, 10000, 0.0, M_TWOPI } },
        { "AWS", { 1000, 1000, 0.0, 65.0 } },
        { "COG", { 1000, 10000, 0.0, M_TWOPI } },
        { "DBK", { 1000, 100, 0.0, 650.0 } },
        { "DBS", { 1000, 100, 0.0, 650.0 } },
        { "DBT", { 1000, 100, 0.0, 650.0 } },
        { "DPT", { 1000, 100, 0.0, 650.0 } },
        { "HDM", { 1000, 10000, 0.0, M_TWOPI } },
        { "HDT", { 1000, 10000, 0.0, M_TWOPI } },
        { "ROT", { 1000, 10000, -M_PI / 180.0 * 99.0, M_PI / 180.0 * 99.0 } }, // min/max is -/+ 99 degrees for "rate of turn"
        { "SOG", { 1000, 1000, 0.0, 65.0 } },
        { "STW", { 1000, 1000, 0.0, 65.0 } },
        { "TWA", { 1000, 10000, 0.0, M_TWOPI } },
        { "TWD", { 1000, 10000, 0.0, M_TWOPI } },
        { "TWS", { 1000, 1000, 0.0, 65.0 } },
        { "WTemp", { 1000, 100, 263.15, 403.15 } }, // water temp [-10..130] °C
        { "formatXdr:C:K", { 1000, 100, 223.15, 423.15 } }, // temperature [-50..150] deg celsius
        { "formatXdr:P:B", { 60000, 1000, 0, 65.0 } }, // pressure [0..65] bar
        { "formatXdr:P:P", { 60000, 0.1, 0, 650000 } }, // pressure [0..6500] hPa
        { "formatXdr:H:P", { 1000, 100, 0, 100 } }, // humidity [0..100] percent
        { "formatXdr:I:A", { 1000, 100, 0, 650.0 } }, // current [0..650] amperes
        { "formatXdr:U:V", { 1000, 1000, 0, 65.0 } }, // voltage [0..65] volts
        { "formatXdr:T:R", { 1000, 1, 0, 30000 } }, // tachometer [0..30000] rpm
        { "formatXdr:V:L", { 10000, 100, 0, 650 } }, // volume [0..650] litres
        { "formatXdr:V:M", { 10000, 10000, 0, 6.50 } }, // volume [0..6.5] m^3
    };

public:
    HstryBuffers(int size, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name);
    void handleHstryBufs(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    GwApi::BoatValue *twaBVal, *twsBVal, *twdBVal, *maxtwsBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *awdBVal;
    GwApi::BoatValue *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    double twd, tws, twa, awd;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();
    GwLog* logger;

public:
    WindUtils(BoatValueList* boatValues, GwLog* log)
        : logger(log)
    {
        twaBVal = boatValues->findValueOrCreate("TWA");
        twsBVal = boatValues->findValueOrCreate("TWS");
        maxtwsBVal = boatValues->findValueOrCreate("MaxTws");
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
    void calcTwdSA(const double* AWA, const double* AWS, const double* AWD, const double* CTW, const double* STW, const double* HDT,
        double* TWA, double* TWS, double* TWD);
    bool calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal, double* hdtVal);
    bool calcWD(const double* waVal, const double* hdtVal, double* wdVal);
    bool calcTrueWinds(const double* awaVal, const double* awsVal, const double* awd,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        double* twdVal, double* twsVal, double* twaVal);
    void setMaxWs(GwApi::BoatValue *wsMaxValue, const double * wsVal);
    void setMaxWs() { setMaxWs(maxtwsBVal, &tws); };
    bool handleWinds(bool calcWinds);
};