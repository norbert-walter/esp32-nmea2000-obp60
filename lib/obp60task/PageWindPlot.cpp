#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPRingBuffer.h"
#include "OBPDataOperations.h"
#include "BoatDataCalibration.h"
#include <vector>

static const double radToDeg = 180.0 / M_PI; // Conversion factor from radians to degrees

// Get maximum difference of last <amount> of TWD ringbuffer values to center chart; returns "0" if data is not valid
int getCntr(const RingBuffer<int16_t>& windDirHstry, size_t amount)
{
    const int MAX_VAL = windDirHstry.getMaxVal();
    size_t count = windDirHstry.getCurrentSize();

    if (windDirHstry.isEmpty() || amount <= 0) {
        return 0;
    }
    if (amount > count)
        amount = count;

    uint16_t midWndDir, minWndDir, maxWndDir = 0;
    int wndCenter = 0;

    midWndDir = windDirHstry.getMid(amount);
    if (midWndDir != MAX_VAL) {
        midWndDir = midWndDir / 1000.0 * radToDeg;
        wndCenter = int((midWndDir + (midWndDir >= 0 ? 5 : -5)) / 10) * 10; // Set new center value; round to nearest 10 degree value
        minWndDir = windDirHstry.getMin(amount) / 1000.0 * radToDeg;
        maxWndDir = windDirHstry.getMax(amount) / 1000.0 * radToDeg;
        if ((maxWndDir - minWndDir) > 180 && !(minWndDir > maxWndDir)) { // if wind range is > 180 and no 0° crossover, adjust wndCenter to smaller wind range end
            wndCenter = WindUtils::to360(wndCenter + 180);
        }
    }

    return wndCenter;
}

// Get maximum difference of last <amount> of TWD ringbuffer values to center chart
int getRng(const RingBuffer<int16_t>& windDirHstry, int center, size_t amount)
{
    int minVal = windDirHstry.getMinVal();
    const int MAX_VAL = windDirHstry.getMaxVal();
    size_t count = windDirHstry.getCurrentSize();

    if (windDirHstry.isEmpty() || amount <= 0) {
        return MAX_VAL;
    }
    if (amount > count)
        amount = count;

    int value = 0;
    int rng = 0;
    int maxRng = minVal;
    // Start from the newest value (last) and go backwards x times
    for (size_t i = 0; i < amount; i++) {
        value = windDirHstry.get(count - 1 - i);

        if (value == MAX_VAL) {
            continue;  // ignore invalid values
        }

        value = value / 1000.0 * radToDeg;
        rng = abs(((value - center + 540) % 360) - 180);
        if (rng > maxRng)
            maxRng = rng;
    }
    if (maxRng > 180) {
        maxRng = 180;
    }

    return (maxRng != minVal ? maxRng : MAX_VAL);
}

// ****************************************************************
class PageWindPlot : public Page {

    bool keylock = false; // Keylock
    char chrtMode = 'D'; // Chart mode: 'D' for TWD, 'S' for TWS, 'B' for both
    bool showTruW = true; // Show true wind or apparant wind in chart area
    bool oldShowTruW = false; // remember recent user selection of wind data type

    int dataIntv = 1; // Update interval for wind history chart:
                      // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart
    bool useSimuData;
    String flashLED;
    String backlightMode;

public:
    PageWindPlot(CommonData& common)
    {
        commonData = &common;
        common.logger->logDebug(GwLog::LOG, "Instantiate PageWindPlot");

        // Get config data
        useSimuData = common.config->getBool(common.config->useSimuData);
        // holdValues = common.config->getBool(common.config->holdvalues);
        flashLED = common.config->getString(common.config->flashLED);
        backlightMode = common.config->getString(common.config->backlight);

    }

