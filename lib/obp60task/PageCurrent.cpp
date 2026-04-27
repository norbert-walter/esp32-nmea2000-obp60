#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include <unordered_map>

// leeway K coefficient selection options from OBP configuration page
static const std::unordered_map<std::string, int> leeKMap = {
    { "---", 0 },
    { "Racer [2]", 2 },
    { "Multihull Draggerboard [4]", 4 },
    { "Performance Cruiser [6]", 6 },
    { "Family Cruiser [9]", 9 },
    { "Heavy Displacement [13]", 13 },
    { "Multihull Fixed Keel [16]", 16 }
};

enum bValIdx {
    ROLL = 0,
    SET,
    DFT,
    HDT,
    STW,
    COG,
    SOG,
    HDM,
    VAR,
    AWA,
    NUM_VALS
};

// Screen coordinates for boat values
struct Points {
    int16_t x1, y1;
    int16_t x2, y2;
    int16_t x3, y3;
};

// Screen coordinates for four boat data values (top-left, bottom-left, top-right, bottom-right)
static constexpr Points POS[] = {
    { 10, 65, 10, 95, 10, 115 }, // Position top left for value, name, unit
    { 10, 270, 10, 220, 10, 190 }, // Position bottom left
    { 295, 65, 390, 95, 390, 115 }, // Position top right
    { 295, 270, 390, 220, 390, 190 } // Position bottom right
};

