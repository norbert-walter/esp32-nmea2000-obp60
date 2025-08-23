// SPDX-License-Identifier: GPL-2.0-or-later
#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

#include <vector>
#include <algorithm> // for vector sorting

/*
 * SkyView / Satellites
 */

class PageSkyView : public Page
{
private:
    String flashLED;
    GwBoatData *bd;

public:
    PageSkyView(CommonData &common)
    {
        commonData = &common;

        // task name access is for example purpose only
        TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
        const char* taskName = pcTaskGetName(currentTaskHandle);
        common.logger->logDebug(GwLog::LOG, "Instantiate PageSkyView in task '%s'", taskName);

        flashLED = common.config->getString(common.config->flashLED);
    }

    int handleKey(int key) {
        // return 0 to mark the key handled completely
        // return the key to allow further action
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0;
        }
        return key;
    }

    void displayNew(PageData &pageData) {
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        bd = pageData.api->getBoatData();
    };

    // Comparator function to sort by SNR
    static bool compareBySNR(const GwSatInfo& a, const GwSatInfo& b) {
       return a.SNR > b.SNR; // Sort in descending order
    }

    int displayPage(PageData &pageData) {
        GwLog *logger = commonData->logger;

        std::vector<GwSatInfo> sats;
        int nSat = bd->SatInfo->getNumSats();

        logger->logDebug(GwLog::LOG, "Drawing at PageSkyView, %d satellites", nSat);

        for (int i = 0; i < nSat; i++) {
            sats.push_back(*bd->SatInfo->getAt(i));
        }
        std::sort(sats.begin(), sats.end(), compareBySNR);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        // current position
        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        // sky view
        Point c = {130, 148};
        uint16_t r = 125;
        uint16_t r1 = r / 2;

        getdisplay().fillCircle(c.x, c.y, r, commonData->bgcolor);
        getdisplay().drawCircle(c.x, c.y, r + 1, commonData->fgcolor);
        getdisplay().drawCircle(c.x, c.y, r + 2, commonData->fgcolor);
        getdisplay().drawCircle(c.x, c.y, r1, commonData->fgcolor);

        // separation lines
        getdisplay().drawLine(c.x - r, c.y, c.x + r, c.y, commonData->fgcolor);
        getdisplay().drawLine(c.x, c.y - r, c.x, c.y + r, commonData->fgcolor);
        Point p = {c.x, c.y - r};
        Point p1, p2;
        p1 = rotatePoint(c, p, 45);
        p2 = rotatePoint(c, p, 45 + 180);
        getdisplay().drawLine(p1.x, p1.y, p2.x, p2.y,  commonData->fgcolor);
        p1 = rotatePoint(c, p, -45);
        p2 = rotatePoint(c, p, -45 + 180);
        getdisplay().drawLine(p1.x, p1.y, p2.x, p2.y, commonData->fgcolor);

        // directions 

        int16_t x1, y1;
        uint16_t w, h;
        getdisplay().setFont(&Ubuntu_Bold12pt8b);

		getdisplay().getTextBounds("N", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - w / 2, c.y - r + h + 2);
        getdisplay().print("N");

		getdisplay().getTextBounds("S", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - w / 2, c.y + r - 2);
        getdisplay().print("S");

		getdisplay().getTextBounds("E", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x + r - w - 2, c.y + h / 2);
        getdisplay().print("E");
        
		getdisplay().getTextBounds("W", 0, 150, &x1, &y1, &w, &h);
        getdisplay().setCursor(c.x - r + 2 , c.y + h / 2);
        getdisplay().print("W");

        getdisplay().setFont(&Ubuntu_Bold8pt8b);

        // show satellites in "map"
        for (int i = 0; i < nSat; i++) {
            float arad = sats[i].Azimut * M_PI / 180.0;
            float erad = sats[i].Elevation * M_PI / 180.0;
            uint16_t x = c.x + sin(arad) * erad * r;
            uint16_t y = c.y + cos(arad) * erad * r;
            getdisplay().drawRect(x-4, y-4, 8, 8, commonData->fgcolor);
        }

        // Signal / Noise bars
        getdisplay().setCursor(325, 34);
        getdisplay().print("SNR");
        getdisplay().drawRect(270, 20, 125, 257, commonData->fgcolor);
        int maxsat = std::min(nSat, 12);
        for (int i = 0; i < maxsat; i++) {
            uint16_t y = 29 + (i + 1) * 20;
            getdisplay().setCursor(276, y);
            char buffer[3];
            snprintf(buffer, 3, "%02d", static_cast<int>(sats[i].PRN));
            getdisplay().print(String(buffer));
            getdisplay().drawRect(305, y-12, 85, 14, commonData->fgcolor);
            getdisplay().setCursor(315, y);
            // TODO SNR as number or as bar via mode key?
            if (sats[i].SNR <= 100) {
                // getdisplay().print(sats[i].SNR);
                getdisplay().fillRect(307, y-10, int(81 * sats[i].SNR / 100.0), 10, commonData->fgcolor);
            } else {
                getdisplay().print("n/a");
            }
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData &common){
    return new PageSkyView(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageSkyView(
    "SkyView",              // Page name
    createPage,             // Action
    0,                      // Number of bus values depends on selection in Web configuration
    true                    // Show display header on/off
);

#endif
