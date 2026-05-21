// Function lib for display of boat data in various chart formats
#include "OBPcharts.h"
#include "OBPDataOperations.h"
#include "OBPRingBuffer.h"

// Default ranges for various boat data types: 1st value default range, 2nd value step for range adjustment
// should be multiple of 4 for full integer chart labels w/o decimals
std::map<String, ChartProps> Chart::dfltChrtDta = {
    { "formatWind", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default wind range 60 degrees
    { "formatCourse", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default course range 60 degrees
    { "formatKnots", { 2.57, 2.57 } }, // default speed range in m/s
    { "formatDepth", { 15.0, 5.0 } }, // default depth range in m
    { "kelvinToC", { 20.0, 5.0 } } // default temp range in °C/K
};

// --- Class Chart ---------------

// Chart - object holding the actual chart, incl. data buffer and format definition
// Parameters: <dataBuf> the history data buffer for the chart
//             <dfltRng> default range of chart, e.g. 30 = [0..30]
//             <common> common program data; required for logger and color data
//             <useSimuData> flag to indicate if simulation data is active
Chart::Chart(RingBuffer<uint16_t>& dataBuf, double dfltRng, CommonData& common, bool useSimuData)
    : dataBuf(dataBuf)
    , dfltRng(dfltRng)
    , commonData(&common)
    , useSimuData(useSimuData)
{
    logger = commonData->logger;
    fgColor = commonData->fgcolor;
    bgColor = commonData->bgcolor;

    // display dimensions (avoid calling width()/height() on incomplete LGFX type)
    #ifdef TFT_DISPLAY
        dWidth = 480;
        dHeight = 320;
    #else
        dWidth = getdisplay().width();
        dHeight = getdisplay().height();
    #endif

    dataBuf.getMetaData(dbName, dbFormat);
    dbMIN_VAL = dataBuf.getMinVal();
    dbMAX_VAL = dataBuf.getMaxVal();
    bufSize = dataBuf.getCapacity();

    smoothCharts = common.config->getBool(common.config->smoothCharts);
    if (smoothCharts) {
        chrtAvg.begin();
    }

    // Initialize chart data format; shorter version of standard format indicator
    if (dbFormat == "formatCourse" || dbFormat == "formatWind" || dbFormat == "formatRot") {
        chrtDataFmt = WIND; // Chart is showing data of course / wind <degree> format
    } else if (dbFormat == "formatRot") {
        chrtDataFmt = ROTATION; // Chart is showing data of rotational <degree> format
    } else if (dbFormat == "formatKnots") {
        chrtDataFmt = SPEED; // Chart is showing data of speed or windspeed format
    } else if (dbFormat == "formatDepth") {
        chrtDataFmt = DEPTH; // Chart ist showing data of <depth> format
    } else if (dbFormat == "kelvinToC") {
        chrtDataFmt = TEMPERATURE; // Chart ist showing data of <temp> format
    } else {
        chrtDataFmt = OTHER; // Chart is showing any other data format
    }

    // "0" value is the same for any data format but for user defined temperature format
    zeroValue = 0.0;
    if (chrtDataFmt == TEMPERATURE) {
        tempFormat = commonData->config->getString(commonData->config->tempFormat); // [K|°C|°F]
        if (tempFormat == "K") {
            zeroValue = 0.0;
        } else if (tempFormat == "C") {
            zeroValue = 273.15;
        } else if (tempFormat == "F") {
            zeroValue = 255.37;
        }
    }

    // Read default range and range step for this chart type
    if (dfltChrtDta.count(dbFormat)) {
        dfltRng = dfltChrtDta[dbFormat].range;
        rngStep = dfltChrtDta[dbFormat].step;
    } else {
        dfltRng = 16.0;
        rngStep = 4.0;
    }

    // Initialize chart range values
    chrtMin = zeroValue;
    chrtMax = chrtMin + dfltRng;
    chrtMid = (chrtMin + chrtMax) / 2;
    chrtRng = dfltRng;
    recalcRngMid = true; // initialize <chrtMid> and chart borders on first screen call

    LOG_DEBUG(GwLog::DEBUG, "Chart Init: dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cRoot {x,y}: %d, %d, dbname: %s, rngStep: %.4f, chrtDataFmt: %d",
        dWidth, dHeight, timAxis, valAxis, cRoot.x, cRoot.y, dbName, rngStep, chrtDataFmt);
};

Chart::~Chart()
{
}

// Perform all actions to draw chart
// Parameters: <chrtDir>:       chart timeline direction: 'H' = horizontal, 'V' = vertical
//             <chrtSz>:        chart size: [0] = full size, [1] = half size left/top, [2] half size right/bottom
//             <chrtIntv>:      chart timeline interval
//             <prntName>;      print data name on horizontal half chart [true|false]
//             <showCurrValue>: print current boat data value [true|false]
//             <currValue>:     current boat data value; used only for test on valid data
void Chart::showChrt(char chrtDir, int8_t chrtSz, const int8_t chrtIntv, bool prntName, bool showCurrValue, GwApi::BoatValue currValue)
{
    if (!setChartDimensions(chrtDir, chrtSz)) {
        return; // wrong chart dimension parameters
    }

    drawChrt(chrtDir, chrtIntv, currValue);
    drawChrtTimeAxis(chrtDir, chrtSz, chrtIntv);
    drawChrtValAxis(chrtDir, chrtSz, prntName);

    if (!bufDataValid) { // No valid data available
        prntNoValidData(chrtDir);
        return;
    }

    if (showCurrValue) { // show latest value from history buffer; this should be the most current one
        currValue.value = dataBuf.getLast();
        currValue.valid = currValue.value != dbMAX_VAL;
        prntCurrValue(chrtDir, currValue);
    }
}

// define dimensions and start points for chart
bool Chart::setChartDimensions(const char direction, const int8_t size)
{
    if ((direction != HORIZONTAL && direction != VERTICAL) || (size < 0 || size > 2)) {
        LOG_DEBUG(GwLog::ERROR, "obp60:setChartDimensions %s: wrong parameters", dataBuf.getName());
        return false;
    }

    if (direction == HORIZONTAL) {
        // horizontal chart timeline direction
        timAxis = dWidth - 1;
        switch (size) {
        case 0:
            valAxis = dHeight - top - bottom;
            cRoot = { 0, top - 1 };
            break;
        case 1:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cRoot = { 0, top - 1 };
            break;
        case 2:
            valAxis = (dHeight - top - bottom) / 2 - hGap;
            cRoot = { 0, top + (valAxis + hGap) + hGap - 1 };
            break;
        }

    } else if (direction == VERTICAL) {
        // vertical chart timeline direction
        timAxis = dHeight - top - bottom;
        switch (size) {
        case 0:
            valAxis = dWidth - 1;
            cRoot = { 0, top - 1 };
            break;
        case 1:
            valAxis = dWidth / 2 - vGap;
            cRoot = { 0, top - 1 };
            break;
        case 2:
            valAxis = dWidth / 2 - vGap;
            cRoot = { dWidth / 2 + vGap - 1, top - 1 };
            break;
        }
    }
    //LOG_DEBUG(GwLog::DEBUG, "obp60:setChartDimensions %s: direction: %c, size: %d, dWidth: %d, dHeight: %d, timAxis: %d, valAxis: %d, cRoot{%d, %d}, top: %d, bottom: %d, hGap: %d, vGap: %d",
    //     dataBuf.getName(), direction, size, dWidth, dHeight, timAxis, valAxis, cRoot.x, cRoot.y, top, bottom, hGap, vGap);
    return true;
}

// draw chart
void Chart::drawChrt(const char chrtDir, const int8_t chrtIntv, GwApi::BoatValue& currValue)
{
    double chrtScale; // Scale for data values in pixels per value

    getBufferStartNSize(chrtIntv);

    // LOG_DEBUG(GwLog::DEBUG, "Chart:drawChart: min: %.1f, mid: %.1f, max: %.1f, rng: %.1f", chrtMin, chrtMid, chrtMax, chrtRng);
    calcChrtBorders(chrtMin, chrtMid, chrtMax, chrtRng);
    chrtScale = double(valAxis) / chrtRng; // Chart scale: pixels per value step
    // LOG_DEBUG(GwLog::DEBUG, "Chart:drawChart: min: %.1f, mid: %.1f, max: %.1f, rng: %.1f", chrtMin, chrtMid, chrtMax, chrtRng);

    // Do we have valid buffer data?
    if (dataBuf.getMax() == dbMAX_VAL) { // only <MAX_VAL> values in buffer -> no valid wind data available
        bufDataValid = false;
        return;

    } else if (currValue.valid || useSimuData) { // latest boat data valid or simulation mode
        numNoData = 0; // reset data error counter
        bufDataValid = true;

    } else { // currently no valid data
        numNoData++;
        bufDataValid = true;

        if (numNoData > THRESHOLD_NO_DATA) { // If more than 4 invalid values in a row, flag for invalid data
            bufDataValid = false;
            return;
        }
    }

    drawChartLines(chrtDir, chrtIntv, chrtScale);
}

// Identify buffer size and buffer start position for chart
void Chart::getBufferStartNSize(const int8_t chrtIntv)
{
    count = dataBuf.getCurrentSize();
    currIdx = dataBuf.getLastIdx();
    numAddedBufVals = (currIdx - lastAddedIdx + bufSize) % bufSize; // Number of values added to buffer since last display

    if (chrtIntv != oldChrtIntv || count == 1) {
        // new data interval selected by user; this is only x * 230 values instead of 240 seconds (4 minutes) per interval step
        numBufVals = min(count, (timAxis - MIN_FREE_VALUES) * chrtIntv); // keep free or release MIN_FREE_VALUES on chart for plotting of new values
        bufStart = max(0, count - numBufVals);
        lastAddedIdx = currIdx;
        oldChrtIntv = chrtIntv;

    } else {
        numBufVals = numBufVals + numAddedBufVals;
        lastAddedIdx = currIdx;
        if (count == bufSize) {
            bufStart = max(0, bufStart - numAddedBufVals);
        }
    }
}

// check and adjust chart range and set range borders and range middle
void Chart::calcChrtBorders(double& rngMin, double& rngMid, double& rngMax, double& rng)
{
    if (chrtDataFmt == WIND || chrtDataFmt == ROTATION) {

        if (chrtDataFmt == ROTATION) {
            // if chart data is of type 'rotation', we want to have <rndMid> always to be '0'
            rngMid = 0;

        } else { // WIND: Chart data is of type 'course' or 'wind'

            // initialize <rngMid> if data buffer has just been started filling
            if ((count == 1 && rngMid == 0) || rngMid == dbMAX_VAL) {
                recalcRngMid = true;
            }

            if (recalcRngMid) {
                // Set rngMid

                rngMid = dataBuf.getMid(numBufVals);

                if (rngMid == dbMAX_VAL) {
                    rngMid = 0;
                } else {
                    rngMid = std::round(rngMid / rngStep) * rngStep; // Set new center value; round to next <rngStep> value

                    // Check if range between  'min' and 'max' is > 180° or crosses '0'
                    rngMin = dataBuf.getMin(numBufVals);
                    rngMax = dataBuf.getMax(numBufVals);
                    rng = (rngMax >= rngMin ? rngMax - rngMin : M_TWOPI - rngMin + rngMax);
                    rng = std::max(rng, dfltRng); // keep at least default chart range

                    if (rng > M_PI) { // If wind range > 180°, adjust wndCenter to smaller wind range end
                        rngMid = WindUtils::to2PI(rngMid + M_PI);
                    }
                }
                recalcRngMid = false; // Reset flag for <rngMid> determination

                // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange: rngMin: %.1f°, rngMid: %.1f°, rngMax: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMin * RAD_TO_DEG, rngMid * RAD_TO_DEG, rngMax * RAD_TO_DEG,
                //     rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);
            }
        }

        // check and adjust range between left, mid, and right chart limit
        double halfRng = rng / 2.0; // we calculate with range between <rngMid> and edges
        double tmpRng = getAngleRng(rngMid, numBufVals);
        tmpRng = (tmpRng == dbMAX_VAL ? 0 : std::ceil(tmpRng / rngStep) * rngStep);

        // LOG_DEBUG(GwLog::DEBUG, "calcChrtBorders: tmpRng: %.1f°, halfRng: %.1f°", tmpRng * RAD_TO_DEG, halfRng * RAD_TO_DEG);

        if (tmpRng > halfRng) { // expand chart range to new value
            halfRng = tmpRng;
        }

        else if (tmpRng + rngStep < halfRng) { // Contract chart range for higher resolution if possible
            halfRng = std::max(dfltRng / 2.0, tmpRng);
        }

        rngMin = WindUtils::to2PI(rngMid - halfRng);
        rngMax = (halfRng < M_PI ? rngMid + halfRng : rngMid + halfRng - (M_TWOPI / 360)); // if chart range is 360°, then make <rngMax> 1° smaller than <rngMin>
        rngMax = WindUtils::to2PI(rngMax);

        rng = halfRng * 2.0;

        // LOG_DEBUG(GwLog::DEBUG, "calcChrtBorders: rngMin: %.1f°, rngMid: %.1f°, rngMax: %.1f°, tmpRng: %.1f°, rng: %.1f°, rngStep: %.1f°", rngMin * RAD_TO_DEG, rngMid * RAD_TO_DEG, rngMax * RAD_TO_DEG,
        //     tmpRng * RAD_TO_DEG, rng * RAD_TO_DEG, rngStep * RAD_TO_DEG);

    } else { // chart data is of any other type

        double currMinVal = dataBuf.getMin(numBufVals);
        double currMaxVal = dataBuf.getMax(numBufVals);

        if (currMinVal == dbMAX_VAL || currMaxVal == dbMAX_VAL) {
            return; // no valid data
        }

        // check if current chart border have to be adjusted
        if (currMinVal < rngMin || (currMinVal > (rngMin + rngStep))) { // decrease rngMin if required or increase if lowest value is higher than old rngMin
            rngMin = std::floor(currMinVal / rngStep) * rngStep; // align low range to lowest buffer value and nearest range interval
        }
        if ((currMaxVal > rngMax) || (currMaxVal < (rngMax - rngStep))) { // increase rngMax if required or decrease if lowest value is lower than old rngMax
            rngMax = std::ceil(currMaxVal / rngStep) * rngStep;
        }

        // Chart range starts at least at '0' if minimum data value allows it
        if (rngMin > zeroValue && dbMIN_VAL <= zeroValue) {
            rngMin = zeroValue;
        }

        // ensure minimum chart range in user format
        if ((rngMax - rngMin) < dfltRng) {
            rngMax = rngMin + dfltRng;
        }

        rngMid = (rngMin + rngMax) / 2.0;
        rng = rngMax - rngMin;

        // LOG_DEBUG(GwLog::DEBUG, "calcChrtRange-end: currMinVal: %.1f, currMaxVal: %.1f, rngMin: %.1f, rngMid: %.1f, rngMax: %.1f, rng: %.1f, rngStep: %.1f, zeroValue: %.1f, dbMIN_VAL: %.1f",
        //     currMinVal, currMaxVal, rngMin, rngMid, rngMax, rng, rngStep, zeroValue, dbMIN_VAL);
    }
}

// Draw chart graph
void Chart::drawChartLines(const char direction, const int8_t chrtIntv, const double chrtScale)
{
    double chrtVal; // Current data value
    Pos point, prevPoint; // current and previous chart point

    if (smoothCharts) {
        // prime moving average filter to ensure the first plotted point is already averaged

        chrtAvg.reset();

        // Feed the filter the 10 values preceding bufStart
        for (int p = 10; p > 0; p--) {
            // Calculate index with wrapping: (start - offset + size) % size
            int primeIdx = (bufStart - (p * chrtIntv));
            double primeVal = dataBuf.get(primeIdx);
            
            if (primeVal != dbMAX_VAL) {
                chrtAvg.reading(primeVal);
            }
        }
    }

    for (int i = 0; i < (numBufVals / chrtIntv); i++) {

        chrtVal = dataBuf.get(bufStart + (i * chrtIntv)); // show the latest wind values in buffer; keep 1st value constant in a rolling buffer

        if (chrtVal == dbMAX_VAL) {
            chrtPrevVal = dbMAX_VAL;
        } else {

            if (smoothCharts) {
                // if chart lines shall be smoothed, apply moving average filter of the last 10 values
                chrtVal = chrtAvg.reading(chrtVal);
            }

            point = setCurrentChartPoint(i, direction, chrtVal, chrtScale);

            // if (i >= (numBufVals / chrtIntv) - 5) // log chart data of 1 line (adjust for test purposes)
            //    LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: i: %d, chrtVal: %.2f, chrtMin: %.2f, {x,y} {%d,%d}", i, chrtVal, chrtMin, x, y);

            if ((i == 0) || (chrtPrevVal == dbMAX_VAL)) {
                // just a dot for 1st chart point or after some invalid values
                prevPoint = point;

            } else if (chrtDataFmt == WIND || chrtDataFmt == ROTATION) {
                // cross borders check for degree values; shift values to [-PI..0..PI]; when crossing borders, range is 2x PI degrees

                double normCurrVal = WindUtils::to2PI(chrtVal - chrtMin);
                double normPrevVal = WindUtils::to2PI(chrtPrevVal - chrtMin);
                // Check if pixel positions are far apart (crossing chart boundary); happens when one value is near chrtMax and the other near chrtMin
                bool crossedBorders = std::abs(normCurrVal - normPrevVal) > (chrtRng / 2.0);

                if (crossedBorders) { // If current value crosses chart borders compared to previous value, split line
                    // LOG_DEBUG(GwLog::DEBUG, "PageWindPlot Chart: crossedBorders: %d, chrtVal: %.2f, chrtPrevVal: %.2f", crossedBorders, chrtVal, chrtPrevVal);
                    bool wrappingFromHighToLow = normCurrVal < normPrevVal; // Determine which edge we're crossing

                    if (direction == HORIZONTAL) {
                        int ySplit = wrappingFromHighToLow ? (cRoot.y + valAxis) : cRoot.y;
                        drawBoldLine(prevPoint.x, prevPoint.y, point.x, ySplit);
                        prevPoint.y = wrappingFromHighToLow ? cRoot.y : (cRoot.y + valAxis);

                    } else { // vertical chart
                        int xSplit = wrappingFromHighToLow ? (cRoot.x + valAxis) : cRoot.x;
                        drawBoldLine(prevPoint.x, prevPoint.y, xSplit, point.y);
                        prevPoint.x = wrappingFromHighToLow ? cRoot.x : (cRoot.x + valAxis);
                    }
                }
            }

            if (chrtDataFmt == DEPTH) {
                if (direction == HORIZONTAL) { // horizontal chart
                    drawBoldLine(point.x, point.y, point.x, cRoot.y + valAxis);
                } else { // vertical chart
                    drawBoldLine(point.x, point.y, cRoot.x + valAxis, point.y);
                }
            } else {
                drawBoldLine(prevPoint.x, prevPoint.y, point.x, point.y);
            }

            chrtPrevVal = chrtVal;
            prevPoint = point;
        }

        // Reaching chart area top end
        if (i >= timAxis - 1) {
            oldChrtIntv = 0; // force reset of buffer start and number of values to show in next display loop

            if (chrtDataFmt == WIND) { // degree of course or wind
                recalcRngMid = true;
                // LOG_DEBUG(GwLog::DEBUG, "PageWindPlot: chart end: timAxis: %d, i: %d, bufStart: %d, numBufVals: %d, recalcRngCntr: %d", timAxis, i, bufStart, numBufVals, recalcRngMid);
            }
            break;
        }

        taskYIELD(); // we run for 50-150ms; be polite to other tasks with same priority
    }
}

// Set current chart point to draw
Pos Chart::setCurrentChartPoint(const int i, const char direction, const double chrtVal, const double chrtScale)
{
    Pos currentPoint;

    if (direction == HORIZONTAL) {
        currentPoint.x = cRoot.x + i; // Position in chart area

        if (chrtDataFmt == WIND || chrtDataFmt == ROTATION) { // degree type value
            currentPoint.y = cRoot.y + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScale) + 0.5); // calculate chart point and round
        } else if (chrtDataFmt == SPEED or chrtDataFmt == TEMPERATURE) { // speed or temperature data format -> print low values at bottom
            currentPoint.y = cRoot.y + valAxis - static_cast<int>(((chrtVal - chrtMin) * chrtScale) + 0.5); // calculate chart point and round
        } else { // any other data format
            currentPoint.y = cRoot.y + static_cast<int>(((chrtVal - chrtMin) * chrtScale) + 0.5); // calculate chart point and round
        }

    } else { // vertical chart
        currentPoint.y = cRoot.y + timAxis - i; // Position in chart area

        if (chrtDataFmt == WIND || chrtDataFmt == ROTATION) { // degree type value
            currentPoint.x = cRoot.x + static_cast<int>((WindUtils::to2PI(chrtVal - chrtMin) * chrtScale) + 0.5); // calculate chart point and round
        } else {
            currentPoint.x = cRoot.x + static_cast<int>(((chrtVal - chrtMin) * chrtScale) + 0.5); // calculate chart point and round
        }
    }

    return currentPoint;
}

