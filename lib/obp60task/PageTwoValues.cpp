#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPcharts.h"

class PageTwoValues : public Page {
private:
    GwLog* logger;

    enum PageMode {
        VALUES,
        VAL1_CHART,
        VAL2_CHART,
        CHARTS
    };
    enum DisplayMode {
        FULL,
        HALF
    };

    static constexpr char HORIZONTAL = 'H';
    static constexpr char VERTICAL = 'V';
    static constexpr int8_t FULL_SIZE = 0;
    static constexpr int8_t HALF_SIZE_TOP = 1;
    static constexpr int8_t HALF_SIZE_BOTTOM = 2;

    static constexpr bool PRNT_NAME = true;
    static constexpr bool NO_PRNT_NAME = false;
    static constexpr bool PRNT_VALUE = true;
    static constexpr bool NO_PRNT_VALUE = false;

    static constexpr int YOFFSET = 130; // y offset for display of 2nd boat value

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    PageMode pageMode = VALUES; // Page display mode
    int8_t dataIntv = 1; // Update interval for wind history chart:
                         // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart

    // String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    String backlightMode;
    String tempFormat;

    // Data buffer pointer (owned by HstryBuffers)
    static constexpr int NUMVALUES = 2; // two data values in this page
    RingBuffer<uint16_t>* dataHstryBuf[NUMVALUES] = { nullptr };
    std::unique_ptr<Chart> dataChart[NUMVALUES]; // Chart object

    // Old values for hold function
    String sValueOld[NUMVALUES] = { "", "" };
    String unitOld[NUMVALUES] = { "", "" };

