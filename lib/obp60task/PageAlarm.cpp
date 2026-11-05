#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPAlarms.h"
#include <vector>

class PageAlarm : public Page {
private:
    enum BuzzerMode {
        OFF,
        SHORT,
        LONG,
        TIME,
        ACK
    };

    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock

    // String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    bool smallDecimals;
    String distanceFormat;
    String sBuzzerMode;
    BuzzerMode buzzerMode;
    ulong buzzerDuration;

    Alarms* boatAlarms = nullptr;
    bool flashHdr = true; // Controls flashing of "Alarm" headline

    // Old values for hold function
    String sValue1Old = "";
    String unit1Old = "";

public:
    PageAlarm(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        GwConfigHandler* config = commonData->config;

        LOG_DEBUG(GwLog::LOG, "Instantiate PageAlarm");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height
        getdisplay().setTextWrap(false);

        // Get config data
        useSimuData = config->getBool(config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        smallDecimals = commonData->config->getBool(commonData->config->smallDecimals);
        distanceFormat = commonData->config->getString(commonData->config->distanceFormat);
        buzzerDuration = ulong(config->getString(commonData->config->buzzerTime, "").toInt() * 1000); // user setting is in seconds
        sBuzzerMode = commonData->config->getString(commonData->config->buzzerMode);
        if (sBuzzerMode == "Off") {
            buzzerMode = OFF;
        } else if (sBuzzerMode == "Short Beep") {
            buzzerMode = SHORT;
        } else if (sBuzzerMode == "Long Beep") {
            buzzerMode = LONG;
        } else if (sBuzzerMode == "Beep Duration") {
            buzzerMode = TIME;
        } else if (sBuzzerMode == "Beep until Confirmation") {
            buzzerMode = ACK;
        } else {
            buzzerMode = SHORT;
        };

        boatAlarms = commonData->alarmList;
    }

    virtual void setupKeys()
    {
        commonData->keydata[0].label = "CNFRM";
        for (int i = 1; i < 5; i++) {
            commonData->keydata[i].label = ""; // delete other key labels except of key 6 <ILUM>
        }
#if defined BOARD_OBP60S3
        constexpr int SNZ_KEY = 4;
#elif defined BOARD_OBP40S3
        constexpr int SNZ_KEY = 1;
#endif
        commonData->keydata[SNZ_KEY].label = "SNOOZE";
    }

    // Key functions
    virtual int handleKey(int key)
    {
        Alarms::tAlarm* m_alarm = boatAlarms->getActiveAlarm(); // get 1st active alarm if any exist

        if (key == 1) {
            if (m_alarm != nullptr) {
                boatAlarms->suspendAlarm(*m_alarm); // handle always first alarm in list
            }
            return 0; // Commit the key
        }

#if defined BOARD_OBP60S3
        if (key == 5) {
#elif defined BOARD_OBP40S3
        if (key == 2) {
#endif
            if (m_alarm != nullptr) {
                boatAlarms->snoozeAlarm(*m_alarm); // handle always first alarm in list
            }
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }

        if (key == 6) { // Code for backlight LED function
            return key; // let function be executed by obp60task()
        }
        
        return 0; // commit any other key -> disable key functions for this page
    }

    virtual void displayNew(PageData& pageData)
    {
        setupKeys();

#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        flashHdr = true; // start new alarm window with activated "Alarm" headline
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageAlarm");

#ifdef BOARD_OBP60S3
        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        if (!boatAlarms->isAlarm())
            return PAGE_OK; // no active alarm, no page to display; should never happen

        std::vector<Alarms::tAlarm>* alarmList;
        alarmList = boatAlarms->getAlarmList();

        LOG_DEBUG(GwLog::DEBUG, "PageAlarm: # of alarms: Defined: %d; active: %d", boatAlarms->getNoOfAlarms(), boatAlarms->countAlarms());

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        int x = 20;
        int y = 50;
        int xTab1 = 45;
        int xTab2 = 125;
        int xTab3 = 245;
        int yTabHdr = 60;
        int yTabLn = 74;
        int lineHgt = 28;

        getdisplay().drawRoundRect(x, y, 360, 222, 5, commonData->fgcolor); // Draw box

        if (flashHdr) { // flash header once per second
            // exclamation icon in left top corner
            getdisplay().drawXBitmap(x + 16, y + 8, exclamation_bits, exclamation_width, exclamation_height, commonData->fgcolor);
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            drawTextCenter(x + 170, y + 20, "ALARM!");
        }
        flashHdr = !flashHdr;

        // print table header
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(x + xTab1, y + yTabHdr);
        getdisplay().print("Type");
        getdisplay().setCursor(x + xTab2, y + yTabHdr);
        getdisplay().print("Value");
        getdisplay().setCursor(x + xTab3, y + yTabHdr);
        getdisplay().print("Threshold");
        getdisplay().drawLine(x + 10, y + yTabHdr + 5, x + 350, y + yTabHdr + 5, commonData->fgcolor);

        int i = 0;
        const GFXfont* textFnt = nullptr;
        const GFXfont* numFnt = nullptr;
        for (auto& alarm : *alarmList) {
            if (boatAlarms->getAlarmState(alarm) == Alarms::AlarmState::ACTIVE) {
                i++;

                if (i == 1 && !(alarm.format == Alarms::BoatDataFmt::DISTANCE || alarm.format == Alarms::BoatDataFmt::TIME)) {
                    // increase font size for 1st alarm because this is the alarm to be acknoledged; <POSITION> and <TIME> alarms are too large for this font size
                    textFnt = &Ubuntu_Bold12pt8b;
                    numFnt = &DSEG7Classic_BoldItalic16pt7b;
                } else {
                    textFnt = &Ubuntu_Bold10pt8b;
                    numFnt = &DSEG7Classic_BoldItalic12pt7b;
                }

                GwApi::BoatValue* bvalue = alarm.boatValue;
                String name = xdrDelete(bvalue->getName());     // Value name
                name = name.substring(0, 6);                    // String length limit for value name
                String svalue = formatValue(bvalue, *commonData).svalue;
                String unit = formatValue(bvalue, *commonData).unit; // Unit of current boat value
                String limitUnit = unit;                        // Unit of alarm limit value; usually the same than boat value unit
                if (alarm.format == Alarms::BoatDataFmt::TIME) {
                    svalue = svalue.substring(0,5);             // truncate seconds from time format output
                } else if (alarm.format == Alarms::BoatDataFmt::DISTANCE && distanceFormat == "m" && bvalue->value > 999.99) {
                    svalue = formatValue(bvalue->value / 1000.0, bvalue->getFormat(), *commonData);
                    unit = "km";
                }
                getdisplay().setCursor(x + 10, y + yTabLn + lineHgt * i);
                getdisplay().setFont(textFnt);
                getdisplay().print("#" + String(i));
                getdisplay().setCursor(x + xTab1, y + yTabLn + lineHgt * i);
                getdisplay().print(name);                       // alarm boat value name
                getdisplay().setFont(numFnt);
                drawTextRalign(x + xTab3 - 49, y + yTabLn + lineHgt * i, svalue, true); // current alarm boat value
                getdisplay().setFont(&Ubuntu_Bold8pt8b);
                getdisplay().print(unit);                       // alarm boat value unit

                double threshold = 0.0;
                if (boatAlarms->triggeredByHigh(alarm)) {
                    threshold = alarm.highLimit; }
                if (boatAlarms->triggeredByLow(alarm)) {
                    threshold = alarm.lowLimit; }
                String sThreshold = formatValue(threshold, bvalue->getFormat(), *commonData);
                if (alarm.format == Alarms::BoatDataFmt::TIME) {
                    sThreshold = sThreshold.substring(0,5); // truncate seconds from time format output
                } else if (alarm.format == Alarms::BoatDataFmt::DISTANCE && distanceFormat == "m" && threshold > 999.99) {
                    sThreshold = formatValue(threshold / 1000.0, bvalue->getFormat(), *commonData);
                    limitUnit = "km";
                }
                getdisplay().setFont(numFnt);
                drawTextRalign(x + 316, y + yTabLn + lineHgt * i, sThreshold, true);        // alarm boat value threshold
                getdisplay().setFont(&Ubuntu_Bold8pt8b);
                getdisplay().print(limitUnit);

#if defined BOARD_OBP60S3
                if (buzzerMode == OFF) {
                    // don't want any buzzer noise
                } else if (buzzerMode == SHORT && alarm.buzzerOn) {
                    buzzer(TONE4, 500);
                    alarm.buzzerOn = false; // single alarm tone only
                } else if (buzzerMode == LONG && alarm.buzzerOn) {
                    buzzer(TONE4, 1000);
                    alarm.buzzerOn = false; // single alarm tone only
                } else if (buzzerMode == TIME && (millis() - alarm.buzzerTime < buzzerDuration)) {
                    // buzzer tone as long as buzzerDuration is not exceeded; <buzzerTime> will be reset at next displayNew()
                    buzzer(TONE4, 500);
                } else if (buzzerMode == ACK) {
                        buzzer(TONE4, 500);
                }
#endif
            }
        }

        // Update display
        displayNextPage(); // Partial update (fast)
        return PAGE_OK;
    }
};

static Page* createPage(CommonData& common)
{
    return new PageAlarm(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAlarm(
    "Alarm", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