// chart time axis label + lines
void Chart::drawChrtTimeAxis(const char chrtDir, const int8_t chrtSz, const int8_t chrtIntv)
{
    int axSlots, intv, i, timeRng;
    char sTime[6];
    int tOffset;

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setTextColor(fgColor);

    intv = 60; // print axis label each 60 pixels = seconds
    axSlots = timAxis / intv; // number of axis labels; value will be truncated to full number

    if (chrtDir == HORIZONTAL) {
        getdisplay().fillRect(0, cRoot.y, dWidth, 2, fgColor);

        i = axSlots * chrtIntv * -1; // current minute label
        // LOG_DEBUG(GwLog::DEBUG, "Chart::drawChrtTimeAxis: intv: %d, axSlots: %d, timAxis: %d, chrtIntv: %d", intv, axSlots, timAxis, chrtIntv);
        for (int j = intv; j < timAxis - 1; j += intv) { // fill time axis with values but keep area free on right hand side for value label

            // draw text with appropriate offset
            tOffset = j == 0 ? 13 : -4;
            snprintf(sTime, sizeof(sTime), "%d", i);
            drawTextCenter(cRoot.x + j + tOffset, cRoot.y - 8, sTime);
            getdisplay().drawLine(cRoot.x + j, cRoot.y, cRoot.x + j, cRoot.y + 5, fgColor); // draw short vertical time mark

            i += chrtIntv;
        }

    } else { // vertical chart

        i = chrtIntv * -1; // current minute label
        for (float j = intv; j < timAxis - 1; j += intv) { // don't print time label at upper and lower end of time axis

            snprintf(sTime, sizeof(sTime), "%d", i);
            getdisplay().drawLine(cRoot.x, cRoot.y + j, cRoot.x + valAxis, cRoot.y + j, fgColor); // Grid line

            if (chrtSz == FULL_SIZE) { // full size chart
                getdisplay().fillRect(0, cRoot.y + j - 9, 32, 15, bgColor); // clear small area to remove potential chart lines
                getdisplay().setCursor((4 - strlen(sTime)) * 7, cRoot.y + j + 3); // time value; print left screen; value right-formated
                getdisplay().printf("%s", sTime); // time value
            } else if (chrtSz == HALF_SIZE_RIGHT) { // half size chart; right side
                drawTextCenter(dWidth / 2, cRoot.y + j, sTime); // time value; print mid screen
            }
            i -= chrtIntv;
        }
    }
}