    // display data values in display <mode> [FULL|HALF]
    void showData(const std::vector<GwApi::BoatValue*>& bValue, DisplayMode mode)
    {
        getdisplay().setTextColor(commonData->fgcolor);

        int numValues = bValue.size(); // do we have to handle 1 or 2 values?

        for (int i = 0; i < numValues; i++) {
            int yOffset = YOFFSET * i;
            String name = xdrDelete(bValue[i]->getName()); // Value name
            name = name.substring(0, 6); // String length limit for value name
            double value = bValue[i]->value; // Value as double in SI unit
            bool valid = bValue[i]->valid; // Valid information
            String sValue = formatValue(bValue[i], *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            String unit = formatValue(bValue[i], *commonData).unit; // Unit of value

            // Show name
            getdisplay().setFont(&Ubuntu_Bold20pt8b);
            getdisplay().setCursor(20, 75 + yOffset);
            getdisplay().print(name); // name

            // Show unit
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(20, 125 + yOffset);

            if (holdValues) {
                getdisplay().print(unitOld[i]); // name
            } else {
                getdisplay().print(unit); // name
            }

            // Switch font depending on value format and adjust position
            if (bValue[i]->getFormat() == "formatLatitude" || bValue[i]->getFormat() == "formatLongitude") {
                getdisplay().setFont(&Ubuntu_Bold20pt8b);
                getdisplay().setCursor(50, 125 + yOffset);
            } else if (bValue[i]->getFormat() == "formatTime" || bValue[i]->getFormat() == "formatDate") {
                getdisplay().setFont(&Ubuntu_Bold20pt8b);
                getdisplay().setCursor(170, 105 + yOffset);
            } else { // Default font for other formats
                getdisplay().setFont(&DSEG7Classic_BoldItalic42pt7b);
                getdisplay().setCursor(180, 125 + yOffset);
            }

            // Show bus data
            if (!holdValues || useSimuData) {
                getdisplay().print(sValue); // Real value as formated string
            } else {
                getdisplay().print(sValueOld[i]); // Old value as formated string
            }

            if (valid == true) {
                sValueOld[i] = sValue; // Save the old value
                unitOld[i] = unit; // Save the old unit
            }
        }

        if (numValues == 2 && mode == FULL) { // print line only, if we want to show 2 data values
            getdisplay().fillRect(0, 145, width, 3, commonData->fgcolor);             // Horizontal line 3 pix
        }
    }

public:
    PageTwoValues(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageTwoValues");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
        // lengthformat = commonData->config->getString(commonData->config->lengthFormat);
        useSimuData = commonData->config->getBool(commonData->config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        backlightMode = commonData->config->getString(commonData->config->backlight);
        tempFormat = commonData->config->getString(commonData->config->tempFormat); // [K|°C|°F]
    }

    virtual void setupKeys()
    {
        Page::setupKeys();

#if defined BOARD_OBP60S3
        constexpr int ZOOM_KEY = 4;
#elif defined BOARD_OBP40S3
        constexpr int ZOOM_KEY = 1;
#endif

        if (dataHstryBuf[0] || dataHstryBuf[1]) { // show "Mode" key only if at least 1 chart supported boat data type is available
            commonData->keydata[0].label = "MODE";
            commonData->keydata[ZOOM_KEY].label = "ZOOM";
        } else {
            commonData->keydata[0].label = "";
            commonData->keydata[ZOOM_KEY].label = "";
        }
    }

    // Key functions
    virtual int handleKey(int key)
    {
        if (dataHstryBuf[0] || dataHstryBuf[1]) { // if at least 1 boat data type supports charts

            // Set page mode: value | value/half chart | full charts
            if (key == 1) {
                switch (pageMode) {

                case VALUES:

                    if (dataHstryBuf[0]) {
                        pageMode = VAL1_CHART;
                    } else if (dataHstryBuf[1]) {
                        pageMode = VAL2_CHART;
                    }
                    break;

                case VAL1_CHART:

                    if (dataHstryBuf[1]) {
                        pageMode = VAL2_CHART;
                    } else {
                        pageMode = CHARTS;
                    }
                    break;

                case VAL2_CHART:
                    pageMode = CHARTS;
                    break;

                case CHARTS:
                    pageMode = VALUES;
                    break;
                }
                return 0; // Commit the key
            }

            // Set time frame to show for history chart
#if defined BOARD_OBP60S3
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
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData& pageData)
    {
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        // buffer initialization will fail, if page is default page, because <displayNew> is not executed at system start for default page
        for (int i = 0; i < NUMVALUES; i++) {
            if (!dataChart[i]) { // Create chart objects if they don't exist

                GwApi::BoatValue* bValue = pageData.values[i]; // Page boat data element
                String bValName = bValue->getName(); // Value name
                String bValFormat = bValue->getFormat(); // Value format

                dataHstryBuf[i] = pageData.hstryBuffers->getBuffer(bValName);

                if (dataHstryBuf[i]) {
                    dataChart[i].reset(new Chart(*dataHstryBuf[i], Chart::dfltChrtDta[bValFormat].range, *commonData, useSimuData));
                    LOG_DEBUG(GwLog::DEBUG, "PageTwoValues: Created chart object%d for %s", i, bValName.c_str());
                } else {
                    LOG_DEBUG(GwLog::DEBUG, "PageTwoValues: No chart object available for %s", bValName.c_str());
                }
            }
        }

        setupKeys(); // adjust <mode> key depending on chart supported boat data type
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageTwoValues");

        // Get boat values for page
        std::vector<GwApi::BoatValue*> bValue;
        bValue.push_back(pageData.values[0]); // Page boat data element 1
        bValue.push_back(pageData.values[1]); // Page boat data element 2

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (bValue[0] == NULL && bValue[1] == NULL)
            return PAGE_OK; // no data, no page to display

        LOG_DEBUG(GwLog::DEBUG, "PageTwoValues: printing #1: %s, %.3f, #2: %s, %.3f",
            bValue[0]->getName().c_str(), bValue[0]->value, bValue[1]->getName().c_str(), bValue[1]->value);

        // Draw page
        //***********************************************************

        getdisplay().setPartialWindow(0, 0, width, height); // Set partial update

        if (pageMode == VALUES || (dataHstryBuf[0] == nullptr && dataHstryBuf[1] == nullptr)) {
            // show only data value; ignore other pageMode options if no chart supported boat data history buffer is available
            showData(bValue, FULL);

        } else if (pageMode == VAL1_CHART) { // show data value 1 and chart
            showData({bValue[0]}, HALF);
            if (dataChart[0]) {
                dataChart[0]->showChrt(HORIZONTAL, HALF_SIZE_BOTTOM, dataIntv, NO_PRNT_NAME, NO_PRNT_VALUE, *bValue[0]);
            }

        } else if (pageMode == VAL2_CHART) { // show data value 2 and chart
            showData({bValue[1]}, HALF);
            if (dataChart[1]) {
                dataChart[1]->showChrt(HORIZONTAL, HALF_SIZE_BOTTOM, dataIntv, NO_PRNT_NAME, NO_PRNT_VALUE, *bValue[1]);
            }

        } else if (pageMode == CHARTS) { // show both data charts
            if (dataChart[0]) {
                if (dataChart[1]) {
                    dataChart[0]->showChrt(HORIZONTAL, HALF_SIZE_TOP, dataIntv, PRNT_NAME, PRNT_VALUE, *bValue[0]);
                } else {
                    dataChart[0]->showChrt(HORIZONTAL, FULL_SIZE, dataIntv, PRNT_NAME, PRNT_VALUE, *bValue[0]);
                }
            }
            if (dataChart[1]) {
                if (dataChart[0]) {
                    dataChart[1]->showChrt(HORIZONTAL, HALF_SIZE_BOTTOM, dataIntv, PRNT_NAME, PRNT_VALUE, *bValue[1]);
                } else {
                    dataChart[1]->showChrt(HORIZONTAL, FULL_SIZE, dataIntv, PRNT_NAME, PRNT_VALUE, *bValue[1]);
                }
            }
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageTwoValues(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageTwoValues(
    "TwoValues", // Page name
    createPage, // Action
    2, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
