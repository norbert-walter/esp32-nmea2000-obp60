#pragma once
#include <N2kMessages.h>
#include "OBPRingBuffer.h"
#include "BoatDataCalibration.h"        // Functions lib for data instance calibration
#include "obp60task.h"
#include <math.h>

typedef struct {
    RingBuffer<int16_t>* twdHstry;
    RingBuffer<uint16_t>* twsHstry;
    RingBuffer<int16_t>* awdHstry;
    RingBuffer<uint16_t>* awsHstry;
} tBoatHstryData; // Holds pointers to all history buffers for boat data

class HstryBuf {
private:
    GwLog *logger;

    RingBuffer<int16_t> twdHstry; // Circular buffer to store true wind direction values
    RingBuffer<uint16_t> twsHstry; // Circular buffer to store true wind speed values (TWS)
    RingBuffer<int16_t> awdHstry; // Circular buffer to store apparant wind direction values
    RingBuffer<uint16_t> awsHstry; // Circular buffer to store apparant xwind speed values (AWS)
    int16_t twdHstryMin; // Min value for wind direction (TWD) in history buffer
    int16_t twdHstryMax; // Max value for wind direction (TWD) in history buffer
    uint16_t twsHstryMin;
    uint16_t twsHstryMax;
    int16_t awdHstryMin;
    int16_t awdHstryMax;
    uint16_t awsHstryMin;
    uint16_t awsHstryMax;

    // boat values for buffers and for true wind calculation
    GwApi::BoatValue *twdBVal, *twsBVal, *twaBVal, *awdBVal, *awsBVal;
    GwApi::BoatValue *awaBVal, *hdtBVal, *hdmBVal, *varBVal, *cogBVal, *sogBVal;

public:
    tBoatHstryData hstryBufList;

    HstryBuf(){
        hstryBufList = {&twdHstry, &twsHstry, &awdHstry, &awsHstry}; // Generate history buffers of zero size
    };
    HstryBuf(int size) {
        hstryBufList = {&twdHstry, &twsHstry, &awdHstry, &awsHstry}; 
        hstryBufList.twdHstry->resize(960); // store 960 TWD values for 16 minutes history
        hstryBufList.twsHstry->resize(960);
        hstryBufList.awdHstry->resize(960);
        hstryBufList.awsHstry->resize(960);
    };
    void init(BoatValueList* boatValues, GwLog *log);
    void handleHstryBuf(bool useSimuData);
};

class WindUtils {
private:
    GwApi::BoatValue *twdBVal, *twsBVal, *twaBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    static constexpr double DBL_MAX = std::numeric_limits<double>::max();

public:
    WindUtils(BoatValueList* boatValues){
        twdBVal = boatValues->findValueOrCreate("TWD");
        twsBVal = boatValues->findValueOrCreate("TWS");
        twaBVal = boatValues->findValueOrCreate("TWA");
        awaBVal = boatValues->findValueOrCreate("AWA");
        awsBVal = boatValues->findValueOrCreate("AWS");
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
        double* TWD, double* TWS, double* TWA);
    static double calcHDT(const double* hdmVal, const double* varVal, const double* cogVal, const double* sogVal);
    bool calcTrueWind(const double* awaVal, const double* awsVal,
        const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
        const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal);
    bool addTrueWind(GwApi* api, BoatValueList* boatValues, GwLog *log);
};