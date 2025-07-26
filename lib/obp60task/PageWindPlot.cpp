#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "BoatDataCalibration.h"
#include "OBP60Extensions.h"
#include "OBPRingBuffer.h"
#include "Pagedata.h"
#include <vector>

static const double radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

// Get maximum difference of last <amount> of TWD ringbuffer values to center chart
int getRng(const RingBuffer<int16_t>& windDirHstry, int center, size_t amount)
{
    int minVal = windDirHstry.getMinVal();
    size_t count = windDirHstry.getCurrentSize();
    //    size_t capacity = windDirHstry.getCapacity();
    //    size_t last = windDirHstry.getLastIdx();

    if (windDirHstry.isEmpty() || amount <= 0) {
        return minVal;
    }
    if (amount > count)
        amount = count;

    int value = 0;
    int rng = 0;
    int maxRng = minVal;
    // Start from the newest value (last) and go backwards x times
    for (size_t i = 0; i < amount; i++) {
        //        value = windDirHstry.get(((last - i) % capacity + capacity) % capacity);
        value = windDirHstry.get(count - 1 - i);

        if (value == minVal) {
            continue;
        }

        value = value / 1000.0 * radToDeg;
        rng = abs(((value - center + 540) % 360) - 180);
        if (rng > maxRng)
            maxRng = rng;
    }
    if (maxRng > 180) {
        maxRng = 180;
    }

    return maxRng;
}

// ****************************************************************
class PageWindPlot : public Page {

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4) seconds for approx. 4, 8, 12, 16 min. history chart
    bool showTWS = true; // Show TWS value in chart area

