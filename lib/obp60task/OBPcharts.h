// Function lib for display of boat data in various graphical chart formats
#pragma once
#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "movingAvg.h"

struct Pos {
    int x;
    int y;
};

struct ChartProps {
    double range;
    double step;
};

template <typename T>
class RingBuffer;
class GwLog;

class Chart {
public:
    /*    enum class ChrtDirection {
            HORIZONTALE,
            VERTICALE
        };

        enum class ChrtSize {
            FULL_SIZEE,
            HALF_SIZE_LEFTE,
            HALF_SIZE_RIGHTE,
            TWO_THIRD_TOPE
        }; */

    Chart(RingBuffer<uint16_t>& dataBuf, CommonData& common, bool useSimuData); // Chart object of data chart
    ~Chart();
    bool init(); // initialize chart object parameters
    bool isValid() { return initValid; }; // Checks if chart object has been fully initialized
    void showChrt(const char chrtDir, const int8_t chrtSz, const int8_t chrtIntv, bool prntName, bool showCurrValue, GwApi::BoatValue currValue); // Perform all actions to draw chart
    //    void showChrt(ChrtDirection chrtDir, ChrtSize chrtSz, const int8_t chrtIntv, bool prntName, bool showCurrValue, GwApi::BoatValue currValue); // Perform all actions to draw chart

protected:
    CommonData* commonData;
    GwLog* logger;

    enum ChrtDataFormat {
        WIND,
        ROTATION,
        SPEED,
        DEPTH,
        TEMPERATURE,
        PRESSURE,
        OTHER
    };

    static constexpr char HORIZONTAL = 'H';
    static constexpr char VERTICAL = 'V';
    static constexpr int8_t FULL_SIZE = 0;
    static constexpr int8_t HALF_SIZE_LEFT = 1;
    static constexpr int8_t HALF_SIZE_RIGHT = 2;
    static constexpr int8_t TWO_THIRD_TOP = 3;

    static constexpr int8_t MIN_FREE_VALUES = 60; // free 60 values when chart line reaches chart end
    static constexpr int8_t THRESHOLD_NO_DATA = 3; // max. seconds of invalid values in a row
    static constexpr int8_t VALAXIS_SLOTS = 5; // no. of value axis labels

    static constexpr bool NO_SIMUDATA = true; // switch off simulation feature of <formatValue> function

    RingBuffer<uint16_t>& dataBuf; // Buffer to display
    bool initValid = false; // indicates whether object holds valid initialization data or not
    double dfltRng; // Default range of chart, e.g. 30 = [0..30]
    uint16_t fgColor; // color code for any screen writing
    uint16_t bgColor; // color code for screen background
    bool useSimuData; // flag to indicate if simulation data is active
    bool smoothCharts; // flag to indicate if charts shall be printed with smoothed gradient
    String tempFormat; // user defined format for temperature
    double zeroValue; // "0" SI value for temperature

    int dWidth; // Display width
    int dHeight; // Display height
    int top = 44; // chart gap at top of display (25 lines for standard gap + 19 lines for axis labels)
    int bottom = 25; // chart gap at bottom of display to keep space for status line
    int hGap = 11; // gap between 2 horizontal charts; actual gap is 2x <gap>
    int vGap = 17; // gap between 2 vertical charts; actual gap is 2x <gap>
    int timAxis, valAxis; // size of time and value chart axis
    Pos cRoot; // start point of chart area
    double chrtRng; // Range of buffer values from min to max value
    double chrtMin; // Range low end value
    double chrtMax; // Range high end value
    double chrtMid; // Range mid value
    double rngStep; // Defines the step of adjustment (e.g. 10 m/s) for value axis range
    bool recalcRngMid = false; // Flag for re-calculation of mid value of chart for wind data types