// chart value axis labels + lines
void Chart::drawChrtValAxis(const char chrtDir, const int8_t chrtSz, bool prntName)
{
    const GFXfont* font;
    constexpr bool NO_LABEL = false;
    constexpr bool LABEL = true;

    getdisplay().setTextColor(fgColor);

    if (chrtDir == HORIZONTAL) {

        if (chrtSz == FULL_SIZE) {

            // print buffer data name on left hand side of time axis (max. size 5 characters)
            font = &Ubuntu_Bold12pt8b;
            getdisplay().setFont(font);
            getdisplay().fillRect(cRoot.x + timAxis - 57, cRoot.y + 2, 58, 20, bgColor); // clear small area to remove potential chart lines
            drawTextRalign(cRoot.x + timAxis, cRoot.y + 19, dbName.substring(0, 5));

            if (chrtDataFmt == WIND) {
                prntHorizChartThreeValueAxisLabel(font);
                return;
            }

            // for any other data formats print multiple axis value lines on full charts
            font = &Ubuntu_Bold10pt8b;
            prntHorizChartMultiValueAxisLabel(font);
            return;

        } else { // half size chart -> just print edge values + middle chart line

            font = &Ubuntu_Bold10pt8b;
            if (prntName) {
                // print buffer data name on right hand side of time axis (max. size 5 characters)
                getdisplay().setFont(font);
                getdisplay().fillRect(cRoot.x + timAxis - 57, cRoot.y + 2, 58, 20, bgColor); // clear small area to remove potential chart lines
                drawTextRalign(cRoot.x + timAxis, cRoot.y + 16, dbName.substring(0, 5));
            }

            prntHorizChartThreeValueAxisLabel(font);
            return;
        }

    } else { // vertical chart

        if (chrtSz == FULL_SIZE) {
            font = &Ubuntu_Bold12pt8b;
            getdisplay().setFont(font); // use larger font
            drawTextRalign(cRoot.x + (valAxis * 0.42), cRoot.y - 2, dbName.substring(0, 6)); // print buffer data name (max. size 5 characters)
        }

        font = &Ubuntu_Bold10pt8b;
        prntVerticChartThreeValueAxisLabel(font);
    }
}