// Define wave visual (XBM Format)
static constexpr int WAVE_W = 132;
static constexpr int WAVE_H = 20;
static const uint8_t wave_bitmap[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x80, 0x00, 0xff, 0xf8, 0x00, 0x00, 0xff, 0x80, 0x00,
    0x7f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x03, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xf0,
    0x01, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xbf, 0xe0, 0x3f, 0xc0, 0xe2, 0x07,
    0xff, 0x1f, 0xf8, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x80, 0x07, 0xff, 0x80,
    0x00, 0xff, 0xff, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x07, 0xfe, 0x00, 0x00, 0x78,
    0x0f, 0x80, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x01, 0xff, 0x00,
    0x00, 0x3f, 0xf0, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0x3f, 0xff, 0x80, 0x07, 0xff,
    0xe0, 0x00, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x7e, 0x00, 0xff, 0xff, 0xf0, 0x3f,
    0xe3, 0xf8, 0x01, 0xc0, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xe7, 0xfc, 0x0f, 0xff,
    0xff, 0x00, 0x3f, 0x00, 0x00, 0x1f, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xf0, 0x03,
    0xff, 0xfc, 0x00, 0x03, 0xf0, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc0,
    0x00, 0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c,
    0x00, 0x00, 0x07, 0x80, 0xfc, 0x00, 0x00, 0x3f, 0xc0, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xe0, 0x00, 0x03, 0xff, 0x80, 0x01, 0xff, 0xf8, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1e, 0x00, 0x0f, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x00, 0xfc, 0x0f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x3e, 0x03, 0xff, 0xff, 0xe0, 0x3f, 0xff, 0xe0, 0x01, 0xe0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x7f, 0xff, 0x00, 0x07, 0xff, 0x80, 0x00,
    0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0, 0x00, 0x1f, 0xfc, 0x00, 0x00, 0xfe, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

class PageCurrent : public Page {
private:
    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock

    bool useSimuData;
    bool holdValues;
    String flashLED;
    String backlightMode;
    String leeKStd;
    double leeK;

    // Old values for hold function
    String sValueOld[NUM_VALS] = { "", "", "", "", "", "", "", "", "", "" };
    String unitOld[NUM_VALS] = { "", "", "", "", "", "", "", "", "", "" };

    struct Current {
        double set; // direction TO which current flows (0..2*PI, true dir)
        double dft; // current drift speed (m/s)
    };

    double calcLeeway(
        double leeK, // leeway coefficient
        double roll, // heel of boat (rad)
        double stw // speed through water (m/s)
    )
    {
        if (stw == 0) {
            return 0;
        }

        double lay = leeK * roll / (stw * stw);
        return lay;
    }

    double calcCTW(
        double awa, // apparent wind angle (0..2PI -> representation within OBP60)
        double hdt, // heading true (rad)
        double lay // leeway angle (rad)
    )
    {
        double ctw; // course through water (rad true)

        if (awa >= M_PI) { // apparent wind > 180° -> comes from port
            ctw = hdt + lay;
        } else { // wind comes from starboard
            ctw = hdt - lay;
        }

        return ctw;
    }

    Current calcSetAndDrift(
        double lay, // leeway angle (rad)
        double sog, // speed over ground (m/s)
        double cog, // course over ground (rad true)
        double stw, // speed through water (m/s)
        double hdt, // heading true (rad)
        double awa // apparent wind angle (rad)
    )
    {
        Current crnt;
        double ctw; // course through water (rad true)

        ctw = calcCTW(awa, hdt, lay);

        // Ground velocity vector (east, north)
        double vg_x = sog * std::sin(cog);
        double vg_y = sog * std::cos(cog);

        // Water-relative velocity vector (east, north)
        double vw_x = stw * std::sin(ctw);
        double vw_y = stw * std::cos(ctw);

        // Current vector = ground - water
        double vc_x = vg_x - vw_x;
        double vc_y = vg_y - vw_y;

        crnt.dft = std::sqrt(vc_x * vc_x + vc_y * vc_y);

        crnt.set = std::atan2(vc_x, vc_y); // atan2(x, y) because 0° = North, clockwise positive
        crnt.set = WindUtils::to2PI(crnt.set); // internal respresentation of wind is [0..2PI]

        // LOG_DEBUG(GwLog::DEBUG, "PageCurrent-setDrift: sog: %.3f, ctw: %.3f, stw: %.3f, vc_x: %.3f, vc_y: %.3f, set: %.3f, drift: %.3f",
        //     sog, ctw, stw, vc_x, vc_y, crnt.set, crnt.dft);

        return crnt;
    };

    // Draw compass rose with boat direction up; top heading of compass rose in degree [0..2PI]
    void drawCompassRose(double topHeading)
    {
        int radius = 110;
        String sDegree;
        int centerX = 200;
        int centerY = 150;

        getdisplay().fillCircle(centerX, centerY, radius + 10, commonData->fgcolor);
        getdisplay().fillCircle(centerX, centerY, radius + 7, commonData->bgcolor);

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        for (int i = 0; i < 360; i += 10) {

            float topAngle = (float)(i * DEG_TO_RAD - topHeading); // Calculate the relative angle for drawing; subtracting topHeading shifts the entire rose
            float sinVal = sin(topAngle);
            float cosVal = cos(topAngle);

            // Coordinates for the labels (positioned slightly inside the rose)
            float xLabel = centerX + (radius - 27) * sinVal;
            float yLabel = centerY - (radius - 24) * cosVal;

            if (i % 30 == 0) {
                // Draw scale markers (triangles)
                float dx = 1;
                float xx1 = -dx;
                float xx2 = +dx;
                float yy1 = -(radius - 10);
                float yy2 = -(radius + 10);

                getdisplay().fillTriangle(centerX + (int)(cosVal * xx1 - sinVal * yy1), centerY + (int)(sinVal * xx1 + cosVal * yy1),
                    centerX + (int)(cosVal * xx2 - sinVal * yy1), centerY + (int)(sinVal * xx2 + cosVal * yy1),
                    centerX + (int)(cosVal * xx1 - sinVal * yy2), centerY + (int)(sinVal * xx1 + cosVal * yy2), commonData->fgcolor);
                getdisplay().fillTriangle(centerX + (int)(cosVal * xx2 - sinVal * yy1), centerY + (int)(sinVal * xx2 + cosVal * yy1),
                    centerX + (int)(cosVal * xx1 - sinVal * yy2), centerY + (int)(sinVal * xx1 + cosVal * yy2),
                    centerX + (int)(cosVal * xx2 - sinVal * yy2), centerY + (int)(sinVal * xx2 + cosVal * yy2), commonData->fgcolor);

                // Print degree labels
                sDegree = String(i);
                int16_t x1, y1;
                uint16_t w, h;
                displayGetTextBounds(sDegree, (int)xLabel, (int)yLabel, &x1, &y1, &w, &h);
                getdisplay().setCursor(xLabel - w / 2, yLabel + h / 2);
                getdisplay().print(sDegree);

            } else {
                // Draw dots for intermediate 10 degree steps
                float xDot = centerX + radius * sinVal;
                float yDot = centerY - radius * cosVal;
                getdisplay().fillCircle((int)xDot, (int)yDot, 2, commonData->fgcolor);
            }
        }
    }

    void drawRotatedArrow(
        float size, // size of the arrow = speed in m/s [0.5..5.2]
        float direction // angle the arrow is pointing to [0..2PI]
    )
    {
        if (size < 0.05) { // no arrow for current set below 0.05 m/s
            return;
        }

        // size [0.5 .. 5.2] -> length [60 .. 130]
        float maxsize = constrain(size, 0.5, 5.2);
        float length = 60.0 + ((maxsize - 0.5) / (5.2 - 0.5)) * (130.0 - 60.0);
        int16_t cx = width / 2;
        int16_t cy = height / 2;

        // geometry (arrow upwards)
        float h = length / 2.0;
        float w = length / 4.0;
        float headW = length / 1.5;
        float splitY = -h / 3;

        struct Pt {
            float x, y;
        };

        // 7 edges of arrow
        Pt p[7] = {
            { -w / 2, h }, { w / 2, h }, { w / 2, splitY }, { headW / 2, splitY },
            { 0, -h }, { -headW / 2, splitY }, { -w / 2, splitY }
        };

        // edges of arrow with reduced size of 1 px for thicker frame
        float offset = 1.0;
        Pt p_in[7] = {
            { -w / 2 + offset, h - offset }, { w / 2 - offset, h - offset },
            { w / 2 - offset, splitY + offset }, { headW / 2 - offset, splitY + offset },
            { 0, -h + offset }, { -headW / 2 + offset, splitY + offset },
            { -w / 2 + offset, splitY + offset }
        };

        // calculate rotation
        float s = sin(direction);
        float c = cos(direction);
        int16_t rx[7], ry[7];

        for (int i = 0; i < 7; i++) {
            // rotate frame
            rx[i] = cx + (int16_t)(p[i].x * c - p[i].y * s);
            ry[i] = cy + (int16_t)(p[i].x * s + p[i].y * c);
        }

        // fill arrow with white area to delete any background data
        getdisplay().fillTriangle(rx[0], ry[0], rx[1], ry[1], rx[2], ry[2], commonData->bgcolor);
        getdisplay().fillTriangle(rx[0], ry[0], rx[2], ry[2], rx[6], ry[6], commonData->bgcolor);
        getdisplay().fillTriangle(rx[5], ry[5], rx[4], ry[4], rx[3], ry[3], commonData->bgcolor);

        // draw arrow frame with "overdraw" for constant thickness
        for (int i = 0; i < 7; i++) {
            int next = (i + 1) % 7;
            int16_t x0 = rx[i], y0 = ry[i];
            int16_t x1 = rx[next], y1 = ry[next];

            // original line
            getdisplay().drawLine(x0, y0, x1, y1, commonData->fgcolor);

            // offset by  1 pixel in x or y depending on delta
            if (abs(x1 - x0) > abs(y1 - y0)) {
                getdisplay().drawLine(x0, y0 + 1, x1, y1 + 1, commonData->fgcolor);
            } else {
                getdisplay().drawLine(x0 + 1, y0, x1 + 1, y1, commonData->fgcolor);
            }
        }
    }

public:
    PageCurrent(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageCurrent");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
        useSimuData = commonData->config->getBool(commonData->config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        backlightMode = commonData->config->getString(commonData->config->backlight);

        leeKStd = commonData->config->getString(commonData->config->leeKStd);
        auto it = leeKMap.find(leeKStd.c_str());
        if (it != leeKMap.end()) {
            leeK = it->second;
        } else if (leeKStd == "Individual value") {
            leeK = (commonData->config->getString(commonData->config->leeK)).toDouble();
        }
        LOG_DEBUG(GwLog::DEBUG, "PageCurrent: leeKStd: %s, leeK: %.3f", leeKStd.c_str(), leeK);
    }

    virtual int handleKey(int key)
    {
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
    }

    int displayPage(PageData& pageData)
    {
        double lay;
        Current current;

        LOG_DEBUG(GwLog::LOG, "Display PageCurrent");

        // Get boat values for page
        // 0=ROLL, 1=SET, 2=DFT, 3=HDT, 4=STW, 5=COG, 6=SOG, 7=HDM, 8=VAR, 9=AWA
        std::vector<GwApi::BoatValue*> bValue(pageData.values.begin(), pageData.values.end());

        LOG_DEBUG(GwLog::DEBUG, "PageCurrent: printing #1: %s, %.3f, valid: %d, #2: %s, %.3f, valid: %d, #3: %s, %.3f, valid: %d, #4: %s, %.3f, valid: %d, #5: %s, %.3f, valid: %d",
            bValue[ROLL]->getName().c_str(), bValue[ROLL]->value, bValue[ROLL]->valid, bValue[SET]->getName().c_str(), bValue[SET]->value, bValue[SET]->valid,
            bValue[DFT]->getName().c_str(), bValue[DFT]->value, bValue[DFT]->valid, bValue[HDT]->getName().c_str(), bValue[HDT]->value, bValue[HDT]->valid,
            bValue[STW]->getName().c_str(), bValue[STW]->value, bValue[STW]->valid);

        // Calculate current data
        //***********************************************************

        if (!bValue[SET]->valid || !bValue[DFT]->valid) { // If SET or DRIFT not available, try to calculate them
            if (bValue[SOG]->valid && bValue[COG]->valid && bValue[STW]->valid && bValue[AWA]->valid) { // calculate current only if all required values are available

                if (!bValue[HDT]->valid) { // HDT not available
                    if (bValue[HDM]->valid) {
                        if (bValue[VAR]->valid) {
                            bValue[HDT]->value = bValue[HDM]->value + bValue[VAR]->value; // Use corrected HDM if HDT is not available
                            bValue[HDT]->value = WindUtils::to2PI(bValue[HDT]->value);
                            bValue[HDT]->valid = true;
                        } else {
                            // if HDT cannot be fully substituted by HDM+VAR, continue with HDM only
                            bValue[HDT] = bValue[HDM];
                        }
                    }
                }

                if (!bValue[ROLL]->valid || (bValue[ROLL]->getFormat() != "formatXdr:A:D" && bValue[ROLL]->getFormat() != "formatXdr:A:rd" && bValue[ROLL]->getFormat() != "formatCourse")) {
                    bValue[ROLL]->value = 0; // delete last value if roll value is not valid anymore (gateway keeps last value) or wrong type selected by user
                }
                LOG_DEBUG(GwLog::DEBUG, "PageCurrent: Roll: %s, %.3f, valid: %d, format: %s",
                    bValue[ROLL]->getName().c_str(), bValue[ROLL]->value, bValue[ROLL]->valid, bValue[ROLL]->getFormat().c_str());

                lay = calcLeeway(leeK, bValue[ROLL]->value, bValue[STW]->value);

                if (bValue[HDT]->valid) { // That's either HDT or HDM
                    current = calcSetAndDrift(lay, bValue[SOG]->value, bValue[COG]->value, bValue[STW]->value, bValue[HDT]->value, bValue[AWA]->value);
                    bValue[SET]->value = current.set;
                    bValue[SET]->setFormat("formatCourse");
                    bValue[SET]->valid = true;
                    bValue[DFT]->value = current.dft;
                    bValue[DFT]->setFormat("formatKnots");
                    bValue[DFT]->valid = true;
                }
            }
        }

        LOG_DEBUG(GwLog::DEBUG, "PageCurrent: leeKStd: %s, leeK: %.3f, lay: %.3f, roll: %.3f, stw: %.3f, awa: %.3f, hdt: %.3f, cog: %.3f, sog: %.3f, set: %.3f, dft: %.3f",
            leeKStd.c_str(), leeK, lay, bValue[ROLL]->value, bValue[STW]->value, bValue[AWA]->value, bValue[HDT]->value, bValue[COG]->value, bValue[SOG]->value,
            bValue[SET]->value, bValue[DFT]->value);

        // Draw page
        //***********************************************************

        displaySetPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        static const int bValType[] = { HDT, STW, SET, DFT }; // Sequence of boat data types to show on page
        for (int i = 0; i < 4; i++) {
            String name = xdrDelete(bValue[bValType[i]]->getName()); // Value name
            name = name.substring(0, 4); // String length limit for value name
            String sValue = formatValue(bValue[bValType[i]], *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            String unit = formatValue(bValue[bValType[i]], *commonData).unit; // Unit of value
            unit = unit.substring(0, 6); // String length limit for value unit

            // Show boat data value
            getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
            getdisplay().setCursor(POS[i].x1, POS[i].y1);
            if (!holdValues || useSimuData) {
                getdisplay().print(sValue);
            } else {
                getdisplay().print(sValueOld[i]);
            }

            // Show name
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            if (i < 2) { // left side values
                getdisplay().setCursor(POS[i].x2, POS[i].y2);
                getdisplay().print(name);
            } else { // right side values
                drawTextRalign(POS[i].x2, POS[i].y2, name);
            }

            String dUnit; // Unit to show
            if (holdValues) {
                dUnit = unitOld[i];
            } else {
                dUnit = unit;
            }
            // Show unit
            getdisplay().setFont(&Ubuntu_Bold8pt8b);
            if (i < 2) { // left side values
                getdisplay().setCursor(POS[i].x3, POS[i].y3);
                getdisplay().print(dUnit);
            } else { // right side values
                drawTextRalign(POS[i].x3, POS[i].y3, dUnit);
            }

            if (bValue[bValType[i]]->valid) {
                sValueOld[i] = sValue; // Save the old value
                unitOld[i] = unit; // Save the old unit
            }
        }

        // Horizontal separator left
        getdisplay().fillRect(0, 149, 60, 2, commonData->fgcolor);
        // Horizontal separator right
        getdisplay().fillRect(339, 149, 60, 2, commonData->fgcolor);

        if (bValue[HDT]->valid) {
            drawCompassRose(bValue[HDT]->value);
        } else {
            drawCompassRose(0.0);
        };
        getdisplay().drawBitmap((width - WAVE_W) / 2, (height - WAVE_H) / 2, wave_bitmap, WAVE_W, WAVE_H, commonData->fgcolor);
        if (bValue[SET]->valid && bValue[DFT]->valid) {
            drawRotatedArrow(bValue[DFT]->value, WindUtils::to2PI(bValue[SET]->value - bValue[HDT]->value));
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageCurrent(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageCurrent(
    "Current", // Page name
    createPage, // Action
    1, // Number of bus values depends on selection in Web configuration -> boat value for ROLL
    { "SET", "DFT", "HDT", "STW", "COG", "SOG", "HDM", "VAR", "AWA" }, // Bus values we need in the page
    true // Show display header on/off
);

#endif
