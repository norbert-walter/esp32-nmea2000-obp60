#include "OBPDataOperations.h"

double WindUtils::to2PI(double a)
{
    a = fmod(a, 2 * M_PI);
    if (a < 0.0) {
        a += 2 * M_PI;
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
    a = fmod(a, 360);
    if (a < 0.0) {
        a += 360;
    }
    return a;
}

double WindUtils::to180(double a)
{
    a += 180;
    a = to360(a);
    a -= 180;

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
    double* TWD, double* TWS, double* TWA)
{
    double awd = *AWA + *HDT;
    awd = to2PI(awd);
    double stw = -*STW;
    //    Serial.println("\ncalcTwdSA: AWA: " + String(*AWA) + ", AWS: " + String(*AWS) + ", CTW: " + String(*CTW) + ", STW: " + String(*STW) + ", HDT: " + String(*HDT));
    addPolar(&awd, AWS, CTW, &stw, TWD, TWS);

    // Normalize TWD and TWA to 0-360°
    *TWD = to2PI(*TWD);
    *TWA = toPI(*TWD - *HDT);
    //    Serial.println("calcTwdSA: TWD: " + String(*TWD) + ", TWS: " + String(*TWS));
}

bool WindUtils::calcTrueWind(const double* awaVal, const double* awsVal,
    const double* cogVal, const double* stwVal, const double* sogVal, const double* hdtVal,
    const double* hdmVal, const double* varVal, double* twdVal, double* twsVal, double* twaVal)
{
    double stw, hdt, ctw;
    double twd, tws, twa;
    static const double DBL_MIN = std::numeric_limits<double>::lowest();

    if (*hdtVal != DBL_MIN) {
        hdt = *hdtVal; // Use HDT if available
    } else {
        if (*hdmVal != DBL_MIN && *varVal != DBL_MIN) {
            hdt = *hdmVal + *varVal; // Use corrected HDM if HDT is not available
            hdt = to2PI(hdt);
        } else if (*cogVal != DBL_MIN) {
            hdt = *cogVal; // Use COG as fallback if HDT and HDM are not available
        } else {
            return false; // Cannot calculate without valid HDT or HDM
        }
    }

    if (*cogVal != DBL_MIN) {
        ctw = *cogVal; // Use COG as CTW if available
        //    ctw = *cogVal + ((*cogVal - hdt) / 2); // Estimate CTW from COG
    } else {
        ctw = hdt; // 2nd approximation for CTW;
        return false;
    }

    if (*stwVal != DBL_MIN) {
        stw = *stwVal; // Use STW if available
    } else if (*sogVal != DBL_MIN) {
        stw = *sogVal;
    } else {
        // If STW and SOG are not available, we cannot calculate true wind
        return false;
    }

    if ((*awaVal == DBL_MIN) || (*awsVal == DBL_MIN) || (*cogVal == DBL_MIN) || (*stwVal == DBL_MIN)) {
        // Cannot calculate true wind without valid AWA, AWS, COG, or STW
        return false;
    } else {
        calcTwdSA(awaVal, awsVal, &ctw, stwVal, &hdt, &twd, &tws, &twa);
        *twdVal = twd;
        *twsVal = tws;
        *twaVal = twa;

        return true;
    }
}

void HstryBuf::fillWndBufSimData(tBoatHstryData& hstryBufs)
// Fill most part of TWD and TWS history buffer with simulated data
{
    double value = 20.0;
    int16_t value2 = 0;
    for (int i = 0; i < 900; i++) {
        value += random(-20, 20);
        value = WindUtils::to360(value);
        value2 = static_cast<int16_t>(value * DEG_TO_RAD * 1000);
        hstryBufs.twdHstry->add(value2);
    }
}

/* double genTwdSimDat()
{
    simTwd += random(-20, 20);
    if (simTwd < 0.0)
        simTwd += 360.0;
    if (simTwd >= 360.0)
        simTwd -= 360.0;

    int16_t z = static_cast<int16_t>(DegToRad(simTwd) * 1000.0);
    pageData.boatHstry.twdHstry->add(z); // Fill the buffer with some test data

    simTws += random(-200, 150) / 10.0; // TWS value in knots
    simTws = constrain(simTws, 0.0f, 50.0f); // Ensure TWS is between 0 and 50 knots
    twsValue = simTws;
}*/