// Print current data value
void Chart::prntCurrValue(const char direction, GwApi::BoatValue& currValue)
{
    const int xPosVal = (direction == HORIZONTAL) ? cRoot.x + (timAxis / 2) - 56 : cRoot.x + 32;
    const int yPosVal = (direction == HORIZONTAL) ? cRoot.y + valAxis - 7 : cRoot.y + timAxis - 7;

    FormattedData frmtDbData = formatValue(&currValue, *commonData, NO_SIMUDATA);
    String sdbValue = frmtDbData.svalue; // value as formatted string
    String dbUnit = frmtDbData.unit; // Unit of value; limit length to 3 characters

    getdisplay().fillRect(xPosVal - 1, yPosVal - 35, 128, 41, bgColor); // Clear area for TWS value
    getdisplay().drawRect(xPosVal, yPosVal - 34, 126, 40, fgColor); // Draw box for TWS value
    getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);
    getdisplay().setCursor(xPosVal + 1, yPosVal);
    getdisplay().print(sdbValue); // value

    getdisplay().setFont(&Ubuntu_Bold10pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal - 17);
    getdisplay().print(dbName.substring(0, 3)); // Name, limited to 3 characters

    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(xPosVal + 76, yPosVal + 0);
    getdisplay().print(dbUnit); // Unit
}