    virtual void setupKeys()
    {
        Page::setupKeys();
        // commonData->keydata[0].label = "MODE";
#if defined BOARD_OBP60S3
        commonData->keydata[1].label = "SRC";
        commonData->keydata[4].label = "INTV";
#elif defined BOARD_OBP40S3
        commonData->keydata[1].label = "INTV";
#endif
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

#if defined BOARD_OBP60S3
        // Set data source TRUE | APP
        if (key == 2) {
            showTruW = !showTruW;
            return 0; // Commit the key
        }

        // Set interval for wind history chart update time (interval)
        if (key == 5) {
#elif defined BOARD_OBP40S3
        if (key == 2) {
#endif
            if (dataIntv == 1) {
                dataIntv = 2;
            } else if (dataIntv == 2) {
                dataIntv = 3;
            } else if (dataIntv == 3) {
                dataIntv = 4;
            } else if (dataIntv == 4) {
                dataIntv = 8;
            } else {
                dataIntv = 1;
            }
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData &pageData){
#ifdef BOARD_OBP40S3
        String wndSrc; // Wind source true/apparant wind - preselection for OBP40

        wndSrc = commonData->config->getString("page" + String(pageData.pageNumber) + "wndsrc");
        if (wndSrc =="True wind") {
            showTruW = true;
        } else {
            showTruW = false; // Wind source is apparant wind
        }
        commonData->logger->logDebug(GwLog::LOG,"New PageWindPlot: wind source=%s", wndSrc);
#endif
        oldShowTruW = !showTruW; // makes wind source being initialized at initial page call
    }

    int displayPage(PageData& pageData)
    {
        GwConfigHandler* config = commonData->config;
        GwLog* logger = commonData->logger;

        static RingBuffer<int16_t>* wdHstry; // Wind direction data buffer
        static RingBuffer<uint16_t>* wsHstry; // Wind speed data buffer
        static String wdName, wdFormat; // Wind direction name and format
        static String wsName, wsFormat; // Wind speed name and format
        static int16_t wdMAX_VAL; // Max. value of wd history buffer, indicating invalid values
        float wsValue; // Wind speed value in chart area
        String wsUnit; // Wind speed unit in chart area
        static GwApi::BoatValue* wsBVal = new GwApi::BoatValue("TWS"); // temp BoatValue for wind speed unit identification; required by OBP60Formater

        // current boat data values; TWD/AWD only for validation test
        const int numBoatData = 2;
        GwApi::BoatValue* bvalue;
        bool BDataValid[numBoatData];

        static bool isInitialized = false; // Flag to indicate that page is initialized
        static bool wndDataValid = false; // Flag to indicate if wind data is valid
        static int numNoData; // Counter for multiple invalid data values in a row

        static int width; // Screen width
        static int height; // Screen height
        static int xCenter; // Center of screen in x direction
        static const int yOffset = 48; // Offset for y coordinates of chart area
        static int cHeight; // height of chart area
        static int bufSize; // History buffer size: 1.920 values for 32 min. history chart
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
        static const int dfltRng = 60; // Default range for chart
        int midWndDir; // New value for wndCenter after chart start / shift

        int x, y; // x and y coordinates for drawing
        static int prevX, prevY; // Last x and y coordinates for drawing
        static float chrtScl; // Scale for wind values in pixels per degree
        int chrtVal; // Current wind value
        static int chrtPrevVal; // Last wind value in chart area for check if value crosses 180 degree line

        LOG_DEBUG(GwLog::LOG, "Display PageWindPlot");
        ulong timer = millis();

        if (!isInitialized) {
            width = getdisplay().width();
            height = getdisplay().height();
            xCenter = width / 2;
            cHeight = height - yOffset - 22;
            numNoData = 0;
            bufStart = 0;
            oldDataIntv = 0;
            wsValue = 0;
            numAddedBufVals, currIdx, lastIdx = 0;
            wndCenter = INT_MAX;
            midWndDir = 0;
            diffRng = dfltRng;
            chrtRng = dfltRng;

            isInitialized = true; // Set flag to indicate that page is now initialized
        }

        // read boat data values; TWD only for validation test, TWS for display of current value
        for (int i = 0; i < numBoatData; i++) {
            bvalue = pageData.values[i];
            BDataValid[i] = bvalue->valid;
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (showTruW != oldShowTruW) {
            if (showTruW) {
                wdHstry = pageData.boatHstry->hstryBufList.twdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.twsHstry;
            } else {
                wdHstry = pageData.boatHstry->hstryBufList.awdHstry;
                wsHstry = pageData.boatHstry->hstryBufList.awsHstry;
            }
            wdHstry->getMetaData(wdName, wdFormat);
            wsHstry->getMetaData(wsName, wsFormat);
            wdMAX_VAL = wdHstry->getMaxVal();
            bufSize = wdHstry->getCapacity();
            wsBVal->setFormat(wsHstry->getFormat());
            lastAddedIdx = wdHstry->getLastIdx();

            oldShowTruW = showTruW;
        }

        // Identify buffer size and buffer start position for chart
        count = wdHstry->getCurrentSize();
        currIdx = wdHstry->getLastIdx();
        numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display
        if (dataIntv != oldDataIntv || count == 1) {
            // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
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
        // LOG_DEBUG(GwLog::DEBUG,"PSRAM Size: %d kByte; free: %d Byte", ESP.getPsramSize()/1024, ESP.getFreePsram());
        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Dataset: count: %d, xWD: %.1f, xWS: %.2f, xWD_valid? %d, intvBufSize: %d, numWndVals: %d, bufStart: %d, numAddedBufVals: %d, lastIdx: %d, wind source: %s",
            count, wdHstry->getLast() / 1000.0 * radToDeg, wsHstry->getLast() / 1000.0 * 1.94384, BDataValid[0], intvBufSize, numWndVals, bufStart, numAddedBufVals, wdHstry->getLastIdx(),
            showTruW ? "True" : "App");

        // Set wndCenter from 1st real buffer value
        if (wndCenter == INT_MAX || (wndCenter == 0 && count == 1)) {
            wndCenter = getCntr(*wdHstry, numWndVals);
            LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Range Init: count: %d, xWD: %.1f, wndCenter: %d, diffRng: %d, chrtRng: %d, Min: %.0f, Max: %.0f", count, wdHstry->getLast() / 1000.0 * radToDeg,
                wndCenter, diffRng, chrtRng, wdHstry->getMin(numWndVals) / 1000.0 * radToDeg, wdHstry->getMax(numWndVals) / 1000.0 * radToDeg);
        } else {
            // check and adjust range between left, center, and right chart limit
            diffRng = getRng(*wdHstry, wndCenter, numWndVals);
            diffRng = (diffRng == wdMAX_VAL ? 0 : diffRng);
            if (diffRng > chrtRng) {
                chrtRng = int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10; // Round up to next 10 degree value
            } else if (diffRng + 10 < chrtRng) { // Reduce chart range for higher resolution if possible
                chrtRng = max(dfltRng, int((diffRng + (diffRng >= 0 ? 9 : -1)) / 10) * 10);
                LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Range adjust: wndCenter: %d, diffRng: %d, chrtRng: %d, Min: %.0f, Max: %.0f", wndCenter, diffRng, chrtRng,
                    wdHstry->getMin(numWndVals) / 1000.0 * radToDeg, wdHstry->getMax(numWndVals) / 1000.0 * radToDeg);
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
        getdisplay().print(wdName); // Wind data name
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

        if (wdHstry->getMax() == wdMAX_VAL) {
            // only <MAX_VAL> values in buffer -> no valid wind data available
            wndDataValid = false;
        } else if (!BDataValid[0] && !useSimuData) {
            // currently no valid xWD data available and no simulation mode
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
                chrtVal = static_cast<int>(wdHstry->get(bufStart + (i * dataIntv))); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer
                if (chrtVal == wdMAX_VAL) {
                    chrtPrevVal = wdMAX_VAL;
                } else {
                    chrtVal = static_cast<int>((chrtVal / 1000.0 * radToDeg) + 0.5); // Convert to degrees and round
                    x = ((chrtVal - wndLeft + 360) % 360) * chrtScl;
                    y = yOffset + cHeight - i; // Position in chart area

                    if (i >= (numWndVals / dataIntv) - 1) // log chart data of 1 line (adjust for test purposes)
                        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %d, bufStart: %d, count: %d, linesToShow: %d", i, chrtVal, bufStart, count, (numWndVals / dataIntv));

                    if ((i == 0) || (chrtPrevVal == wdMAX_VAL)) {
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

                    int minWndDir = wdHstry->getMin(numWndVals) / 1000.0 * radToDeg;
                    int maxWndDir = wdHstry->getMax(numWndVals) / 1000.0 * radToDeg;
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FreeTop: Minimum: %d, Maximum: %d, OldwndCenter: %d", minWndDir, maxWndDir, wndCenter);
                    // if (((minWndDir - wndCenter >= 0) && (minWndDir - wndCenter < 180)) || ((maxWndDir - wndCenter <= 0) && (maxWndDir - wndCenter >=180))) {
                    if ((wndRight > wndCenter && (minWndDir >= wndCenter && minWndDir <= wndRight)) || (wndRight <= wndCenter && (minWndDir >= wndCenter || minWndDir <= wndRight)) || (wndLeft < wndCenter && (maxWndDir <= wndCenter && maxWndDir >= wndLeft)) || (wndLeft >= wndCenter && (maxWndDir <= wndCenter || maxWndDir >= wndLeft))) {
                        // Check if all wind value are left or right of center value -> optimize chart center
                        wndCenter = getCntr(*wdHstry, numWndVals);
                    }
                    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot FreeTop: cHeight: %d, bufStart: %d, numWndVals: %d, wndCenter: %d", cHeight, bufStart, numWndVals, wndCenter);
                    break;
                }
            }

            // Print wind speed value
            int currentZone;
            static int lastZone = 0;
            static bool flipTws = false;
            int xPosTws;
            static const int yPosTws = yOffset + 40;

            xPosTws = flipTws ? 20 : width - 145;
            currentZone = (y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146) ? 1 : 0; // Define current zone for TWS value
            if (currentZone != lastZone) {
                // Only flip when x moves to a different zone
                if ((y >= yPosTws - 38) && (y <= yPosTws + 6) && (x >= xPosTws - 4) && (x <= xPosTws + 146)) {
                    flipTws = !flipTws;
                    xPosTws = flipTws ? 20 : width - 145;
                }
            }
            lastZone = currentZone;

            wsValue = wsHstry->getLast();
            wsBVal->value = wsValue / 1000.0; // temp variable to retreive data unit from OBP60Formater
            wsBVal->valid = (static_cast<uint16_t>(wsValue) != wsHstry->getMinVal());
            String swsValue = formatValue(wsBVal, *commonData).svalue; // value (string)
            wsUnit = formatValue(wsBVal, *commonData).unit; // Unit of value
            getdisplay().fillRect(xPosTws - 4, yPosTws - 38, 142, 44, commonData->bgcolor); // Clear area for TWS value
            getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
            getdisplay().setCursor(xPosTws, yPosTws);
            getdisplay().print(swsValue); // Value
/*            if (!wsBVal->valid) {
                getdisplay().print("--.-");
            } else {
                wsValue = wsValue / 1000.0 * 1.94384; // Wind speed value in knots
                if (wsValue < 10.0) {
                    getdisplay().printf("!%3.1f", wsValue); // Value, round to 1 decimal
                } else {
                    getdisplay().printf("%4.1f", wsValue); // Value, round to 1 decimal
                }
            } */
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(xPosTws + 82, yPosTws - 14);
            getdisplay().print(wsName); // Name
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(xPosTws + 82, yPosTws + 1);
            getdisplay().print(wsUnit); // Unit

        } else {
            // No valid data available
            LOG_DEBUG(GwLog::LOG, "PageWindPlot: No valid data available");
            getdisplay().setFont(&Ubuntu_Bold10pt8b);
            getdisplay().fillRect(xCenter - 33, height / 2 - 20, 66, 24, commonData->bgcolor); // Clear area for message
            drawTextCenter(xCenter, height / 2 - 10, "No data");
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

        LOG_DEBUG(GwLog::DEBUG, "PageWindPlot time: %ld", millis() - timer);
        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageWindPlot(common);
}

/* with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need */
PageDescription registerPageWindPlot(
    "WindPlot", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    { "TWD", "AWD" }, // Bus values we need in the page
    true // Show display header on/off
);

#endif