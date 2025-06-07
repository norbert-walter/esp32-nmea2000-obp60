#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

// these constants have to match the declaration below in :
//      PageDescription registerPageCompass(
//      {"COG","HDT", "HDM"}, // Bus values we need in the page
const int HowManyValues = 6;

const int AverageValues = 4;

const int ShowHDM = 0;
const int ShowHDT = 1;
const int ShowCOG = 2;
const int ShowSTW = 3;
const int ShowSOG = 4;
const int ShowDBS = 5;

const int Compass_X0 = 200;         // center point of compass band
const int Compass_Y0 = 220;         // position of compass lines
const int Compass_LineLength = 22;      // length of compass lines
const float Compass_LineDelta = 8.0;        // compass band: 1deg = 5 Pixels, 10deg = 50 Pixels 

class PageCompass : public Page
{
    int WhichDataCompass = ShowHDM; 
    int WhichDataDisplay = ShowHDM; 

    public:
    PageCompass(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageCompass");
    }

    virtual void setupKeys(){
        Page::setupKeys();
        commonData->keydata[0].label = "CMP";
        commonData->keydata[1].label = "SRC";
   }
    
    virtual int handleKey(int key){
        // Code for keylock
   
        if ( key == 1 ) {
            WhichDataCompass += 1;
            if ( WhichDataCompass > ShowCOG)
                 WhichDataCompass = ShowHDM;
           return 0;
            }
      if ( key == 2 ) {
            WhichDataDisplay += 1;
            if ( WhichDataDisplay > ShowDBS)
                 WhichDataDisplay = ShowHDM;
             }

        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;

    
            // Old values for hold function
            static String OldDataText[HowManyValues] = {"", "", "","", "", ""};
            static String OldDataUnits[HowManyValues] = {"", "", "","", "", ""};
    
            // Get config data
            String lengthformat = config->getString(config->lengthFormat);
            // bool simulation = config->getBool(config->useSimuData);
            bool holdvalues = config->getBool(config->holdvalues);
            String flashLED = config->getString(config->flashLED);
            String backlightMode = config->getString(config->backlight);
            
            GwApi::BoatValue *bvalue;
            String DataName[HowManyValues];
            double DataValue[HowManyValues];
            bool DataValid[HowManyValues];
            String DataText[HowManyValues];
            String DataUnits[HowManyValues];
            String DataFormat[HowManyValues];
            FormatedData TheFormattedData;
    
            for (int i = 0; i < HowManyValues; i++){
                bvalue = pageData.values[i];
                TheFormattedData = formatValue(bvalue, *commonData);   
                DataName[i] = xdrDelete(bvalue->getName());
                DataName[i] = DataName[i].substring(0, 6);                  // String length limit for value name
                DataUnits[i] = formatValue(bvalue, *commonData).unit;   
                DataText[i] = TheFormattedData.svalue;    // Formatted value as string including unit conversion and switching decimal places
                DataValue[i] = TheFormattedData.value;                 // Value as double in SI unit
                DataValid[i] = bvalue->valid;
                DataFormat[i] = bvalue->getFormat();     // Unit of value
                LOG_DEBUG(GwLog::LOG,"Drawing at PageCompass: %d %s %f %s %s",  i,  DataName[i], DataValue[i], DataFormat[i], DataText[i] );
            }

            // Optical warning by limit violation (unused)
            if(String(flashLED) == "Limit Violation"){
                setBlinkingLED(false);
                setFlashLED(false); 
            }
    
            if (bvalue == NULL) return;
           
            //***********************************************************
    
            // Set display in partial refresh mode
            getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
            getdisplay().setTextColor(commonData->fgcolor);

         // Horizontal line 3 pix top & bottom
         // print Data on top half
            getdisplay().fillRect(0, 23, 400, 3, commonData->fgcolor); 
            getdisplay().setFont(&Ubuntu_Bold20pt7b);
            getdisplay().setCursor(10, 70);
            getdisplay().print(DataName[WhichDataDisplay]);                           // Page name
     // Show unit
            getdisplay().setFont(&Ubuntu_Bold12pt7b);
            getdisplay().setCursor(10, 120);
            getdisplay().print(DataUnits[WhichDataDisplay]); 
            getdisplay().setCursor(200, 120);
            getdisplay().setFont(&DSEG7Classic_BoldItalic42pt7b);
  
         if(holdvalues == false){
            getdisplay().print(DataText[WhichDataDisplay]);                                     // Real value as formated string
        }
        else{
            getdisplay().print(OldDataText[WhichDataDisplay]);                                  // Old value as formated string
        }
        if(DataValid[WhichDataDisplay] == true){
            OldDataText[WhichDataDisplay] = DataText[WhichDataDisplay];                                       // Save the old value
            OldDataUnits[WhichDataDisplay] = DataUnits[WhichDataDisplay];                                           // Save the old unit
        }  
        // now draw compass band                
        // get the data
            double TheAngle = DataValue[WhichDataCompass];
            static double AvgAngle = 0;
            AvgAngle = ( AvgAngle * AverageValues + TheAngle ) / (AverageValues + 1 );

            int TheTrend = round( ( TheAngle - AvgAngle) * 180.0 / M_PI );
  
            static const int bsize = 30;
            char buffer[bsize+1];
            buffer[0]=0;              
 
            getdisplay().setFont(&Ubuntu_Bold16pt7b);
            getdisplay().setCursor(10, Compass_Y0-60);
            getdisplay().print(DataName[WhichDataCompass]);                           // Page name
 
     
  // draw compass base line and pointer
            getdisplay().fillRect(0, Compass_Y0, 400, 3, commonData->fgcolor);            
            getdisplay().fillTriangle(Compass_X0,Compass_Y0-40,Compass_X0-10,Compass_Y0-80,Compass_X0+10,Compass_Y0-80,commonData->fgcolor);
// draw trendlines
            for ( int i = 1; i < abs(TheTrend) / 2; i++){
                int x1;
                if ( TheTrend < 0 )
                     x1 = Compass_X0 +  20 * i;
                else
                     x1 = Compass_X0 - 20 * ( i + 1 );

                getdisplay().fillRect(x1, Compass_Y0 -60, 10, 6, commonData->fgcolor);            
                 }
// central line +  satellite lines
            double NextSector = round(TheAngle / ( M_PI / 9 )) * ( M_PI / 9 );       // get the next 20degree value
            double Offset = - ( NextSector -  TheAngle);                  // offest of the center line compared to TheAngle in Radian
            
            int Delta_X = int (  Offset * 180.0 / M_PI  * Compass_LineDelta );
            for ( int i = 0; i <=4; i++ )
                {
                    int x0;
                    x0 = Compass_X0 + Delta_X + 2 * i * 5 * Compass_LineDelta;
                    getdisplay().fillRect(x0-1, Compass_Y0 - 2 * Compass_LineLength,3, 2 * Compass_LineLength, commonData->fgcolor);
                    x0 = Compass_X0 + Delta_X + ( 2 * i + 1 ) * 5 * Compass_LineDelta;
                    getdisplay().fillRect(x0-1, Compass_Y0 - Compass_LineLength,3, Compass_LineLength, commonData->fgcolor);

                    x0 = Compass_X0 + Delta_X - 2 * i * 5 * Compass_LineDelta;
                    getdisplay().fillRect(x0-1, Compass_Y0 - 2 * Compass_LineLength,3, 2 * Compass_LineLength, commonData->fgcolor);
                    x0 = Compass_X0 + Delta_X - ( 2 * i + 1 ) * 5 * Compass_LineDelta;
                    getdisplay().fillRect(x0-1, Compass_Y0 - Compass_LineLength,3, Compass_LineLength, commonData->fgcolor);
                }

            getdisplay().fillRect(0, Compass_Y0, 400, 3, commonData->fgcolor);
    // add the numbers to the compass band
            int x0;
            float AngleToDisplay = NextSector * 180.0 / M_PI;

            x0 = Compass_X0 + Delta_X;
            getdisplay().setFont(&DSEG7Classic_BoldItalic16pt7b);

            do {
                    getdisplay().setCursor(x0 - 40, Compass_Y0 + 40);
                    snprintf(buffer,bsize,"%03.0f", AngleToDisplay);
                    getdisplay().print(buffer);
                    AngleToDisplay += 20;
                    if ( AngleToDisplay >= 360.0 )
                        AngleToDisplay -= 360.0;
                    x0 -= 4 * 5 * Compass_LineDelta;
            } while (  x0 >= 0 - 60  );
           
            AngleToDisplay = NextSector * 180.0 / M_PI - 20;
            if ( AngleToDisplay < 0 )
                AngleToDisplay += 360.0;
 
            x0 = Compass_X0 + Delta_X + 4 * 5 * Compass_LineDelta;
            do {
                    getdisplay().setCursor(x0 - 40, Compass_Y0 + 40);
                    snprintf(buffer,bsize,"%03.0f", AngleToDisplay);
                    // quick and dirty way to prevent wrapping text in next line
                if ( ( x0 - 40 ) > 380 )
                    buffer[0] = 0;
                else if ( ( x0 - 40 )  > 355 )
                     buffer[1] = 0;
                else if ( ( x0 - 40 )  >  325 ) 
                    buffer[2] = 0;
                 
                getdisplay().print(buffer);             
 
                AngleToDisplay -= 20;
                if ( AngleToDisplay < 0 )
                   AngleToDisplay += 360.0;
                x0 += 4 * 5 * Compass_LineDelta;
            } while (x0 < ( 400 - 20 -40 ) );
 
            // static int x_test = 320;
            // x_test += 2;

            // snprintf(buffer,bsize,"%03d", x_test);
            // getdisplay().setCursor(x_test, Compass_Y0 - 60);
            // getdisplay().print(buffer);
            // if ( x_test > 390)
            //     x_test = 320;

             // Update display
            getdisplay().nextPage();    // Partial update (fast)
    
        };
    
    };
static Page *createPage(CommonData &common){
    return new PageCompass(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageCompass(
    "Compass",    // Page name
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {"HDM","HDT", "COG", "STW", "SOG", "DBS"}, // Bus values we need in the page
   true            // Show display header on/off
);

#endif