// print message for no valid data availabletemplate <typename T>
void Chart::prntNoValidData(const char direction)
{
    Pos p;

    getdisplay().setFont(&Ubuntu_Bold10pt8b);

    if (direction == HORIZONTAL) {
        p.x = cRoot.x + (timAxis / 2);
        p.y = cRoot.y + (valAxis / 2) - 10;
    } else {
        p.x = cRoot.x + (valAxis / 2);
        p.y = cRoot.y + (timAxis / 2) - 10;
    }

    getdisplay().fillRect(p.x - 37, p.y - 10, 78, 24, bgColor); // Clear area for message
    drawTextCenter(p.x, p.y, "No data");

    LOG_DEBUG(GwLog::LOG, "Page chart <%s>: No valid data available", dbName);
}

// Get maximum difference of last <amount> of dataBuf ringbuffer values to center chart; for angle data only
double Chart::getAngleRng(const double center, size_t amount)
{
    size_t count = dataBuf.getCurrentSize();

    if (dataBuf.isEmpty() || amount <= 0) {
        return dbMAX_VAL;
    }
    if (amount > count)
        amount = count;

    double value = 0;
    double range = 0;
    double maxRng = dbMIN_VAL;

    // Start from the newest value (last) and go backwards x times
    for (size_t i = 0; i < amount; i++) {
        value = dataBuf.get(count - 1 - i);

        if (value == dbMAX_VAL) {
            continue; // ignore invalid values
        }

        range = abs(fmod((value - center + (M_TWOPI + M_PI)), M_TWOPI) - M_PI);
        if (range > maxRng)
            maxRng = range;
    }

    if (maxRng > M_PI) {
        maxRng = M_PI;
    }

    return (maxRng != dbMIN_VAL ? maxRng : dbMAX_VAL); // Return range from <mid> to <max>
}

 // print value axis label with only three values: top, mid, and bottom for vertical chart
 void Chart::prntVerticChartThreeValueAxisLabel(const GFXfont* font)
{
    double cVal;
    char sVal[7];

    getdisplay().fillRect(cRoot.x, cRoot.y, valAxis, 2, fgColor); // top chart line
    getdisplay().setFont(font);

    cVal = chrtMin;
    cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
    snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
    getdisplay().setCursor(cRoot.x, cRoot.y - 2);
    getdisplay().printf("%s", sVal); // Range low end

    cVal = chrtMid;
    cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
    snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
    drawTextCenter(cRoot.x + (valAxis / 2), cRoot.y - 9, sVal); // Range mid end

    cVal = chrtMax;
    cVal = convertValue(cVal, dbName, dbFormat, *commonData); // value (converted)
    snprintf(sVal, sizeof(sVal), "%.0f", round(cVal));
    drawTextRalign(cRoot.x + valAxis - 2, cRoot.y - 2, sVal); // Range high end

    // draw vertical grid lines for each axis label
    for (int j = 0; j <= valAxis; j += (valAxis / 2)) {
        getdisplay().drawLine(cRoot.x + j, cRoot.y, cRoot.x + j, cRoot.y + timAxis, fgColor);
    }
}

