#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include <Arduino.h>
#include <Wire.h>         // I2C
#include <RTClib.h>       // Driver for DS1388 RTC
#include <PCF8574.h>      // PCF8574 modules from Horter
#include "SunRise.h"      // Lib for sunrise and sunset calculation
#include "Pagedata.h"
#include "OBP60Hardware.h"
#include "OBP60Extensions.h"
#include "imglib.h"

// Character sets
#include "fonts/DSEG7Classic-BoldItalic10pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic12pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic16pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic20pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic26pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic30pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic42pt7b.h"
#include "fonts/DSEG7Classic-BoldItalic60pt7b.h"
#include "fonts/Ubuntu_Bold8pt8b.h"
#include "fonts/Ubuntu_Bold10pt8b.h"
#include "fonts/Ubuntu_Bold12pt8b.h"
#include "fonts/Ubuntu_Bold16pt8b.h"
#include "fonts/Ubuntu_Bold20pt8b.h"
#include "fonts/Ubuntu_Bold32pt8b.h"
#include "fonts/Atari16px8b.h" // Key label font
#include "fonts/IBM8x8px.h"

// E-Ink Display
// Definition for e-paper width an height refer OBP60Hardware.h
#ifdef DISPLAY_GDEW042T2
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
// Export display in new funktion
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> & getdisplay(){return display;}
#endif