public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageWindPlot");
    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        //        commonData->keydata[0].label = "MODE";
        commonData->keydata[1].label = "INTV";
        commonData->keydata[4].label = "TWS";
    }

    // Key functions
    virtual int handleKey(int key)
    {
        // Set chart mode TWD | TWS -> to be implemented
        if (key == 1) {
            if (chrtMode == 'D') {
                chrtMode = 'S';
            } else if (chrtMode == 'S') {
                chrtMode = 'B';
            } else {
                chrtMode = 'D';
            }
            return 0; // Commit the key
        }

        // Set interval for wind history chart update time
        if (key == 2) {
            if (dataIntv == 1) {
                dataIntv = 2;
            } else if (dataIntv == 2) {
                dataIntv = 3;
            } else if (dataIntv == 3) {
                dataIntv = 4;
            } else {
                dataIntv = 1;
            }
            return 0; // Commit the key
        }

        // Switch TWS on/off
        if (key == 5) {
            showTWS = !showTWS;
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    int displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        float twsValue; // TWS value in chart area
        static String twdName, twdUnit; // TWD name and unit
        static int updFreq; // Update frequency for TWD
        static int16_t twdLowest, twdHighest; // TWD range
        // static int16_t twdBufMinVal; // lowest possible twd buffer value; used for non-set data

        // current boat data values; TWD only for validation test, TWS for display of current value
        const int numBoatData = 2;
        GwApi::BoatValue* bvalue;
        String BDataName[numBoatData];
        double BDataValue[numBoatData];
        bool BDataValid[numBoatData];
        String BDataText[numBoatData];
        String BDataUnit[numBoatData];
        String BDataFormat[numBoatData];

        static bool isInitialized = false; // Flag to indicate that page is initialized
        static bool wndDataValid = false; // Flag to indicate if wind data is valid
        static int numNoData; // Counter for multiple invalid data values in a row
        static bool simulation = false;
        static bool holdValues = false;

        static int width; // Screen width
        static int height; // Screen height
        static int xCenter; // Center of screen in x direction
        static const int yOffset = 48; // Offset for y coordinates of chart area
        static int cHeight; // height of chart area
        static int bufSize; // History buffer size: 960 values for appox. 16 min. history chart
        static int intvBufSize; // Buffer size used for currently selected time interval
        int count; // current size of buffer
        static int numWndVals; // number of wind values available for current interval selection
        static int bufStart; // 1st data value in buffer to show
        int numAddedBufVals; // Number of values added to buffer since last display
        size_t currIdx; // Current index in TWD history buffer
        static size_t lastIdx; // Last index of TWD history buffer
        static size_t lastAddedIdx = 0; // Last index of TWD history buffer when new data was added
        static int oldDataIntv; // remember recent user selection of data interval

        static int wndCenter; // chart wind center value position
        static int wndLeft; // chart wind left value position
        static int wndRight; // chart wind right value position
        static int chrtRng; // Range of wind values from mid wind value to min/max wind value in degrees
        int diffRng; // Difference between mid and current wind value
        static const int dfltRng = 40; // Default range for chart
        int midWndDir; // New value for wndCenter after chart start / shift
        static int simTwd; // Simulation value for TWD
        static float simTws; // Simulation value for TWS

        int x, y; // x and y coordinates for drawing
        static int prevX, prevY; // Last x and y coordinates for drawing
        static float chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        static int chrtPrevVal; // Last wind value in chart area for check if value crosses 180 degree line

        LOG_DEBUG(GwLog::LOG, "Display page WindPlot");

        // Get config data
        simulation = config->getBool(config->useSimuData);
        holdValues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String backlightMode = config->getString(config->backlight);

        if (!isInitialized) {
            width = getdisplay().width();
            height = getdisplay().height();
            xCenter = width / 2;
            cHeight = height - yOffset - 22;
            bufSize = pageData.boatHstry.twdHstry->getCapacity();
            numNoData = 0;
            simTwd = pageData.boatHstry.twdHstry->getLast() / 1000.0 * radToDeg;
            simTws = 0;
            twsValue = 0;
            bufStart = 0;
            oldDataIntv = 0;
            numAddedBufVals, currIdx, lastIdx = 0;
            lastAddedIdx = pageData.boatHstry.twdHstry->getLastIdx();
            pageData.boatHstry.twdHstry->getMetaData(twdName, twdUnit, updFreq, twdLowest, twdHighest);
            wndCenter = INT_MIN;
            midWndDir = 0;
            diffRng = dfltRng;
            chrtRng = dfltRng;

            isInitialized = true; // Set flag to indicate that page is now initialized
        }

        // read boat data values; TWD only for validation test, TWS for display of current value
        for (int i = 0; i < numBoatData; i++) {
            bvalue = pageData.values[i];
            // BDataName[i] = xdrDelete(bvalue->getName());
            BDataName[i] = BDataName[i].substring(0, 6); // String length limit for value name
            calibrationData.calibrateInstance(bvalue, logger); // Check if boat data value is to be calibrated
            BDataValue[i] = bvalue->value; // Value as double in SI unit
            BDataValid[i] = bvalue->valid;
            BDataText[i] = formatValue(bvalue, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            BDataUnit[i] = formatValue(bvalue, *commonData).unit;
            BDataFormat[i] = bvalue->getFormat(); // Unit of value
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        // Identify buffer size and buffer start position for chart
        count = pageData.boatHstry.twdHstry->getCurrentSize();
        currIdx = pageData.boatHstry.twdHstry->getLastIdx();
        numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display
        if (dataIntv != oldDataIntv || count == 1) {
            // new data interval selected by user
            intvBufSize = cHeight * dataIntv;
            numWndVals = min(count, (cHeight - 60) * dataIntv);
            bufStart = max(0, count - numWndVals);
            lastAddedIdx = currIdx;
            oldDataIntv = dataIntv;
        } else {
            numWndVals = numWndVals + numAddedBufVals;
            lastAddedIdx = currIdx;
            if (count == bufSize) {
                bufStart = max(0, bufStart - numAddedBufVals);
            }
        }
        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Dataset: count: %d, TWD: %.0f, TWS: %.1f, TWD_valid? %d, intvBufSize: %d, numWndVals: %d, bufStart: %d, numAddedBufVals: %d, lastIdx: %d, old: %d, act: %d",
            count, pageData.boatHstry.twdHstry->getLast() / 1000.0 * radToDeg, pageData.boatHstry.twsHstry->getLast() / 10.0 * 1.94384, BDataValid[0],
            intvBufSize, numWndVals, bufStart, numAddedBufVals, pageData.boatHstry.twdHstry->getLastIdx(), oldDataIntv, dataIntv);

        // Set wndCenter from 1st real buffer value
        if (wndCenter == INT_MIN || (wndCenter == 0 && count == 1)) {
            midWndDir = pageData.boatHstry.twdHstry->getMid(numWndVals);
            if (midWndDir != INT16_MIN) {
                midWndDir = midWndDir / 1000.0 * radToDeg;
                wndCenter = int((midWndDir + (midWndDir >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
            } else {
                wndCenter = 0;
            }
            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Range Init: count: %d, TWD: %.0f, wndCenter: %d, diffRng: %d, chrtRng: %d", count, pageData.boatHstry.twdHstry->getLast() / 1000.0 * radToDeg,
                wndCenter, diffRng, chrtRng);
        } else {
            // check and adjust range between left, center, and right chart limit
            diffRng = getRng(*pageData.boatHstry.twdHstry, wndCenter, numWndVals);
            diffRng = (diffRng == INT16_MIN ? 0 : diffRng);
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
            } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
                chrtRng = max(dfltRng, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
            }
        }
        chrtScl = float(width) / float(chrtRng) / 2.0; // Chart scale: pixels per degree
        wndLeft = wndCenter - chrtRng;
        if (wndLeft < 0)
            wndLeft += 360;
        wndRight = (chrtRng < 180 ? wndCenter + chrtRng : wndCenter + chrtRng - 1);
        if (wndRight >= 360)
            wndRight -= 360;

        // Draw page
        //***********************************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        // chart lines
        getdisplay().fillRect(0, yOffset, width, 2, commonData->fgcolor);
        getdisplay().fillRect(xCenter, yOffset, 1, cHeight, commonData->fgcolor);

        // chart labels
        char sWndLbl[4]; // char buffer for Wind angle label
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(xCenter - 88, yOffset - 3);
        getdisplay().print("TWD"); // Wind data name
        snprintf(sWndLbl, 4, "%03d", (wndCenter < 0) ? (wndCenter + 360) : wndCenter);
        drawTextCenter(xCenter, yOffset - 11, sWndLbl);
        getdisplay().drawCircle(xCenter + 25, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(xCenter + 25, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(1, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndLeft < 0) ? (wndLeft + 360) : wndLeft);
        getdisplay().print(sWndLbl); // Wind left value
        getdisplay().drawCircle(46, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(46, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol
        getdisplay().setCursor(width - 50, yOffset - 3);
        snprintf(sWndLbl, 4, "%03d", (wndRight < 0) ? (wndRight + 360) : wndRight);
        getdisplay().print(sWndLbl); // Wind right value
        getdisplay().drawCircle(width - 5, yOffset - 17, 2, commonData->fgcolor); // <degree> symbol
        getdisplay().drawCircle(width - 5, yOffset - 17, 3, commonData->fgcolor); // <degree> symbol

        if (pageData.boatHstry.twdHstry->getMax() == pageData.boatHstry.twdHstry->getMinVal()) {
            // only <INT16_MIN> values in buffer -> no valid wind data available
            wndDataValid = false;
        } else if (!BDataValid[0]) {
            // currently no valid TWD data available
            numNoData++;
            wndDataValid = true;
            if (numNoData > 3) {
                // If more than 4 invalid values in a row, send message
                wndDataValid = false;
            }
        } else {
            numNoData = 0; // reset data error counter
            wndDataValid = true; // At least some wind data available
        }
        // Draw wind values in chart
        //***********************************************************************
        if (wndDataValid) {
            for (int i = 0; i < (numWndVals / dataIntv); i++) {
                chrtVal = static_cast<int>(pageData.boatHstry.twdHstry->get(bufStart + (i * dataIntv))); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
                if (chrtVal == INT16_MIN) {
                    chrtPrevVal = INT16_MIN;
                    /* if (i == linesToShow - 1) {
                        numNoData++;
                        // If more than 4 invalid values in a row, reset chart
                    } else {
                        numNoData = 0; // Reset invalid value counter
                    }
                    if (numNoData > 4) {
                        // If more than 4 invalid values in a row, send message
                        getdisplay().setFont(&Ubuntu_Bold10pt8b);
                        getdisplay().fillRect(xCenter - 66, height / 2 - 20, 146, 24, commonData->bgcolor); // Clear area for TWS value
                        drawTextCenter(xCenter, height / 2 - 10, "No sensor data");
                    } */
                } else {
                    chrtVal = static_cast<int>((chrtVal / 1000.0 * radToDeg) + 0.5); // Convert to degrees and round
                    x = ((chrtVal - wndLeft + 360) % 360) * chrtScl;
                    y = yOffset + cHeight - i; // Position in chart area

                    if (i >= (numWndVals / dataIntv) - 10)
                        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %d, bufStart: %d, count: %d, linesToShow: %d", i, chrtVal, bufStart, count, (numWndVals / dataIntv));

                    if ((i == 0) || (chrtPrevVal == INT16_MIN)) {
                        // just a dot for 1st chart point or after some invalid values
                        prevX = x;
                        prevY = y;
                    } else {
                        // cross borders check; shift values to [-180..0..180]; when crossing borders, range is 2x 180 degrees
                        int wndLeftDlt = -180 - ((wndLeft >= 180) ? (wndLeft - 360) : wndLeft);
                        int chrtVal180 = ((chrtVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                        int chrtPrevVal180 = ((chrtPrevVal + wndLeftDlt + 180) % 360 + 360) % 360 - 180;
                        if (((chrtPrevVal180 >= -180) && (chrtPrevVal180 < -90) && (chrtVal180 > 90)) || ((chrtPrevVal180 <= 179) && (chrtPrevVal180 > 90) && chrtVal180 <= -90)) {
                            // If current value crosses chart borders compared to previous value, split line
                            int xSplit = (((chrtPrevVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                            getdisplay().drawLine(prevX, prevY, xSplit, y, commonData->fgcolor);
                            getdisplay().drawLine(prevX, prevY - 1, ((xSplit != prevX) ? xSplit : xSplit - 1), ((xSplit != prevX) ? y - 1 : y), commonData->fgcolor);
                            prevX = (((chrtVal180 > 0 ? wndRight : wndLeft) - wndLeft + 360) % 360) * chrtScl;
                        }
                    }

                    // Draw line with 2 pixels width + make sure vertical line are drawn correctly
                    getdisplay().drawLine(prevX, prevY, x, y, commonData->fgcolor);
                    getdisplay().drawLine(prevX, prevY - 1, ((x != prevX) ? x : x - 1), ((x != prevX) ? y - 1 : y), commonData->fgcolor);
                    chrtPrevVal = chrtVal;
                    prevX = x;
                    prevY = y;
                }
                // Reaching chart area top end
                if (i >= (cHeight - 1)) {
                    oldDataIntv = 0; // force reset of buffer start and number of values to show in next display loop

                    int minWndDir = pageData.boatHstry.twdHstry->getMin(numWndVals) / 1000.0 * radToDeg;
                    int maxWndDir = pageData.boatHstry.twdHstry->getMax(numWndVals) / 1000.0 * radToDeg;
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FreeTop: Minimum: %d, Maximum: %d, OldwndCenter: %d", minWndDir, maxWndDir, wndCenter);
                    if ((minWndDir + 540 >= wndCenter + 540) || (maxWndDir + 540 <= wndCenter + 540)) {
                        // Check if all wind value are left or right of center value -> optimize chart range
                        midWndDir = pageData.boatHstry.twdHstry->getMid(numWndVals) / 1000.0 * radToDeg;
                        if (midWndDir != INT16_MIN) {
                            wndCenter = int((midWndDir + (midWndDir >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
                        }
                    }
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FreeTop: cHeight: %d, bufStart: %d, numWndVals: %d, wndCenter: %d", cHeight, bufStart, numWndVals, wndCenter);
                    break;
                }
            }

        } else {
            // No valid data available
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
            getdisplay().setFont(&Ubuntu_Bold10pt8b);
            getdisplay().fillRect(xCenter - 33, height / 2 - 20, 66, 24, commonData->bgcolor); // Clear area for message
            drawTextCenter(xCenter, height / 2 - 10, "No data");
        }

        // Print TWS value
        if (showTWS) {
            int currentZone;
            static int lastZone = 0;
            static bool flipTws = false;
            int xPosTws;
            static const int yPosTws = yOffset + 40;

            twsValue = pageData.boatHstry.twsHstry->getLast() / 10.0 * 1.94384; // TWS value in knots

            xPosTws = flipTws ? 20 : width - 138;
            currentZone = (y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146) ? 1 : 0; // Define current zone for TWS value
            if (currentZone != lastZone) {
                // Only flip when x moves to a different zone
                if ((y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146)) {
                    flipTws = !flipTws;
                    xPosTws = flipTws ? 20 : width - 145;
                }
            }
            lastZone = currentZone;

            getdisplay().fillRect(xPosTws - 4, yPosTws - 38, 142, 44, commonData->bgcolor); // Clear area for TWS value
            getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
            getdisplay().setCursor(xPosTws, yPosTws);
            if (!BDataValid[1]) {
                getdisplay().print("--.-");
            } else {
                double dbl = BDataValue[1] * 3.6 / 1.852;
                if (dbl < 10.0) {
                    getdisplay().printf("!%3.1f", dbl); // Value, round to 1 decimal
                } else {
                    getdisplay().printf("%4.1f", dbl); // Value, round to 1 decimal
                }
            }
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(xPosTws + 82, yPosTws - 14);
//            getdisplay().print("TWS"); // Name
            getdisplay().print(BDataName[1]); // Name
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
//            getdisplay().setCursor(xPosTws + 78, yPosTws + 1);
            getdisplay().setCursor(xPosTws + 82, yPosTws + 1);
//            getdisplay().printf(" kn"); // Unit
            getdisplay().print(BDataUnit[1]); // Unit
        }

        // chart Y axis labels; print at last to overwrite potential chart lines in label area
        int yPos;
        int chrtLbl;
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        for (int i = 1; i <= 3; i++) {
            yPos = yOffset + (i * 60);
            getdisplay().fillRect(0, yPos, width, 1, commonData->fgcolor);
            getdisplay().fillRect(0, yPos - 8, 24, 16, commonData->bgcolor); // Clear small area to remove potential chart lines
            getdisplay().setCursor(1, yPos + 4);
            if (count >= intvBufSize) {
                // Calculate minute value for label
                chrtLbl = ((i - 1 + (prevY < yOffset + 30)) * dataIntv) * -1; // change label if last data point is more than 30 lines (= seconds) from chart line
            } else {
                int j = 3 - i;
                chrtLbl = (int((((numWndVals / dataIntv) - 50) * dataIntv / 60) + 1) - (j * dataIntv)) * -1; // 50 lines left below last chart line
            }
            getdisplay().printf("%3d", chrtLbl); // Wind value label
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageWindPlot(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageWindPlot(
    "WindPlot", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    { "TWD", "TWS" }, // Bus values we need in the page
//    {}, // Bus values we need in the page
    true // Show display header on/off
);

#endif