// print value axis label with only three values: top, mid, and bottom for horizontal chart
void Chart::prntHorizChartThreeValueAxisLabel(const GFXfont* font)
{
    double axLabel;
    double chrtMin, chrtMid, chrtMax;
    int xOffset, yOffset; // offset for text position of x axis label for different font sizes
    char sVal[11];

    if (font == &Ubuntu_Bold10pt8b) {
        xOffset = 32;
        yOffset = 16;
    } else if (font == &Ubuntu_Bold12pt8b) {
        xOffset = 42;
        yOffset = 18;
    }
    getdisplay().setFont(font);

    // convert & round chart bottom+top label to next range step
    chrtMin = convertValue(this->chrtMin, dbName, dbFormat, *commonData);
    chrtMid = convertValue(this->chrtMid, dbName, dbFormat, *commonData);
    chrtMax = convertValue(this->chrtMax, dbName, dbFormat, *commonData);

    // print top axis label
    axLabel = (chrtDataFmt == SPEED || chrtDataFmt == TEMPERATURE) ? chrtMax : chrtMin;
    snprintf(sVal, sizeof(sVal), "%3.0f", axLabel);
    getdisplay().fillRect(cRoot.x, cRoot.y + 2, xOffset + 3, yOffset, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + yOffset, sVal); // range value

    // print mid axis label
    axLabel = chrtMid;
    formatLabel(axLabel).toCharArray(sVal, 11); // print mid label with 1 decimal for small numbers, if required
    getdisplay().fillRect(cRoot.x, cRoot.y + (valAxis / 2) - 8, xOffset + 3, 16, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + (valAxis / 2) + 6, sVal); // range value
    getdisplay().drawLine(cRoot.x + xOffset + 3, cRoot.y + (valAxis / 2), cRoot.x + timAxis, cRoot.y + (valAxis / 2), fgColor);

    // print bottom axis label
    axLabel = (chrtDataFmt == SPEED || chrtDataFmt == TEMPERATURE) ? chrtMin : chrtMax;
    snprintf(sVal, sizeof(sVal), "%3.0f", axLabel);
    getdisplay().fillRect(cRoot.x, cRoot.y + valAxis - 14, xOffset + 3, 15, bgColor); // Clear small area to remove potential chart lines
    drawTextRalign(cRoot.x + xOffset, cRoot.y + valAxis, sVal); // range value
    getdisplay().drawLine(cRoot.x + xOffset + 3, cRoot.y + valAxis, cRoot.x + timAxis, cRoot.y + valAxis, fgColor);
}