#ifdef DISPLAY_GDEY042T81
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(GxEPD2_420_GDEY042T81(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
// Export display in new funktion
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> & getdisplay(){return display;}
#endif

#ifdef DISPLAY_GYE042A87
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(GxEPD2_420_GYE042A87(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
// Export display in new funktion
GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> & getdisplay(){return display;}
#endif

#ifdef DISPLAY_SE0420NQ04
// Set display type and SPI pins for display
GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> display(GxEPD2_420_SE0420NQ04(OBP_SPI_CS, OBP_SPI_DC, OBP_SPI_RST, OBP_SPI_BUSY)); // GDEW042T2 400x300, UC8176 (IL0398)
// Export display in new funktion
GxEPD2_BW<GxEPD2_420_SE0420NQ04, GxEPD2_420_SE0420NQ04::HEIGHT> & getdisplay(){return display;}
#endif

#ifdef TFT_DISPLAY
// panel device + offscreen shadow framebuffer
static LGFX panelDisplay;
static LGFXCanvas shadowDisplay(&panelDisplay);
static LGFXCanvas scaleDisplay(&panelDisplay);
static bool shadowDisplayInitialized = false;
static bool scaleDisplayInitialized = false;
static uint16_t scaleDisplayWidth = 0;
static uint16_t scaleDisplayHeight = 0;

LGFXCanvas & getdisplay(){return shadowDisplay;}
LGFX & getpaneldisplay(){return panelDisplay;}
LGFXCanvas & getscaleddisplay(){return scaleDisplay;}

bool initDisplayShadowBuffer(){
    if (shadowDisplayInitialized) return true;

    shadowDisplay.setPsram(true);
    shadowDisplay.setColorDepth(16);
    shadowDisplay.setTextDatum(textdatum_t::baseline_left);

    if (shadowDisplay.createSprite(GxEPD_WIDTH, GxEPD_HEIGHT) == nullptr) {
        shadowDisplayInitialized = false;
        return false;
    }

    shadowDisplay.fillScreen(GxEPD_BLACK);
    shadowDisplayInitialized = true;
    return true;
}

bool initDisplayScaleBuffer(uint16_t width, uint16_t height){
    if (scaleDisplayInitialized && scaleDisplayWidth == width && scaleDisplayHeight == height) {
        return true;
    }

    scaleDisplay.deleteSprite();
    scaleDisplay.setPsram(true);
    scaleDisplay.setColorDepth(16);
    scaleDisplay.setTextDatum(textdatum_t::baseline_left);

    if (scaleDisplay.createSprite(width, height) == nullptr) {
        scaleDisplayInitialized = false;
        scaleDisplayWidth = 0;
        scaleDisplayHeight = 0;
        return false;
    }

    scaleDisplayWidth = width;
    scaleDisplayHeight = height;
    scaleDisplayInitialized = true;
    return true;
}
#endif

// Horter I2C moduls
PCF8574 pcf8574_Modul1(PCF8574_I2C_ADDR1); // First digital IO modul PCF8574 from Horter

// FRAM
Adafruit_FRAM_I2C fram;
bool hasFRAM = false;

// SD Card
#ifdef BOARD_OBP40S3
sdmmc_card_t *sdcard;
#endif
bool hasSDCard = false;

// Global vars
bool blinkingLED = false;       // Enable / disable blinking flash LED
bool statusLED = false;         // Actual status of flash LED on/off
bool statusBacklightLED = false;// Actual status of flash LED on/off

int uvDuration = 0;             // Under voltage duration in n x 100ms

RTC_DATA_ATTR uint8_t RTC_lastpage; // Remember last page while deep sleeping


LedTaskData *ledTaskData=nullptr;

void hardwareInit(GwApi *api)
{
    GwLog *logger = api->getLogger();
    GwConfigHandler *config = api->getConfig();

    Wire.begin(OBP_I2C_SDA, OBP_I2C_SCL);
    // Init PCF8574 digital outputs
    Wire.setClock(I2C_SPEED_LOW);   // Set I2C clock on 10 kHz
    if(pcf8574_Modul1.begin()){        // Initialize PCF8574
        pcf8574_Modul1.write8(255);    // Clear all outputs (low activ)
    }
    Wire.setClock(I2C_SPEED);       // Set I2C clock on 100 kHz
    fram = Adafruit_FRAM_I2C();
    if (esp_reset_reason() ==  ESP_RST_POWERON) {
        // help initialize FRAM
        logger->logDebug(GwLog::LOG, "Delaying I2C init for 250ms due to cold boot");
        delay(250);
    }
    // FRAM (e.g. MB85RC256V)
    if (fram.begin(FRAM_I2C_ADDR)) {
        hasFRAM = true;
        uint16_t manufacturerID;
        uint16_t productID;
        fram.getDeviceID(&manufacturerID, &productID);
        // Boot counter
        uint8_t framcounter = fram.read(0x0000);
        fram.write(0x0000, framcounter+1);
        logger->logDebug(GwLog::LOG, "FRAM detected: 0x%04x/0x%04x (counter=%d)", manufacturerID, productID, framcounter);
    }
    else {
        hasFRAM = false;
        logger->logDebug(GwLog::LOG, "NO FRAM detected");
    }
    // SD Card
    hasSDCard = false;
#ifdef BOARD_OBP40S3
    if (config->getBool(config->useSDCard)) {
        esp_err_t ret;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.slot = SPI3_HOST;
        logger->logDebug(GwLog::DEBUG, "SDSPI_HOST: max_freq_khz=%d" , host.max_freq_khz);
        spi_bus_config_t bus_cfg = {
            .mosi_io_num = SD_SPI_MOSI,
            .miso_io_num = SD_SPI_MISO,
            .sclk_io_num = SD_SPI_CLK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 4000,
        };
        ret = spi_bus_initialize((spi_host_device_t) host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
        if (ret != ESP_OK) {
            logger->logDebug(GwLog::ERROR, "Failed to initialize SPI bus for SD card");
        } else {
            sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
            slot_config.gpio_cs = SD_SPI_CS;
            slot_config.host_id = (spi_host_device_t) host.slot;
            esp_vfs_fat_sdmmc_mount_config_t mount_config = {
                .format_if_mount_failed = false,
                .max_files = 5,
                .allocation_unit_size = 16 * 1024
            };
            ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &sdcard);
            if (ret != ESP_OK) {
                if (ret == ESP_FAIL) {
                    logger->logDebug(GwLog::ERROR, "Failed to mount SD card filesystem");
                } else {
                    // ret == 263 could be not powered up yet
                    logger->logDebug(GwLog::ERROR, "Failed to initialize SD card (error #%d)", ret);
                }
            } else {
                logger->logDebug(GwLog::LOG, "SD card filesystem mounted at '%s'", MOUNT_POINT);
                hasSDCard = true;
            }
        }
        if (hasSDCard) {
            // read some stats
            String features = "";
            if (sdcard->is_mem) features += "MEM "; // Memory card
            if (sdcard->is_sdio) features += "IO "; // IO Card
            if (sdcard->is_mmc) features += "MMC "; // MMC Card
            if (sdcard->is_ddr) features += "DDR ";
            // if (sdcard->is_uhs1) features += "UHS-1 ";
            // ext_csd. Extended information
            // uint8_t rev, uint8_t power_class
            logger->logDebug(GwLog::LOG, "SD card features: %s", features);
            logger->logDebug(GwLog::LOG, "SD card size: %lluMB", ((uint64_t) sdcard->csd.capacity) * sdcard->csd.sector_size / (1024 * 1024));
        }
    }
#endif
}

void powerInit(String powermode) {
    // Max Power | Only 5.0V | Min Power
    if (powermode == "Max Power" || powermode == "Only 5.0V") {
#ifdef BOARD_OBP60S3
        setPortPin(OBP_POWER_50, true); // Power on 5.0V rail
#endif
#ifdef BOARD_OBP40S3
        setPortPin(OBP_POWER_EPD, true);// Power on ePaper display
        setPortPin(OBP_POWER_SD, true); // Power on SD card
#endif
    } else { // Min Power
#ifdef BOARD_OBP60S3
        setPortPin(OBP_POWER_50, false); // Power off 5.0V rail
#endif
#ifdef BOARD_OBP40S3
        setPortPin(OBP_POWER_EPD, false);// Power off ePaper display
        setPortPin(OBP_POWER_SD, false); // Power off SD card
#endif
    }
}

void setPCF8574PortPinModul1(uint8_t pin, uint8_t value)
{
  static bool firstRunFinished;
  static uint8_t port1;                      // Retained data for port bits 
  // If fisrt run then set all outputs to low
  if(firstRunFinished == false){
    port1 = 255;                             // Low active
    firstRunFinished = true;
  }
  if (pin > 7) return;
  Wire.setClock(I2C_SPEED_LOW);              // Set I2C clock on 10 kHz for longer wires
  // Set bit
  if (pcf8574_Modul1.begin(port1))           // Check module availability and start it
  {
    if (value == LOW)  port1 &= ~(1 << pin); // Set bit
    else               port1 |=  (1 << pin);
    pcf8574_Modul1.write8(port1);            // Write byte
  }
  Wire.setClock(I2C_SPEED);                  // Set I2C clock on 100 kHz
}


void setPortPin(uint pin, bool value){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
}

void togglePortPin(uint pin){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, !digitalRead(pin));
}

void startLedTask(GwApi *api){
    ledTaskData=new LedTaskData(api);
    createSpiLedTask(ledTaskData);
}

uint8_t getLastPage() {
    return RTC_lastpage;
}

#ifdef BOARD_OBP60S3
void deepSleep(CommonData &common){
    RTC_lastpage = common.data.actpage - 1;
    // Switch off all power lines
    setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
    setFlashLED(false);                     // Flash LED Off
    buzzer(TONE4, 20);                      // Buzzer tone 4kHz 20ms
    // Shutdown EInk display
    getdisplay().setFullWindow();               // Set full Refresh
    getdisplay().fillScreen(common.bgcolor);    // Clear screen
    getdisplay().setTextColor(common.fgcolor);
    getdisplay().setFont(&Ubuntu_Bold20pt8b);
    getdisplay().setCursor(85, 150);
    getdisplay().print("Sleep Mode");
    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(65, 175);
    getdisplay().print("To wake up press key and wait 5s");
    displayNextPage();                // Update display contents
    #ifdef TFT_DISPLAY
    getpaneldisplay().powerSave(true);      // Display power save
    #else
    getdisplay().powerOff();                // Display power off
    #endif
    setPortPin(OBP_POWER_50, false);        // Power off ePaper display
    // Stop system
    esp_deep_sleep_start();                 // Deep Sleep with weakup via touch pin
}
#endif
#ifdef BOARD_OBP40S3
// Deep sleep funktion
void deepSleep(CommonData &common){
    RTC_lastpage = common.data.actpage - 1;
    // Switch off all power lines
    setPortPin(OBP_BACKLIGHT_LED, false);   // Backlight Off
    setFlashLED(false);                     // Flash LED Off
    // Shutdown EInk display
    getdisplay().setFullWindow();               // Set full Refresh
    //getdisplay().setPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
    getdisplay().fillScreen(common.bgcolor);    // Clear screen
    getdisplay().setTextColor(common.fgcolor);
    getdisplay().setFont(&Ubuntu_Bold20pt8b);
    getdisplay().setCursor(85, 150);
    getdisplay().print("Sleep Mode");
    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    getdisplay().setCursor(65, 175);
    getdisplay().print("To wake up press wheel and wait 5s");
    displayNextPage();                // Partial update
    #ifdef TFT_DISPLAY
    getpaneldisplay().powerSave(true);      // Display power save
    #else
    getdisplay().powerOff();                // Display power off
    #endif
    setPortPin(OBP_POWER_EPD, false);       // Power off ePaper display
    setPortPin(OBP_POWER_SD, false);        // Power off SD card
    // Stop system
    esp_deep_sleep_start();             // Deep Sleep with weakup via GPIO pin
}
#endif

// Valid colors see hue
Color colorMapping(const String &colorString){
    Color color = COLOR_RED;
    if(colorString == "Orange"){color = Color(255,153,0);}
    if(colorString == "Yellow"){color = Color(255,255,0);}
    if(colorString == "Green"){color = COLOR_GREEN;}
    if(colorString == "Blue"){color = COLOR_BLUE;}
    if(colorString == "Aqua"){color = Color(51,102,255);}
    if(colorString == "Violet"){color = Color(255,0,102);}
    if(colorString == "White"){color = COLOR_WHITE;}
    return color;
}

BacklightMode backlightMapping(const String &backlightString) {
    static std::map<String, BacklightMode> const table = {
        {"Off", BacklightMode::OFF},
        {"Control by Bus", BacklightMode::BUS},
        {"Control by Time", BacklightMode::TIME},
        {"Control by Key", BacklightMode::KEY},
        {"On", BacklightMode::ON},
    };
    auto it = table.find(backlightString);
    if (it != table.end()) {
        return it->second;
    }
    return BacklightMode::OFF;
}

// All defined colors see pixeltypes.h in FastLED lib
void setBacklightLED(uint brightness, const Color &color){
    if (ledTaskData == nullptr) return;
    Color nv=setBrightness(color,brightness);
    LedInterface current=ledTaskData->getLedData();
    current.setBacklight(nv);
    ledTaskData->setLedData(current);    
}

void toggleBacklightLED(uint brightness, const Color &color){
    if (ledTaskData == nullptr) return;
    statusBacklightLED = !statusBacklightLED;
    Color nv=setBrightness(statusBacklightLED?color:COLOR_BLACK,brightness);
    LedInterface current=ledTaskData->getLedData();
    current.setBacklight(nv);
    ledTaskData->setLedData(current); 
}

void stepsBacklightLED(uint brightness, const Color &color){
    static uint step = 0;
    uint actBrightness = 0;
    // Different brightness steps
    if(step == 0){
        actBrightness = brightness;         // 100% from brightess
        statusBacklightLED = true;
    }
    if(step == 1){
        actBrightness = brightness * 0.5;   // 50% from brightess
        statusBacklightLED = true;
    }
    if(step == 2){
        actBrightness = brightness * 0.2;   // 20% from brightess
        statusBacklightLED = true;
    }
    if(step == 3){
        actBrightness = 0;                  // 0%
        statusBacklightLED = false;
    }
    if(actBrightness < 5){                  // Limiter if values too low
        actBrightness = 5;
    }
    step = step + 1;    // Increment step counter
    if(step == 4){      // Reset counter
        step = 0;
    }
    if (ledTaskData == nullptr) return;
    Color nv=setBrightness(statusBacklightLED?color:COLOR_BLACK,actBrightness);
    LedInterface current=ledTaskData->getLedData();
    current.setBacklight(nv);
    ledTaskData->setLedData(current); 
}

void setFlashLED(bool status){
    if (ledTaskData == nullptr) return;
    Color c=status?COLOR_RED:COLOR_BLACK;
    LedInterface current=ledTaskData->getLedData();
    current.setFlash(c);
    ledTaskData->setLedData(current);
}

void blinkingFlashLED(){
    if(blinkingLED == true){
        statusLED = !statusLED;     // Toggle LED for each run
        setFlashLED(statusLED);
    }    
}

void setBlinkingLED(bool status){
    blinkingLED = status;
}

uint buzzerpower = 50;
static QueueHandle_t buzzerQueue;
struct BuzzerRequest {
    uint frequency;
    uint duration;
};

// buzzer function in separate task to avoid blocking of main task
static void buzzerTaskFunction(void* pvParameters) {
    // Using LED PWM function for sound generation
    pinMode(OBP_BUZZER, OUTPUT);
    ledcSetup(0, 2000, 8);        // channel/resolution fixed once; frequency reset per request below
    ledcAttachPin(OBP_BUZZER, 0);

    BuzzerRequest req;
    while (true) {
        if (xQueueReceive(buzzerQueue, &req, portMAX_DELAY) == pdTRUE) {
            uint frequency = min(req.frequency, uint(8000)); // Max 8000Hz
            uint duration  = min(req.duration, uint(1000));  // Max 1000ms
            uint power = min(buzzerpower, uint(100));        // Max 100%

            ledcSetup(0, frequency, 8);                 // Ch 0, ferquency in Hz, 8 Bit resolution of PWM
            ledcWrite(0, uint(power * 1.28));           // 50% duty cycle are 100%
            vTaskDelay(pdMS_TO_TICKS(duration));
            ledcWrite(0, 0);                            // 0% duty cycle are 0%
        }
        delay(20); // interrupt two subsequent buzzer calls
    }
}

// Create buzzer task; call once at setup
// xTask requires ~1.700 bytes of memory
void createBuzzerTask() {
#if defined BOARD_OBP60S3
    buzzerQueue = xQueueCreate(4, sizeof(BuzzerRequest));
    xTaskCreate(buzzerTaskFunction, "BuzzerTask", 1288, nullptr, 4, nullptr); // run buzzer task with slightly higher priority for constant beeps
#endif
}

// Ring buzzer
void buzzer(uint frequency, uint duration) {
#if defined BOARD_OBP60S3
    BuzzerRequest req{ frequency, duration };
    if (buzzerQueue) {
        xQueueSend(buzzerQueue, &req, 0);   // 0 = don't wait if queue's full, just drop the request
    }
#endif
}

void setBuzzerPower(uint power){
    buzzerpower = min(power, uint(100));
}

// Delete xdr prefix from string
String xdrDelete(String input){
    if(input.substring(0,3) == "xdr"){
        input = input.substring(3, input.length());
    }
    return input;
}

void fillPoly4(const std::vector<Point>& p4, uint16_t color) {
    getdisplay().fillTriangle(p4[0].x, p4[0].y, p4[1].x, p4[1].y, p4[2].x, p4[2].y, color);
    getdisplay().fillTriangle(p4[0].x, p4[0].y, p4[2].x, p4[2].y, p4[3].x, p4[3].y, color);
}

void drawPoly(const std::vector<Point>& points, uint16_t color) {
    size_t polysize = points.size();
    for (size_t i = 0; i < polysize - 1; i++) {
        getdisplay().drawLine(points[i].x, points[i].y, points[i+1].x, points[i+1].y, color);
    }
    // close path
    getdisplay().drawLine(points[polysize-1].x, points[polysize-1].y, points[0].x, points[0].y, color);
}

// Split string into words, whitespace separated
std::vector<String> split(const String &s) {
    std::vector<String> words;
    String word = "";
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')  {
            if (word.length() > 0) {
                words.push_back(word);
                word = "";
            }
        } else {
             word += s[i];
        }
    }
    if (word.length() > 0) {
        words.push_back(word);
    }
    return words;
}

// Wordwrap single line, monospaced font
std::vector<String> wordwrap(String &line, uint16_t maxwidth) {
    std::vector<String> lines;
    std::vector<String> words = split(line);
    String currentLine = "";
    for (const auto& word : words) {
        if (currentLine.length() + word.length() + 1 > maxwidth) {
             if (currentLine.length() > 0) {
                  lines.push_back(currentLine);
                  currentLine = "";
             }
        }
        if (currentLine.length() > 0) {
            currentLine += " ";
        }
        currentLine += word;
    }
    if (currentLine.length() > 0) {
        lines.push_back(currentLine);
    }
    return lines;
}

// Draw centered text
//   bool dsegAdjust = false  - optional, for adjustment of DSEG italic font
// returns left <x> edge of printed string
int16_t drawTextCenter(int16_t cx, int16_t cy, String text, bool dsegAdjust) {
    int16_t x1, y1;
    uint16_t w, h;
    displayGetTextBounds(text, 0, 0, &x1, &y1, &w, &h, dsegAdjust);
    int16_t cursorX = cx - (x1 + static_cast<int16_t>(w / 2));
    int16_t cursorY = cy - (y1 + static_cast<int16_t>(h / 2));
    getdisplay().setCursor(cursorX, cursorY);
    getdisplay().print(text);

    return cursorX;
}

// Draw centered button with centered text
void drawButtonCenter(int16_t cx, int16_t cy, int8_t sx, int8_t sy, String text, uint16_t fg, uint16_t bg, bool inverted) {
    int16_t x1, y1;
    uint16_t w, h;
    displayGetTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t cursorX = cx - (x1 + static_cast<int16_t>(w / 2));
    int16_t cursorY = cy - (y1 + static_cast<int16_t>(h / 2));
    //getdisplay().drawPixel(cx, cy, fg);                         // Debug pixel for center position
    if (inverted) {
        getdisplay().fillRoundRect(cx - sx / 2, cy - sy / 2, sx, sy, 5, fg); // Draw button
        getdisplay().setTextColor(bg);
        getdisplay().setCursor(cursorX, cursorY);                 // Set cursor to center
        getdisplay().print(text);                               // Draw text
     }
     else{
        getdisplay().drawRoundRect(cx - sx / 2, cy - sy / 2, sx, sy, 5, fg); // Draw button
        getdisplay().setTextColor(fg);
        getdisplay().setCursor(cursorX, cursorY);                 // Set cursor to center
        getdisplay().print(text);                               // Draw text
     }
}

// Draw right aligned text
//   int16_t x                - upper right x position for text to start printing
//   int16_t y                - upper right y position for text to start printing
//   const String& text       - text to be printed
//   bool dsegAdjust = false  - optional, for adjustment of DSEG italic font
// returns left <x> edge of printed string
int16_t drawTextRalign(int16_t x, int16_t y,  const String& text, bool dsegAdjust) {
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;

    displayGetTextBounds(text, 0, y, &x1, &y1, &w, &h, dsegAdjust);
    int16_t cursorX = x - x1 - w;
    getdisplay().setCursor(cursorX, y);
    getdisplay().print(text);

    return cursorX;  // currently visible left edge
}

// Implementation of <printBoatValue()>; does the actual printing
static void printBoatValueImpl(const String &sBoatValue, const int16_t x, const int16_t y,
                                const int8_t align, const int8_t fontSize, bool smallDecs) {
    const GFXfont *mainFont = nullptr;
    const GFXfont *decimalFont = nullptr;
    constexpr bool dsegAdjust = true;

    String integerPart = "";
    String decimalPart = "";
    int16_t dot = 0;

    uint16_t intW = 0, decW = 0;
    int16_t decimalLeftX = 0;

    // -----------------------------------------------------------------
    // Font selection
    // -----------------------------------------------------------------
    if (sBoatValue.indexOf('"') != -1)
    {
        // LAT/LON values
        if (fontSize <= 12)
            mainFont = &Ubuntu_Bold12pt8b;
        else if (fontSize <= 16)
            mainFont = &Ubuntu_Bold16pt8b;
        else if (fontSize <= 20)
            mainFont = &Ubuntu_Bold20pt8b;
        else
            mainFont = &Ubuntu_Bold32pt8b;

        smallDecs = false; // disable small decimals for LAT/LON in any case
    }
    else if (fontSize <= 12) {
        mainFont = &DSEG7Classic_BoldItalic12pt7b;
        decimalFont = &DSEG7Classic_BoldItalic10pt7b;
    } else if (fontSize <= 16) {
        mainFont = &DSEG7Classic_BoldItalic16pt7b;
        decimalFont = &DSEG7Classic_BoldItalic12pt7b;
    } else if (fontSize <= 20) {
        mainFont = &DSEG7Classic_BoldItalic20pt7b;
        decimalFont = &DSEG7Classic_BoldItalic16pt7b;
    } else if (fontSize <= 26) {
        mainFont = &DSEG7Classic_BoldItalic26pt7b;
        decimalFont = &DSEG7Classic_BoldItalic20pt7b;
    } else if (fontSize <= 30) {
        mainFont = &DSEG7Classic_BoldItalic30pt7b;
        decimalFont = &DSEG7Classic_BoldItalic26pt7b;
    } else if (fontSize <= 42) {
        mainFont = &DSEG7Classic_BoldItalic42pt7b;
        decimalFont = &DSEG7Classic_BoldItalic30pt7b;
    } else {
        mainFont = &DSEG7Classic_BoldItalic60pt7b;
        decimalFont = &DSEG7Classic_BoldItalic42pt7b;
    }

    // separate integer and decimal part of the value
    dot = sBoatValue.indexOf('.');
    if (dot == -1) { // no decimal point found
        integerPart = sBoatValue;
        decimalPart = "";
        smallDecs = false; // print only integer part
    } else {
        integerPart = sBoatValue.substring(0, dot + 1); // keep decimal point
        decimalPart = sBoatValue.substring(dot + 1);
    }

    // Measure integer part
    int16_t ix1, iy1;
    uint16_t iw, ih;
    display.setFont(mainFont);
    displayGetTextBounds(integerPart, 0, 0, &ix1, &iy1, &iw, &ih, dsegAdjust);
    intW = ix1 + iw;

    // Measure decimal part only if one actually exists
    decW = 0;
    if (dot != -1 && decimalPart.length() > 0) {
        int16_t dx1, dy1;
        uint16_t dw, dh;
        display.setFont(smallDecs ? decimalFont : mainFont);
        displayGetTextBounds(decimalPart, 0, 0, &dx1, &dy1, &dw, &dh, dsegAdjust);
        decW = dx1 + dw;
    }

    const int16_t totalWidth = intW + decW;
    int16_t startX;
    switch (align) {
        case 0: // LEFT
            startX = x;
            break;
        case 1: // CENTER
            startX = x - totalWidth / 2;
            break;
        case 2: // RIGHT
        default:
            startX = x - totalWidth;
            break;
    }
    decimalLeftX = startX + intW;

    // Draw integer part
    display.setFont(mainFont);
    getdisplay().setCursor(startX, y);
    getdisplay().print(integerPart);

    // Draw decimal part only when present
    if (dot != -1 && decimalPart.length() > 0) {
        display.setFont(smallDecs ? decimalFont : mainFont);
        getdisplay().setCursor(decimalLeftX, y);
        getdisplay().print(decimalPart);
    }
}

// Print boat data value at x,y position with selectable alignment
// Version with <GwApi::BoatValue *bValue>; formats the boat data value and then prints the formatted value
//   GwApi::BoatValue *bValue       - boat value to print
//   CommonData &commondata         - <commondata> required for <formatValue()> function
//   int16_t x                      - upper left/right x position for text to start printing
//   int16_t y                      - upper left/right y position for text to start printing
//   int8_t align                   - alignment [0,1,2] of the value: 0 = left, 1 = center, 2 = right
//   int8_t fontSize                - font size for integer part of number or for full value, if <smallDecs> is <false>/not set
//   bool smallDecs                 - flag specifies if decimals will be printed in smaller fontsize [default = false]
void printBoatValue(GwApi::BoatValue *bValue, CommonData &commondata, const int16_t x, const int16_t y,
                     const int8_t align, const int8_t fontSize, const bool smallDecs)
{
    String sBoatValue = formatValue(bValue, commondata).svalue;
    printBoatValueImpl(sBoatValue, x, y, align, fontSize, smallDecs);
}

// Print boat data value at x,y position with selectable alignment; oerload with ready-to-print <String>; omits boat data value formatting
void printBoatValue(const String &sBoatValue, const int16_t x, const int16_t y,
                     const int8_t align, const int8_t fontSize, const bool smallDecs)
{
    printBoatValueImpl(sBoatValue, x, y, align, fontSize, smallDecs);
}

// Draw text inside box, normal or inverted
void drawTextBoxed(Rect box, String text, uint16_t fg, uint16_t bg, bool inverted, bool border) {
    if (inverted) {
        getdisplay().fillRect(box.x, box.y, box.w, box.h, fg);
        getdisplay().setTextColor(bg);
    } else {
        if (border) {
            getdisplay().fillRect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, bg);
            getdisplay().drawRect(box.x, box.y, box.w, box.h, fg);
        }
        getdisplay().setTextColor(fg);
    }
    uint16_t border_offset = box.h / 4; // 25% of box height
    getdisplay().setCursor(box.x + border_offset, box.y + box.h - border_offset);
    getdisplay().print(text);
    getdisplay().setTextColor(fg);
}

