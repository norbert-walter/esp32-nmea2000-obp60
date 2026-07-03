#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPcharts.h"

class PageWeather : public Page {
private:
    GwLog* logger;

    enum PageMode {
        VAL_CHART,
        CHART
    };

    static constexpr int XOFFSET = 133; // x offset for display of boat values

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    PageMode pageMode = VAL_CHART; // Page display mode
    int8_t dataIntv = 1; // Update interval for barograph history chart:
                         // (1)|(3)|(6)|(12)|(24) x 60 min. for 1, 3, 6, 12, 24 hours history chart

    // String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    String backlightMode;
    String tempFormat;

    static constexpr int NUMVALUES = 4; // number of boat values used on this page
    static constexpr int NUMCHARTS = 1; // one data buffer used on this page

    // Data buffer pointer (owned by HstryBuffers)
    RingBuffer<uint16_t>* dataHstryBuf[NUMCHARTS] = { nullptr };
    std::unique_ptr<Chart> dataChart[NUMCHARTS]; // Chart object

    // Old values for hold function
    String sValueOld[NUMVALUES] = { "", "", "", "" };
    String unitOld[NUMVALUES] = { "", "", "", "" };

    // display data values in display mode <HALF>
    void showData(const std::vector<GwApi::BoatValue*>& bValue)
    {
        getdisplay().setTextColor(commonData->fgcolor);

        int numValues = bValue.size(); // How many values do we have to handle? We will ignore value no. 1

        for (int i = 1; i < numValues; i++) {
            String name = xdrDelete(bValue[i]->getName()); // Value name
            name = name.substring(0, 7); // String length limit for value name
            double value = bValue[i]->value; // Value as double in SI unit
            bool valid = bValue[i]->valid; // Valid information
            String sValue = formatValue(bValue[i], *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            String unit = formatValue(bValue[i], *commonData).unit; // Unit of value

            int xOffset = XOFFSET * (i - 1);

            // Print name
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            getdisplay().setCursor(5 + xOffset, 213);
            getdisplay().print(name); // name

            // Print unit
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            getdisplay().setCursor(5 + xOffset, 229);
            if (holdValues) {
                getdisplay().print(unitOld[i]); // name
            } else {
                getdisplay().print(unit); // name
            }

            // Print value
            getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
            if (bValue[i]->getFormat() == "formatXdr:P:P" || bValue[i]->getFormat() == "formatXdr:P:B") {
                getdisplay().setCursor(6 + xOffset, 275); // pressure format is always 4 digits when all other boat data has 3 digit format
            } else {
                getdisplay().setCursor(37 + xOffset, 275);
            }
            if (!holdValues || useSimuData) {
                getdisplay().print(sValue); // Real value as formated string
            } else {
                getdisplay().print(sValueOld[i]); // Old value as formated string
            }

            if (valid) { // Save value for hold function
                sValueOld[i] = sValue;
                unitOld[i] = unit;
            }
        }

        // print lines for data separation of bottom data values
        getdisplay().fillRect(0, 191, 400, 2, commonData->fgcolor); // horizontal line
        getdisplay().fillRect(133, 192, 2, 84, commonData->fgcolor); // vertical lines
        getdisplay().fillRect(266, 192, 2, 84, commonData->fgcolor);
    }

public:
    PageWeather(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageWeather");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
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

        if (dataHstryBuf) { // show "Mode" key only if chart-supported boat data type is available
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
        if (dataHstryBuf) { // if boat data type supports charts

            // Set page mode: value/half chart | full chart
            if (key == 1) {
                switch (pageMode) {
                case VAL_CHART:
                    pageMode = CHART;
                    break;
                case CHART:
                    pageMode = VAL_CHART;
                    break;
                }
                setupKeys(); // Adjust key definition depending on <pageMode> and chart-supported boat data type
                return 0; // Commit the key
            }

            // Set time frame to show for chart
#if defined BOARD_OBP60S3
            if (key == 5) {
#elif defined BOARD_OBP40S3
            if (key == 2) {
#endif
                if (dataIntv == 1) {
                    dataIntv = 2;
                } else if (dataIntv == 2) {
                    dataIntv = 4;
                } else if (dataIntv == 4) {
                    dataIntv = 8;
                } else if (dataIntv == 8) {
                    dataIntv = 12;
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

        for (int i = 0; i < NUMCHARTS; i++) {
            if (!dataChart[i]) { // Create chart objects if they don't exist

                GwApi::BoatValue* bValue = pageData.values[i]; // Page boat data element
                String bValName = bValue->getName(); // Value name
                String bValFormat = bValue->getFormat(); // Value format

                dataHstryBuf[i] = pageData.hstryBuffers->getBuffer(bValName);

                if (dataHstryBuf[i]) {
                    dataChart[i].reset(new Chart(*dataHstryBuf[i], *commonData, useSimuData));
                    LOG_DEBUG(GwLog::DEBUG, "PageWeather: Created chart object %d for %s, format: %s", i, bValName.c_str(), dataHstryBuf[i]->getFormat().c_str());
                } else {
                    LOG_DEBUG(GwLog::DEBUG, "PageWeather: No chart object available for %s", bValName.c_str());
                }
            }
        }
    }

    int displayPage(PageData& pageData)
    {

        LOG_DEBUG(GwLog::LOG, "Display PageWeather");

        // Get latest boat values for page
        std::vector<GwApi::BoatValue*> bValue;
        for (int i = 0; i < NUMVALUES; i++) {
            bValue.push_back(pageData.values[i]);
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (bValue[0] == NULL && bValue[1] == NULL && bValue[2] == NULL && bValue[3] == NULL)
            return PAGE_OK; // no data, no page to display

        LOG_DEBUG(GwLog::DEBUG, "PageWeather: printing #1: %s, %.3f, %s, #2: %s, %.3f, %s, #3: %s, %.3f, %s, #4: %s, %.3f, %s",
            bValue[0]->getName().c_str(), bValue[0]->value, bValue[0]->getFormat().c_str(), bValue[1]->getName().c_str(), bValue[1]->value, bValue[1]->getFormat().c_str(),
            bValue[2]->getName().c_str(), bValue[2]->value, bValue[2]->getFormat().c_str(), bValue[3]->getName().c_str(), bValue[3]->value, bValue[3]->getFormat().c_str());

        // Draw page
        //***********************************************************

        displaySetPartialWindow(0, 0, width, height); // Set partial update

        if (dataHstryBuf == nullptr) { // no buffer for main boat data item, no page display
            return PAGE_UPDATE;
        }
        if (!dataChart[0]->isValid()) {
            dataChart[0]->init(); // try late initialization if chart object could not be properly initialized earlier due to missing boat data
        }

        if (pageMode == VAL_CHART) {
            if (dataChart[0]) {
                dataChart[0]->showChrt(Chart::HORIZONTAL, Chart::TWO_THIRD_TOP, dataIntv, Chart::PRNT_NAME, Chart::PRNT_VALUE, *bValue[0]);
            }
            showData(bValue);

        } else if (pageMode == CHART && dataChart[0]) { // show only data chart, but that has to exist
            dataChart[0]->showChrt(Chart::HORIZONTAL, Chart::FULL_SIZE, dataIntv, Chart::PRNT_NAME, Chart::PRNT_VALUE, *bValue[0]);
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageWeather(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageWeather(
    "Weather", // Page name
    createPage, // Action
    4, // Number of bus values depends on selection in Web configuration (air baro pressure, air temperature, humidity, true wind speed)
    { }, // Bus values we need in the page
    true // Show display header on/off
);

#endif