// print value axis label with multiple axis lines for horizontal chart
void Chart::prntHorizChartMultiValueAxisLabel(const GFXfont* font)
{
    double chrtMin, chrtMax, chrtRng;
//    int axSlots = 5; // no. of axis labels
    int xOffset; // offset for text position of x axis label for different font sizes
    char sVal[11];

    if (font == &Ubuntu_Bold10pt8b) {
        xOffset = 32;
    } else if (font == &Ubuntu_Bold12pt8b) {
        xOffset = 42;
    }
    getdisplay().setFont(font);

    chrtMin = convertValue(this->chrtMin, dbName, dbFormat, *commonData);
    chrtMax = convertValue(this->chrtMax, dbName, dbFormat, *commonData);
    chrtRng = chrtMax - chrtMin;

    double axIntv = chrtRng / VALAXIS_SLOTS; // axis label interval
    double axLabel = chrtMin + axIntv; // current axis label
    double chrtScale = double(valAxis) / chrtRng; // Chart scale: pixels per value step
    double valAxisStep = axIntv * chrtScale; // pixel per axis label interval

    // LOG_DEBUG(GwLog::DEBUG, "Chart::printHorizMultiValueAxisLabel: chrtRng: %.2f, th-chrtRng: %.2f, axSlots: %.2f, axIntv: %.2f, axLabel: %.2f, chrtMin: %.2f, chrtMid: %.2f, chrtMax: %.2f", chrtRng, this->chrtRng, VALAXIS_SLOTS, axIntv, axLabel, this->chrtMin, chrtMid, chrtMax);

    int loopStrt, loopEnd, loopStp;
    if (chrtDataFmt == SPEED || chrtDataFmt == TEMPERATURE || chrtDataFmt == OTHER) {
        loopStrt = valAxis - valAxisStep;
        loopEnd = valAxisStep / 2;
        loopStp = valAxisStep * -1;
    } else {
        // Low value at top
        loopStrt = valAxisStep;
        loopEnd = valAxis - (valAxisStep / 2);
        loopStp = valAxisStep;
    }

    for (int j = loopStrt; (loopStp > 0) ? (j < loopEnd) : (j > loopEnd); j += loopStp) {
        // sVal = formatLabel(axLabel);
        snprintf(sVal, sizeof(sVal), "%3.0f", axLabel);
        getdisplay().fillRect(cRoot.x, cRoot.y + j - 11, xOffset + 3, 21, bgColor); // Clear small area to remove potential chart lines
        drawTextRalign(cRoot.x + xOffset, cRoot.y + j + 7, sVal); // range value
        getdisplay().drawLine(cRoot.x + xOffset + 3, cRoot.y + j, cRoot.x + timAxis, cRoot.y + j, fgColor);

        axLabel += axIntv;
    }
}