    String dbName, dbFormat; // Name and format of data buffer
    ChrtDataFormat chrtDataFmt; // Data format of chart boat data type
    double dbMIN_VAL; // Lowest possible value of buffer of type <T>
    double dbMAX_VAL; // Highest possible value of buffer of type <T>; indicates invalid value in buffer
    size_t bufSize; // History buffer size: 1.920 values for 32 min. history chart
    int count; // current size of buffer
    int numBufVals; // number of wind values available for current interval selection
    int bufStart; // 1st data value in buffer to show
    int numAddedBufVals; // Number of values added to buffer since last display
    size_t currIdx; // Current index in history buffer
    size_t lastIdx; // Last index of history buffer
    size_t lastAddedIdx = 0; // Last index of history buffer when new data was added
    int numNoData; // Counter for multiple invalid data values in a row
    bool bufDataValid = false; // Flag to indicate if buffer data is valid
    int oldChrtIntv = 0; // remember recent user selection of data interval

    movingAvg<double> chrtAvg { 7 }; // Store average of the last 7 chart values if chart gradient shall be smoothed
    double chrtPrevVal; // Last data value in chart area
    int x, y; // x and y coordinates for drawing
    int prevX, prevY; // Last x and y coordinates for drawing

    // Default ranges for various boat data types
    // Parameters: <1st value> default range, <2nd value> step for range adjustment
    std::map<String, ChartProps> dfltChrtDta = {
        { "formatWind", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default wind range 60 degrees
        { "formatCourse", { 60.0 * DEG_TO_RAD, 10.0 * DEG_TO_RAD } }, // default course range 60 degrees
        { "formatKnots", { 2.572, 2.572 } }, // default speed range in m/s
        { "formatDepth", { 10.0, 5.0 } }, // default depth range in m
        { "kelvinToC", { 20.0, 5.0 } }, // default temp range in °C/K
        { "formatXdr:P:P", { 4000.0, 1000.0 } } // default pressure range in Pascal (hPa * 100); XDR <B> (bar) format is represented in gateway in the same way
    };

    bool setChartDimensions(const char direction, const int8_t size); // define dimensions and start points for chart
    //    bool setChartDimensions(const ChrtDirection direction, const ChrtSize size); // define dimensions and start points for chart
    void drawChrt(const char chrtDir, const int8_t chrtIntv, GwApi::BoatValue& currValue); // Draw chart line
    void getBufferStartNSize(const int8_t chrtIntv); // Identify buffer size and buffer start position for chart
    void calcChrtBorders(double& rngMin, double& rngMid, double& rngMax, double& rng); // Calculate chart points for value axis and return range between <min> and <max>
    void drawChartLines(const char direction, const int8_t chrtIntv, const double chrtScale); // Draw chart graph
    Pos setCurrentChartPoint(const int i, const char direction, const double chrtVal, const double chrtScale); // Set current chart point to draw
    void drawChrtTimeAxis(const char chrtDir, const int8_t chrtSz, const int8_t chrtIntv); // Draw time axis of chart, value and lines
    void drawChrtValAxis(const char chrtDir, const int8_t chrtSz, bool prntLabel); // Draw value axis of chart, value and lines
    void prntCurrValue(const char chrtDir, GwApi::BoatValue& currValue); // Add current boat data value to chart
    void prntNoValidData(const char chrtDir); // print message for no valid data available
    double getAngleRng(const double center, size_t amount); // Calculate range between chart center and edges
    void prntVerticChartThreeValueAxisLabel(const GFXfont* font); // print value axis label with only three values: top, mid, and bottom for vertical chart
    void prntHorizChartThreeValueAxisLabel(const GFXfont* font); // print value axis label with only three values: top, mid, and bottom for horizontal chart
    void prntHorizChartMultiValueAxisLabel(const GFXfont* font); // print value axis label with multiple axis lines for horizontal chart
    void drawBoldLine(const int16_t x1, const int16_t y1, const int16_t x2, const int16_t y2); // Draw chart line with thickness of 2px
    String convNformatLabel(const double& label); // Convert and format current axis label to user defined format; helper function for easier handling of OBP60Formatter
    String formatLabel(const double& label); // Format current axis label for printing w/o data format conversion (has been done earlier)
};