// Show a triangle for trend direction high (x, y is the left edge)
void displayTrendHigh(int16_t x, int16_t y, uint16_t size, uint16_t color){
    getdisplay().fillTriangle(x, y, x+size*2, y, x+size, y-size*2, color);
}

// Show a triangle for trend direction low (x, y is the left edge)
void displayTrendLow(int16_t x, int16_t y, uint16_t size, uint16_t color){
    getdisplay().fillTriangle(x, y, x+size*2, y, x+size, y+size*2, color);
}

// Show header informations
void displayHeader(CommonData &commonData, GwApi::BoatValue *date, GwApi::BoatValue *time, GwApi::BoatValue *hdop){

    static bool heartbeat = false;
    static unsigned long usbRxOld = 0;
    static unsigned long usbTxOld = 0;
    static unsigned long serRxOld = 0;
    static unsigned long serTxOld = 0;
    static unsigned long tcpSerRxOld = 0;
    static unsigned long tcpSerTxOld = 0;
    static unsigned long tcpClRxOld = 0;
    static unsigned long tcpClTxOld = 0;
    static unsigned long n2kRxOld = 0;
    static unsigned long n2kTxOld = 0;

    if(commonData.config->getBool(commonData.config->statusLine) == true){

        // Show status info
        getdisplay().setTextColor(commonData.fgcolor);
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(0, 15);
        if(commonData.status.wifiApOn){
        getdisplay().print(" AP ");
        }
        // If receive new telegram data then display bus name
        if(commonData.status.tcpClRx != tcpClRxOld || commonData.status.tcpClTx != tcpClTxOld || commonData.status.tcpSerRx != tcpSerRxOld || commonData.status.tcpSerTx != tcpSerTxOld){
        getdisplay().print("TCP ");
        }
        if(commonData.status.n2kRx != n2kRxOld || commonData.status.n2kTx != n2kTxOld){
        getdisplay().print("N2K ");
        }
        if(commonData.status.serRx != serRxOld || commonData.status.serTx != serTxOld){
        getdisplay().print("183 ");
        }
        if(commonData.status.usbRx != usbRxOld || commonData.status.usbTx != usbTxOld){
        getdisplay().print("USB ");
        }
        double gpshdop = formatValue(hdop, commonData).value;
        if(commonData.config->getString(commonData.config->useGPS) != "off" &&  gpshdop <= commonData.config->getInt(commonData.config->hdopAccuracy) && hdop->valid == true){
        getdisplay().print("GPS");
        }
        // Save old telegram counter
        tcpClRxOld = commonData.status.tcpClRx;
        tcpClTxOld = commonData.status.tcpClTx;
        tcpSerRxOld = commonData.status.tcpSerRx;
        tcpSerTxOld = commonData.status.tcpSerTx;
        n2kRxOld = commonData.status.n2kRx;
        n2kTxOld = commonData.status.n2kTx;
        serRxOld = commonData.status.serRx;
        serTxOld = commonData.status.serTx;
        usbRxOld = commonData.status.usbRx;
        usbTxOld = commonData.status.usbTx;

#ifdef BOARD_OBP60S3
        // Display key lock status
        if (commonData.keylock) {
            getdisplay().drawXBitmap(170, 1, lock_bits, icon_width, icon_height, commonData.fgcolor);
        } else {
            getdisplay().drawXBitmap(166, 1, swipe_bits, swipe_width, swipe_height, commonData.fgcolor);
        }
#endif
#ifdef LIPO_ACCU_1200
        if (commonData.data.BatteryChargeStatus == 1) {
             getdisplay().drawXBitmap(170, 1, battery_loading_bits, battery_width, battery_height, commonData.fgcolor);
        } else {
#ifdef VOLTAGE_SENSOR
            if (commonData.data.batteryLevelLiPo < 10) {
                getdisplay().drawXBitmap(170, 1, battery_0_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 25) {
                getdisplay().drawXBitmap(170, 1, battery_25_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 50) {
                getdisplay().drawXBitmap(170, 1, battery_50_bits, battery_width, battery_height, commonData.fgcolor);
            } else if (commonData.data.batteryLevelLiPo < 75) {
                getdisplay().drawXBitmap(170, 1, battery_75_bits, battery_width, battery_height, commonData.fgcolor);
            } else {
                getdisplay().drawXBitmap(170, 1, battery_100_bits, battery_width, battery_height, commonData.fgcolor);
            }
#endif // VOLTAGE_SENSOR
        }
#endif // LIPO_ACCU_1200

        // Heartbeat as page number
        if (heartbeat) {
            getdisplay().setTextColor(commonData.bgcolor);
            getdisplay().fillRect(201, 0, 23, 19, commonData.fgcolor);
        } else {
            getdisplay().setTextColor(commonData.fgcolor);
            getdisplay().drawRect(201, 0, 23, 19, commonData.fgcolor);
        }
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        drawTextCenter(211, 9, String(commonData.data.actpage));
        heartbeat = !heartbeat;

        // Date and time
        String fmttype = commonData.config->getString(commonData.config->dateFormat);
        String timesource = commonData.config->getString(commonData.config->timeSource);
        double tz = commonData.config->getString(commonData.config->timeZone).toDouble();
        getdisplay().setTextColor(commonData.fgcolor);
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(230, 15);
        if (timesource == "RTC" or timesource == "iRTC") {
            // TODO take DST into account
            if (commonData.data.rtcValid) {
                time_t tv = mktime(&commonData.data.rtcTime) + (int)(tz * 3600);
                struct tm *local_tm = localtime(&tv);
                getdisplay().print(formatTime('m', local_tm->tm_hour, local_tm->tm_min, 0));
                getdisplay().print(" ");
                getdisplay().print(formatDate(fmttype, local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday));
                getdisplay().print(" ");
                getdisplay().print(tz == 0 ? "UTC" : "LOT");
            } else {
                drawTextRalign(396, 15, "RTC invalid");
            }
        }
        else if (timesource == "GPS") {
            // Show date and time if date present
            if(date->valid == true){
                String acttime = formatValue(time, commonData).svalue;
                acttime = acttime.substring(0, 5);
                String actdate = formatValue(date, commonData).svalue;
                getdisplay().print(acttime);
                getdisplay().print(" ");
                getdisplay().print(actdate);
                getdisplay().print(" ");
                getdisplay().print(tz == 0 ? "UTC" : "LOT");
            }
            else{
                if(commonData.config->getBool(commonData.config->useSimuData) == true){
                    getdisplay().print("12:00 01.01.2024 LOT");
                }
                else{
                    drawTextRalign(396, 15, "No GPS data");
                }
            }
        } // timesource == "GPS"
        else {
            getdisplay().print("No time source");
        }
    }
}

void displayFooter(CommonData &commonData) {

    getdisplay().setFont(&Atari16px);
    getdisplay().setTextColor(commonData.fgcolor);

#ifdef BOARD_OBP60S3
    // Frame around key icon area
    if (! commonData.keylock) {
        // horizontal elements
        const uint16_t top = 280;
        const uint16_t bottom = 299;
        // horizontal stub lines
        getdisplay().drawLine(commonData.keydata[0].x, top, commonData.keydata[0].x+10, top, commonData.fgcolor);
        for (int i = 1; i <= 5; i++) {
            getdisplay().drawLine(commonData.keydata[i].x-10, top, commonData.keydata[i].x+10, top, commonData.fgcolor);
        }
        getdisplay().drawLine(commonData.keydata[5].x + commonData.keydata[5].w - 10, top, commonData.keydata[5].x + commonData.keydata[5].w + 1, top, commonData.fgcolor);
        // vertical key separators
        for (int i = 0; i <= 4; i++) {
            getdisplay().drawLine(commonData.keydata[i].x + commonData.keydata[i].w, top, commonData.keydata[i].x + commonData.keydata[i].w, bottom, commonData.fgcolor);
        }
        for (int i = 0; i < 6; i++) {
            uint16_t x, y;
            if (commonData.keydata[i].label.length() > 0) {
                // check if icon is enabled
                String icon_name = commonData.keydata[i].label.substring(1);
                if (commonData.keydata[i].label[0] == '#') {
                    if (iconmap.find(icon_name) != iconmap.end()) {
                        x = commonData.keydata[i].x + (commonData.keydata[i].w - icon_width) / 2;
                        y = commonData.keydata[i].y + (commonData.keydata[i].h - icon_height) / 2;
                        getdisplay().drawXBitmap(x, y, iconmap[icon_name], icon_width, icon_height, commonData.fgcolor);
                    } else {
                        // icon is missing, use name instead
                        x = commonData.keydata[i].x + commonData.keydata[i].w / 2;
                        y = commonData.keydata[i].y + commonData.keydata[i].h / 2;
                        drawTextCenter(x, y, icon_name);
                    }
                } else {
                    x = commonData.keydata[i].x + commonData.keydata[i].w / 2;
                    y = commonData.keydata[i].y + commonData.keydata[i].h / 2;
                    drawTextCenter(x, y, commonData.keydata[i].label);
                }
            }
        }
    } else {
        getdisplay().setCursor(65, 295);
        getdisplay().print("Press 1 and 6 fast to unlock keys");
    }
#endif
#ifdef BOARD_OBP40S3
    // grapical page indicator
    static const uint16_t r = 5;
    static const uint16_t space = 4;
    uint16_t w = commonData.data.maxpage * r * 2 + (commonData.data.maxpage - 1) * space;
    uint16_t x0 = (GxEPD_WIDTH - w) / 2 + r * 2;
    for (int i = 0; i < commonData.data.maxpage; i++) {
        if (i == (commonData.data.actpage - 1)) {
            getdisplay().fillCircle(x0 + i * (r * 2 + space), 290, r, commonData.fgcolor);
        } else {
            getdisplay().drawCircle(x0 + i * (r * 2 + space), 290, r, commonData.fgcolor);
        }
    }
    // key indicators
    // left side = top key "menu"
    getdisplay().drawLine(0, 280, 10, 280, commonData.fgcolor);
    getdisplay().drawLine(55, 280, 65, 280, commonData.fgcolor);
    getdisplay().drawLine(65, 280, 65, 299, commonData.fgcolor);
    drawTextCenter(33, 291, commonData.keydata[0].label);
    // right side = bottom key "exit"
    getdisplay().drawLine(390, 280, 399, 280, commonData.fgcolor);
    getdisplay().drawLine(335, 280, 345, 280, commonData.fgcolor);
    getdisplay().drawLine(335, 280, 335, 399, commonData.fgcolor);
    drawTextCenter(366, 291, commonData.keydata[1].label);
#endif
}

// Alarm overlay, to be drawn as very last draw operation
void displayAlarm(CommonData &commonData) {

    const uint16_t x = 50;   // overlay area
    const uint16_t y = 100;
    const uint16_t w = 300;
    const uint16_t h = 150;

    getdisplay().setFont(&Atari16px);
    getdisplay().setTextColor(commonData.fgcolor);

    // overlay
    getdisplay().drawRect(x, y, w, h, commonData.fgcolor);
    getdisplay().fillRect(x + 1, y + 1, w - 1, h - 1, commonData.bgcolor);
    getdisplay().drawRect(x + 3, y + 3, w - 5, h - 5, commonData.fgcolor);

    // exclamation icon in left top corner
    getdisplay().drawXBitmap(x + 16, y + 16, exclamation_bits, exclamation_width, exclamation_height, commonData.fgcolor);

    // title
    getdisplay().setCursor(x + 64, y + 30);
    getdisplay().print("A L A R M");
    getdisplay().setCursor(x + 64, y + 48);
    getdisplay().print("#" + commonData.alarm.id);
    getdisplay().print(" from ");
    getdisplay().print(commonData.alarm.source);

    // message, but maximum 4 lines
    std::vector<String> lines = wordwrap (commonData.alarm.message, w - 16 - 8 / 8);
    int n = 0;
    for (const auto& l : lines) {
        getdisplay().setCursor(x + 16, y + 80 + n);
        getdisplay().print(l);
        n += 16;
        if (n > 64) {
            break;
        }
    }
    drawTextCenter(x + w / 2,  y + h - 16, "Press button 1 to dismiss alarm");
}

// Sunset und sunrise calculation
SunData calcSunsetSunrise(double time, double date, double latitude, double longitude, float timezone){
    SunData returnset;
    SunRise sr;
    int secPerHour = 3600;
    int secPerYear = 86400;
    sr.hasRise = false;
    sr.hasSet = false;
    time_t t = 0;
    time_t sunR = 0;
    time_t sunS = 0;

    if (!isnan(time) && !isnan(date) && !isnan(latitude) && !isnan(longitude) && !isnan(timezone)) {
        
        // Calculate local epoch
        t = (date * secPerYear) + time;
        sr.calculate(latitude, longitude, t);       // LAT, LON, EPOCH
        // Sunrise
        if (sr.hasRise) {
            sunR = (sr.riseTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
            returnset.sunriseHour = int (sunR / secPerHour);
            returnset.sunriseMinute = int((sunR - returnset.sunriseHour * secPerHour)/60);
        }
        // Sunset
        if (sr.hasSet)  {
            sunS = (sr.setTime  + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
            returnset.sunsetHour = int (sunS / secPerHour);
            returnset.sunsetMinute = int((sunS - returnset.sunsetHour * secPerHour)/60);      
        }
        // Sun control (return value by sun on sky = false, sun down = true)
        if ((t >= sr.riseTime) && (t <= sr.setTime))      
             returnset.sunDown = false; 
        else returnset.sunDown = true;
    }
    // Return values
    return returnset;
}

SunData calcSunsetSunriseRTC(struct tm *rtctime, double latitude, double longitude, float timezone) {
    SunData returnset;
    SunRise sr;
    const int secPerHour = 3600;
    const int secPerYear = 86400;
    sr.hasRise = false;
    sr.hasSet = false;
    time_t t =  mktime(rtctime) + timezone * 3600;;
    time_t sunR = 0;
    time_t sunS = 0;

    sr.calculate(latitude, longitude, t);       // LAT, LON, EPOCH
    // Sunrise
    if (sr.hasRise) {
        sunR = (sr.riseTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
        returnset.sunriseHour = int (sunR / secPerHour);
        returnset.sunriseMinute = int((sunR - returnset.sunriseHour * secPerHour) / 60);
    }
    // Sunset
    if (sr.hasSet) {
        sunS = (sr.setTime + int(timezone * secPerHour) + 30) % secPerYear; // add 30 seconds: round to minutes
        returnset.sunsetHour = int (sunS / secPerHour);
        returnset.sunsetMinute = int((sunS - returnset.sunsetHour * secPerHour) / 60);
    }
    // Sun control (return value by sun on sky = false, sun down = true)
    if ((t >= sr.riseTime) && (t <= sr.setTime))
         returnset.sunDown = false;
    else returnset.sunDown = true;
    return returnset;
}

// Battery graphic with fill level
void batteryGraphic(uint x, uint y, float percent, int pcolor, int bcolor){
    // Show battery
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness
        // Percent limits
        if(percent < 0){
            percent = 0;
        }
         if(percent > 99){
            percent = 99;
        }
        // Battery corpus 100x80 with fill level
        int level = int((100.0 - percent) * (80-(2*t)) / 100.0);
        getdisplay().fillRect(xb, yb, 100, 80, pcolor);
        if(percent < 99){
            getdisplay().fillRect(xb+t, yb+t, 100-(2*t), level, bcolor);
        }
        // Plus pol 20x15
        int xp = xb + 20;
        int yp = yb - 15 + t;
        getdisplay().fillRect(xp, yp, 20, 15, pcolor);
        getdisplay().fillRect(xp+t, yp+t, 20-(2*t), 15-(2*t), bcolor);
        // Minus pol 20x15
        int xm = xb + 60;
        int ym = yb -15 + t;
        getdisplay().fillRect(xm, ym, 20, 15, pcolor);
        getdisplay().fillRect(xm+t, ym+t, 20-(2*t), 15-(2*t), bcolor);
}

// Solar graphic with fill level
void solarGraphic(uint x, uint y, int pcolor, int bcolor){
    // Show solar modul
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness
        int percent = 0;
        // Solar corpus 100x80
        int level = int((100.0 - percent) * (80-(2*t)) / 100.0);
        getdisplay().fillRect(xb, yb, 100, 80, pcolor);
        if(percent < 99){
            getdisplay().fillRect(xb+t, yb+t, 100-(2*t), level, bcolor);
        }
        // Draw horizontel lines
        getdisplay().fillRect(xb, yb+28-t, 100, t, pcolor);
        getdisplay().fillRect(xb, yb+54-t, 100, t, pcolor);
        // Draw vertical lines
        getdisplay().fillRect(xb+19+t, yb, t, 80, pcolor);
        getdisplay().fillRect(xb+39+2*t, yb, t, 80, pcolor);
        getdisplay().fillRect(xb+59+3*t, yb, t, 80, pcolor);

}

// Generator graphic
void generatorGraphic(uint x, uint y, int pcolor, int bcolor){
    // Show battery
        int xb = x;     // X position
        int yb = y;     // Y position
        int t = 4;      // Line thickness

        // Generator corpus with radius 45
        getdisplay().fillCircle(xb, yb, 45, pcolor);
        getdisplay().fillCircle(xb, yb, 41, bcolor);
        // Insert G
        getdisplay().setTextColor(pcolor);
        getdisplay().setFont(&Ubuntu_Bold32pt8b);
        getdisplay().setCursor(xb-22, yb+20);
        getdisplay().print("G");
}

// Display rudder position as horizontal bargraph with configurable +/- range (degrees)
void displayRudderPosition(int rudderPosition, uint8_t rangeDeg, uint16_t cx, uint16_t cy, uint16_t fg, uint16_t bg){
    const int w = 360;
    const int h = 20;
    const int t = 3; // Line thickness
    const int halfw = w/2;
    const int halfh = h/2;
    // Calculate top-left of bar (cx,cy are center of 0°)
    int left = int(cx) - halfw;
    int top = int(cy) - halfh;

    // clamp provided range to allowed bounds [10,45]
    if (rangeDeg < 10) rangeDeg = 10;
    if (rangeDeg > 45) rangeDeg = 45;

    // Pixels per degree for +/-rangeDeg -> total span = 2*rangeDeg
    const float pxPerDeg = float(w) / (2.0f * float(rangeDeg));

    // Draw outer border (thickness t)
    for (int i = 0; i < t; i++) {
        getdisplay().drawRect(left + i, top + i, w - 2 * i, h - 2 * i, fg);
    }

    // Fill inner area with background
    getdisplay().fillRect(left + t, top + t, w - 2 * t, h - 2 * t, bg);

    // Draw center line
    getdisplay().drawRect(cx - 1, top + 1, 3 , h - 2, fg);

    // Clamp rudder position to -rangeDeg..rangeDeg
    if (rudderPosition > (int)rangeDeg) rudderPosition = (int)rangeDeg;
    if (rudderPosition < -((int)rangeDeg)) rudderPosition = -((int)rangeDeg);

    // Compute fill width in pixels
    int fillPx = int(round(rudderPosition * pxPerDeg)); // positive -> right

    // Fill area from center to position (if non-zero)
    int centerx = cx;
    int innerTop = top + t;
    int innerH = h - 2 * t;
    if (fillPx > 0) {
        // Right side
        getdisplay().fillRect(centerx, innerTop, fillPx, innerH, fg);
    } else if (fillPx < 0) {
        // Left side
        getdisplay().fillRect(centerx + fillPx, innerTop, -fillPx, innerH, fg);
    }

    
    // Draw tick marks every 5° and labels outside the bar
    getdisplay().setTextColor(fg);
    getdisplay().setFont(&Ubuntu_Bold8pt8b);
    for (int angle = -((int)rangeDeg); angle <= (int)rangeDeg; angle += 5) {
        int xpos = int(round(centerx + angle * pxPerDeg));
        // Vertical tick inside bar
        getdisplay().drawLine(xpos, top, xpos, top + h + 2, fg);
        // Label outside: below the bar
        String lbl = String(angle);
        int16_t bx, by;
        uint16_t bw, bh;
        #ifdef TFT_DISPLAY
            // LovyanGFX: compute width/height manually
            bw = getdisplay().textWidth(lbl);
            bh = getdisplay().fontHeight();
            bx = 0; by = 0;
        #else
            getdisplay().getTextBounds(lbl, 0, 0, &bx, &by, &bw, &bh);
        #endif
        int16_t tx = xpos - bw/2;
        int16_t ty = top + h + bh + 5; // A little spacing
        getdisplay().setCursor(tx, ty);
        getdisplay().print(lbl);
    }
}

// Function to handle HTTP image request
// http://192.168.15.1/api/user/OBP60Task/screenshot
void doImageRequest(GwApi *api, int *pageno, const PageStruct pages[MAX_PAGE_NUMBER], AsyncWebServerRequest *request) {
    GwLog *logger = api->getLogger();

    String imgformat = api->getConfig()->getConfigItem(api->getConfig()->imageFormat,true)->asString();
    imgformat.toLowerCase();
    String filename = "Page" + String(*pageno) + "_" +  pages[*pageno].description->pageName + "." + imgformat;

    logger->logDebug(GwLog::LOG,"handle image request [%s]: %s", imgformat, filename);

    uint8_t *fb = nullptr; // EPD framebuffer
    std::vector<uint8_t> imageBuffer;       // image in webserver transferbuffer
    String mimetype;
    #ifndef TFT_DISPLAY
        fb = getdisplay().getBuffer(); // available only for EPD
    #endif
    if (!fb) {
        request->send(500, "text/plain", "screenshot not available");
        return;
    }

    if (imgformat == "gif") {
        // GIF is commpressed with LZW, so small
        mimetype = "image/gif";
        if (!createGIF(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT)) {
             logger->logDebug(GwLog::LOG,"GIF creation failed: Hashtable init error!");
             return;
        }
    }
    else if (imgformat == "bmp") {
        // Microsoft BMP bitmap
        mimetype = "image/bmp";
        createBMP(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT);
    }
    else {
        // PBM simple portable bitmap
        mimetype = "image/x-portable-bitmap";
        createPBM(fb, &imageBuffer, GxEPD_WIDTH, GxEPD_HEIGHT);
    }

    AsyncWebServerResponse *response = request->beginResponse_P(200, mimetype, (const uint8_t*)imageBuffer.data(), imageBuffer.size());
    response->addHeader("Content-Disposition", "inline; filename=" + filename);
    request->send(response);

    imageBuffer.clear();
}



// Calculate the distance between two Geo coordinates
double distanceBetweenCoordinates(double lat1, double lon1, double lat2, double lon2) {
    // Grad → Radiant
    double lat1Rad = lat1 * DEG_TO_RAD;
    double lon1Rad = lon1 * DEG_TO_RAD;
    double lat2Rad = lat2 * DEG_TO_RAD;
    double lon2Rad = lon2 * DEG_TO_RAD;

    // Differenzen
    double dLat = lat2Rad - lat1Rad;
    double dLon = lon2Rad - lon1Rad;

    // Haversine-Formel
    double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
               cos(lat1Rad) * cos(lat2Rad) *
               sin(dLon / 2.0) * sin(dLon / 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    // Abstand in Metern
    return double(EARTH_RADIUS) * c;
}


#endif