// Draw chart line with thickness of 2px
void Chart::drawBoldLine(const int16_t x1, const int16_t y1, const int16_t x2, const int16_t y2)
{
    int16_t dx = std::abs(x2 - x1);
    int16_t dy = std::abs(y2 - y1);

    getdisplay().drawLine(x1, y1, x2, y2, fgColor);

    if (dx >= dy) { // line has horizontal tendency
        getdisplay().drawLine(x1, y1 - 1, x2, y2 - 1, fgColor);
    } else { // line has vertical tendency
        getdisplay().drawLine(x1 - 1, y1, x2 - 1, y2, fgColor);
    }
}

// Convert and format current axis label to user defined format; helper function for easier handling of OBP60Formatter
String Chart::convNformatLabel(const double& label)
{
    GwApi::BoatValue tmpBVal(dbName); // temporary boat value for string formatter
    String sVal;

    tmpBVal.setFormat(dbFormat);
    tmpBVal.valid = true;
    tmpBVal.value = label;
    sVal = formatValue(&tmpBVal, *commonData, NO_SIMUDATA).svalue; // Formatted value as string including unit conversion and switching decimal places
    if (sVal.length() > 0 && sVal[0] == '!') {
        sVal = sVal.substring(1); // cut leading "!" created at OBPFormatter; doesn't work for other fonts than 7SEG
    }

    return sVal;
}

// Format current axis label for printing w/o data format conversion (has been done earlier)
String Chart::formatLabel(const double& label)
{
    char sVal[11];

    if (dbFormat == "formatCourse" || dbFormat == "formatWind") {
        // Format 3 numbers with prefix zero
        snprintf(sVal, sizeof(sVal), "%03.0f", label);

/*    } else if (dbFormat == "formatRot") {
        if (label > -10 && label < 10) {
            snprintf(sVal, sizeof(sVal), "%3.2f", label);
        } else {
            snprintf(sVal, sizeof(sVal), "%3.0f", label);
        } */

    } else {
        if (label < 9.95) {
            snprintf(sVal, sizeof(sVal), "%3.1f", label);
        } else {
            snprintf(sVal, sizeof(sVal), "%3.0f", label);
        }
    }

    return String(sVal);
}
// --- Class Chart ---------------
