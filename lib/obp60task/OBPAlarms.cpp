#include "OBPAlarms.h"
#include "OBPDataOperations.h"
#include "OBP60Extensions.h"

String Alarms::lengthFormat = "";
String Alarms::distanceFormat = "";
String Alarms::speedFormat = "";
String Alarms::windspeedFormat = "";
String Alarms::tempFormat = "";
uint Alarms::buzzerPower = 0;
double Alarms::timeZoneSec = 0.0;
ulong Alarms::alrmSuspTime = 0;

static constexpr double TIME_ALARM_WINDOW = 60.0; // seconds during which a time alarm is "hit"
static constexpr double SECONDS_PER_DAY = 86400.0;

const std::map<Alarms::BoatDataFmt, double> Alarms::hysteresis = {
    { BoatDataFmt::SPEED, 1.0 },
    { BoatDataFmt::WINDSPEED, 1.0 },
    { BoatDataFmt::DEPTH, 1.0 },
    { BoatDataFmt::COURSE, 5 * DEG_TO_RAD },
    { BoatDataFmt::WIND, 5 * DEG_TO_RAD },
    { BoatDataFmt::TEMP, 5.0 },
    { BoatDataFmt::ROTATION, 5 * DEG_TO_RAD },
    { BoatDataFmt::DOP, 0.5 },
    { BoatDataFmt::XTE, 0.5 },
    { BoatDataFmt::LAT, 0.0002 }, // approx. 20m
    { BoatDataFmt::LON, 0.0003 }, // approx. 20m at Lat 54°N
    { BoatDataFmt::DISTANCE, 20 },
    { BoatDataFmt::TEMP, 5.0 },
    { BoatDataFmt::TIME, 0.0 },
    { BoatDataFmt::ATTITUDE, 5 * DEG_TO_RAD },
    { BoatDataFmt::PRESSUREBAR, 0.3 }, // pressure in bar
    { BoatDataFmt::PRESSUREPA, 30000.0 }, // pressure in Pa
    { BoatDataFmt::HUMIDITY, 1.0 }, // humidity in percent
    { BoatDataFmt::VOLT, 0.1 }, // voltage
    { BoatDataFmt::CURRENT, 0.5 }, // current in amperes
    { BoatDataFmt::RPM, 200 }, // tachometer in rpm
    { BoatDataFmt::LITRE, 10 }, // volume in litres
    { BoatDataFmt::CBM, 0.01 }, // volume in cbm
};

// Initialize boat data format as a shorter version of boatValue->getFormat() standard format indicator
static Alarms::BoatDataFmt initFormat(GwApi::BoatValue* bValue)
{
    String format = bValue->getFormat();
    String name = bValue->getName();

    if (format == "formatKnots" && (name == "SOG" || name == "STW" || name == "DFT")) {
        return Alarms::BoatDataFmt::SPEED;
    } else if (format == "formatKnots" && (name == "AWS" || name == "TWS" || name == "MaxAws" || name == "MaxTws")) {
        return Alarms::BoatDataFmt::WINDSPEED;
    } else if (format == "formatCourse") {
        return Alarms::BoatDataFmt::COURSE;
    } else if (format == "formatWind") {
        return Alarms::BoatDataFmt::WIND;
    } else if (format == "formatDepth") {
        return Alarms::BoatDataFmt::DEPTH;
    } else if (format == "formatRot") {
        return Alarms::BoatDataFmt::ROTATION;
    } else if (format == "formatLatitude") {
        return Alarms::BoatDataFmt::LAT;
    } else if (format == "formatLongitude") {
        return Alarms::BoatDataFmt::LON;
    } else if (format == "mtr2nm") {
        return Alarms::BoatDataFmt::DISTANCE;
    } else if (format == "formatDop") {
        return Alarms::BoatDataFmt::DOP;
    } else if (format == "formatXte") {
        return Alarms::BoatDataFmt::XTE;
    } else if (format == "kelvinToC") {
        return Alarms::BoatDataFmt::TEMP;
    } else if (format == "formatTime") {
        return Alarms::BoatDataFmt::TIME;
     } else if (format.startsWith("formatXdr:A")) {
        return Alarms::BoatDataFmt::ATTITUDE;
    } else if (format == "formatXdr:P:B") {
        return Alarms::BoatDataFmt::PRESSUREBAR;
    } else if (format == "formatXdr:P:P") {
        return Alarms::BoatDataFmt::PRESSUREPA;
    } else if (format == "formatXdr:H:P") {
        return Alarms::BoatDataFmt::HUMIDITY;
    } else if (format == "formatXdr:U:V") {
        return Alarms::BoatDataFmt::VOLT;
    } else if (format == "formatXdr:I:A") {
        return Alarms::BoatDataFmt::CURRENT;
    } else if (format == "formatXdr:T:R") {
        return Alarms::BoatDataFmt::RPM;
    } else if (format == "formatXdr:V:L") {
        return Alarms::BoatDataFmt::LITRE;
    } else if (format == "formatXdr:V:M") {
        return Alarms::BoatDataFmt::CBM;
    } else {
        return Alarms::BoatDataFmt::OTHER; // should not happen
    }
}

enum class AngleZone : uint8_t {
    INSIDE,
    BEYOND_HIGH,
    BEYOND_LOW
};

static AngleZone angleOutOfZone(double value, double low, double high, double margin)
{
    double width = WindUtils::to2PI(high - low);
    if (width == 0.0) {
        return AngleZone::INSIDE; // no usable range -> no alarm
    }
    margin = std::min(margin, width / 2.0);
    double w = width - 2.0 * margin;
    double fromStart = WindUtils::to2PI(value - (low + margin));

    if (fromStart <= w) {
        return AngleZone::INSIDE;
    }
    double beyond = fromStart - w;
    double before = M_TWOPI - fromStart;
    return (beyond <= before) ? AngleZone::BEYOND_HIGH : AngleZone::BEYOND_LOW;
}

// Map any number of seconds (negative or beyond 24 h) to [0, 86400)
static double secondsOfDay(double seconds) 
{ 
    return fmod(fmod(seconds, SECONDS_PER_DAY) + SECONDS_PER_DAY, SECONDS_PER_DAY);
}

double Alarms::timeSecToLocal(double utcSec) const
{ 
    return secondsOfDay(utcSec + timeZoneSec); 
}

double Alarms::timeSecToUTC(double localSec) const 
{ 
    return secondsOfDay(localSec - timeZoneSec); 
}

// Parse "hh:mm" (24h) into seconds since midnight; returns NaN for invalid input
static double convTimetoSecs(const String& text)
{
    int h = -1, m = -1;
    char trailing;
    if (sscanf(text.c_str(), "%d:%d%c", &h, &m, &trailing) != 2) {
        // "%c" catches extra characters: sscanf returns 2 only if the string consists exactly of 2 fields "h:m"
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return h * 3600.0 + m * 60.0;
}

// Check if <nowSec> (seconds of day) lies within the window [triggerSec, triggerSec + TIME_ALARM_WINDOW)
static bool timeOfDayHit(double nowSec, double triggerSec)
{
    if (!std::isfinite(nowSec) || !std::isfinite(triggerSec)) {
        return false;
    }
    double diff = fmod(fmod(nowSec - triggerSec, SECONDS_PER_DAY) + SECONDS_PER_DAY, SECONDS_PER_DAY);
    return diff < TIME_ALARM_WINDOW;
}

Alarms::Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log, GwApi* api)
    : m_boatValueList(boatValueList)
    , m_common(common)
    , logger(log)
    , m_api(api)
{
}

// Initial load of alarm definitions into internal list
void Alarms::readConfig(GwConfigHandler* config)
{
    String noInstance = ""; // the instance variable to read from config
    String noAlarmSet = "";
    String noAlmLat = "";
    String noAlmLon = "";
    String noHighLimit = "";
    String noLowLimit = "";
    String noAlmDelay = "";
    String noSnzTime = "";
    String instance = "";

    alarmList.clear();
    alarmList.reserve(MAX_ALARMS);

    // Read user settings
    lengthFormat = config->getString(config->lengthFormat);
    distanceFormat = config->getString(config->distanceFormat);
    speedFormat = config->getString(config->speedFormat);
    windspeedFormat = config->getString(config->windspeedFormat);
    tempFormat = config->getString(config->tempFormat);
    buzzerPower = uint(config->getString(config->buzzerPower, "100").toInt());
    timeZoneSec = double(int(config->getConfigItem(config->timeZone, true)->asFloat() * 3600.0));
    alrmSuspTime = ulong(config->getString(config->almSuspTime, "60").toInt() * 1000 * 60); // user setting is in minutes

    // Read alarm definition for data instances
    for (size_t i = 0; i < MAX_ALARMS; i++) {
        noInstance = "almInstance" + String(i + 1);
        noAlarmSet = "almSet" + String(i + 1);
        noAlmLat = "almLat" + String(i + 1);
        noAlmLon = "almLon" + String(i + 1);
        noHighLimit = "almHighLimit" + String(i + 1);
        noLowLimit = "almLowLimit" + String(i + 1);
        noAlmDelay = "almDelay" + String(i + 1);
        noSnzTime = "snzTime" + String(i + 1);

        instance = config->getString(noInstance, "---");
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "No alarm definition for instance no. %d", i + 1);
            continue;
        }

        tAlarm a;
        if (instance == "LAT" || instance == "LON") {
            a.boatValue = m_boatValueList.findValueOrCreate(POSITION_ALARM); // For positions, we want to store the distance to the reference location as the alarm value
            a.boatValue->setFormat("mtr2nm");
        } else if (instance == "GPST") {
            a.boatValue = m_boatValueList.findValueOrCreate(TIME_ALARM);
            a.boatValue->setFormat("formatTime");
        } else {
            a.boatValue = m_boatValueList.findValueOrCreate(instance);
        }
        if (!a.boatValue) {
            LOG_DEBUG(GwLog::LOG, "Alarm definition for instance no. %d failed.", i + 1);
            continue;
        }

        a.alarmSet = config->getBool(noAlarmSet, false);
        a.latitude = config->getString(noAlmLat, "").toDouble();
        a.longitude = config->getString(noAlmLon, "").toDouble();

        String sHigh = config->getString(noHighLimit, "");
        String sLow = config->getString(noLowLimit, "");
        sHigh.trim();
        sLow.trim();
        if (instance == "GPST") {
            double t = convTimetoSecs(sHigh); // Time alarm: threshold "hh:mm" (UTC), stored as seconds of day
            a.highLimit = std::isfinite(t) ? timeSecToUTC(t) : NO_HIGH_LIMIT;
            a.lowLimit = NO_LOW_LIMIT; // time alarm has no low threshold
        } else {
            a.highLimit = sHigh.isEmpty() ? NO_HIGH_LIMIT : sHigh.toDouble();
            a.lowLimit = sLow.isEmpty() ? NO_LOW_LIMIT : sLow.toDouble();
        }

        a.almDelay = constrain(ulong(config->getString(noAlmDelay, "").toInt() * 1000), 0UL, 10000UL); // user setting is in seconds
        a.snzTimer = ulong(config->getString(noSnzTime, "").toInt() * 1000); // user setting is in seconds
        a.state = AlarmState::INACTIVE;
        a.snzTime = 0;
        a.suspTime = 0;
        a.alarmTime = 0;
        a.format = Alarms::BoatDataFmt::OTHER; // set initially to unknown; will be updated as soon as boat value is available
        a.signal = SignalType::MESSAGE;
        alarmList.push_back(a);

        LOG_DEBUG(GwLog::LOG, "Alarm definition added: boat value: %s, format: %s, alarm set: %d, alarm delay: %lu, snooze timer: %lu",
            a.boatValue->getName().c_str(), a.boatValue->getFormat().c_str(), a.alarmSet, a.almDelay, a.snzTimer);

    }

    // Update boat value list with format parameters and adjust user defined limit values -> we need corresponding boat data for proper conversion
    m_api->getBoatDataValues(m_boatValueList.numValues, m_boatValueList.allBoatValues);
    initAlarmData();

    LOG_DEBUG(GwLog::LOG, "All alarm definition data read");
}

// Initialize alarm parameters for each alarm type
void Alarms::initAlarmData() 
{
    for (auto& a : alarmList) {
        if (a.initialized) {
            continue;
        }
        if (a.boatValue->getFormat().isEmpty()) {
            continue;                              // boat data not available yet, try next time
        }
        BoatDataFmt fmt = initFormat(a.boatValue);
        if (fmt == Alarms::BoatDataFmt::OTHER) {
            continue;                              // format still unknown
        }
        a.format = fmt;

        bool hasHigh = std::isfinite(a.highLimit);
        bool hasLow = std::isfinite(a.lowLimit);
        if (hasHigh && a.format != Alarms::BoatDataFmt::TIME) {  // only convert value if other than GPST value
            a.highLimit = convertValueToSI(a.highLimit, a.format);
        }
        if (hasLow) {
            a.lowLimit = convertValueToSI(a.lowLimit, a.format);
        }

        if (a.format == Alarms::BoatDataFmt::COURSE || a.format == Alarms::BoatDataFmt::WIND) {
            // angles require a range of low and high threshold; if one is not given, we set it to 0°
            if (!hasLow) {
                a.lowLimit = 0.0;
            };
            if (!hasHigh) {
                a.highLimit = M_TWOPI;
            }

        } else { // for other boat values we want to make sure that low threshold is not larger than high value
            if (hasHigh && hasLow && a.lowLimit > a.highLimit) {
                a.lowLimit = a.highLimit; // adjust <lowLimit> if bigger than <highLimit>; does not apply to wind angle or direction
            }
        }

        auto it = hysteresis.find(a.format);
        if (it != hysteresis.end()) {
            a.hysteresis = it->second;
        } else {
            a.hysteresis = 0.0;
        }

        a.initialized = true;
        LOG_DEBUG(GwLog::LOG, "Alarm initialized: boat value: %s, format: %s, alarm format: %d, low limit: %.2f, high limit: %.2f, hysteresis: %.2f",
            a.boatValue->getName().c_str(), a.boatValue->getFormat().c_str(), a.format, a.lowLimit, a.highLimit, a.hysteresis);
    }
}

// Get alarm definition for <index>
Alarms::tAlarm* Alarms::getAlarm(int index)
{
    if (index < 0 || index >= (int)alarmList.size()) {
        return nullptr;
    }

    return &alarmList[index];
}

// Get 1st active alarm in list
Alarms::tAlarm* Alarms::getActiveAlarm()
{
    for (auto& alarm : alarmList) {
        if (isAlarm(alarm))
            return &alarm;
    }

    return nullptr;
}

// Check whether boatvalue exceeds high threshold
bool Alarms::checkHighHit(const tAlarm& alarm, bool withHysteresis) const
{
    if (alarm.format == Alarms::BoatDataFmt::TIME) {
        if (alarm.state == AlarmState::SNOOZE) {
            return snoozeTimeOver(alarm); // Snoozed time alarm: the window reopens when the snooze period ends
        }
        return timeOfDayHit(alarm.boatValue->value, alarm.highLimit); // hysteresis not applicable
    }

    if (alarm.format == Alarms::BoatDataFmt::COURSE || alarm.format == Alarms::BoatDataFmt::WIND) {
        if (!std::isfinite(alarm.lowLimit) || !std::isfinite(alarm.highLimit)) {
            return false; // an angle range needs both limits
        }
        double margin = withHysteresis ? alarm.hysteresis : 0.0;
        return angleOutOfZone(alarm.boatValue->value, alarm.lowLimit, alarm.highLimit, margin) == AngleZone::BEYOND_HIGH;
    }

    double margin = 0.0;
    double limit = alarm.highLimit;
    if (!std::isfinite(limit)) { // no high limit defined
        return false;
    }

    if (withHysteresis) {
        margin = (alarm.hysteresis > 0.0) ? alarm.hysteresis : fabs(limit) * HYSTERESIS; // fallback: percent
    }
    return alarm.boatValue->value > (limit - margin);
}

// Check whether boatvalue exceeds low threshold
bool Alarms::checkLowHit(const tAlarm& alarm, bool withHysteresis) const
{
    if (alarm.format == Alarms::BoatDataFmt::TIME) {
        return false;
    }

    if (alarm.format == Alarms::BoatDataFmt::COURSE || alarm.format == Alarms::BoatDataFmt::WIND) {
        if (!std::isfinite(alarm.lowLimit) || !std::isfinite(alarm.highLimit)) {
            return false; // an angle range needs both limits
        }
        double margin = withHysteresis ? alarm.hysteresis : 0.0;
        return angleOutOfZone(alarm.boatValue->value, alarm.lowLimit, alarm.highLimit, margin) == AngleZone::BEYOND_LOW;
    }

    double margin = 0.0;
    double limit = alarm.lowLimit;
    if (!std::isfinite(limit)) { // no low limit defined
        return false;
    }

    if (withHysteresis) {
        margin = (alarm.hysteresis > 0.0) ? alarm.hysteresis : fabs(limit) * HYSTERESIS; // fallback: percent
    }

    return alarm.boatValue->value < (limit + margin);
}

// Test current boat values for alarm conditions and activate alarm flag
void Alarms::checkAlarms()
{
    initAlarmData(); // We need to test and perform alarm data initialization for boat data types which have not been available at device startup

    for (auto& alarm : alarmList) {
        LOG_DEBUG(GwLog::DEBUG, "checkAlarms: boat value: %s, low limit: %.2f, high limit: %.2f, value: %.2f, valid: %d, set: %d",
            alarm.boatValue->getName().c_str(), alarm.lowLimit, alarm.highLimit, alarm.boatValue->value, alarm.boatValue->valid, alarm.alarmSet);

        // Helper: drop any pending (delayed) alarm condition
        auto clearPending = [&alarm]() {
            alarm.pendingSince = 0;
            alarm.pendingHigh = false;
            alarm.pendingLow = false;
        };

        // For position alarms, calculate distance to reference position as alarm value
        if (alarm.boatValue->getName() == POSITION_ALARM && alarm.alarmSet) {
            GwApi::BoatValue* bValueLat = m_boatValueList.findValueOrCreate("LAT"); // read current location
            GwApi::BoatValue* bValueLon = m_boatValueList.findValueOrCreate("LON");
            if (bValueLat->valid && bValueLon->valid) {
                alarm.boatValue->value = distanceBetweenCoordinates(bValueLat->value, bValueLon->value, alarm.latitude, alarm.longitude);
                alarm.boatValue->valid = true;
            } else {
                alarm.boatValue->valid = false;
            }
        }

        // For time alarms, use GPS time; fall back to RTC if there is no GPS time
        if (alarm.format == BoatDataFmt::TIME && alarm.alarmSet) {
            GwApi::BoatValue* bValueGpst = m_boatValueList.findValueOrCreate("GPST");
            if (bValueGpst->valid) {
                alarm.boatValue->value = bValueGpst->value;
                alarm.boatValue->valid = true;
            } else if (m_common->data.rtcValid) {
                const tm& rtcT = m_common->data.rtcTime;
                alarm.boatValue->value = rtcT.tm_hour * 3600.0 + rtcT.tm_min * 60.0 + rtcT.tm_sec;
                alarm.boatValue->valid = true;
            } else {
                alarm.boatValue->valid = false;
            }
        }        

        if (!alarm.alarmSet || !alarm.boatValue->valid) {
            clearPending();
            setAlarmState(alarm, AlarmState::INACTIVE); // ToDo: implement holdValues and 4 sec delay after invalid data
            alarm.hitHighLimit = false;
            alarm.hitLowLimit = false;
            continue;
        }

        AlarmState currentState = getAlarmState(alarm);

        // Check if current alarm value is below limit incl. threshold
        bool highCleared = !checkHighHit(alarm, true);
        bool lowCleared = !checkLowHit(alarm, true);

        if ((currentState == AlarmState::INACTIVE && suspTimeOver(alarm)) || (currentState == AlarmState::SNOOZE && snoozeTimeOver(alarm))) {
            bool pending = (alarm.pendingSince != 0);

            if (!pending) {
                bool highHit = checkHighHit(alarm);
                bool lowHit = checkLowHit(alarm);
                if (highHit || lowHit) {
                    alarm.pendingSince = millis(); // start delay timer
                    if (alarm.pendingSince == 0) {
                        alarm.pendingSince = 1; // 0 is reserved for "not pending"
                    }
                    alarm.pendingHigh = highHit;
                    alarm.pendingLow = lowHit;
                    pending = true;
                } else if (currentState == AlarmState::SNOOZE) {
                    // Snooze expired and condition is gone (e.g. time window passed): resume monitoring
                    setAlarmState(alarm, AlarmState::INACTIVE);
                }
             } else if (highCleared && lowCleared) {
                // Condition vanished (incl. hysteresis) within delay time -> no alarm
                clearPending();
                pending = false;
            }

            // Delay time elapsed while condition still valid -> activate alarm
            if (pending && (millis() - alarm.pendingSince) >= alarm.almDelay) {
                activateAlarm(alarm);
                alarm.hitHighLimit = alarm.pendingHigh;
                alarm.hitLowLimit = alarm.pendingLow;
                clearPending();
            }


        } else if (currentState == AlarmState::ACTIVE) {
            clearPending();
            if (alarm.format != Alarms::BoatDataFmt::TIME && highCleared && lowCleared) {
                // Time alarms stay active until confirmed by the user
                suspendAlarm(alarm);
                alarm.hitHighLimit = false;
                alarm.hitLowLimit = false;
            }

        } else {
            // INACTIVE with suspend time not over, or SNOOZE with snooze time not over
            clearPending();
        }
        LOG_DEBUG(GwLog::DEBUG, "checkAlarms: alarm state for alarm: %s: %d", alarm.boatValue->getName().c_str(), getAlarmState(alarm));
    }
}

// Count no. of active alarms
int Alarms::countAlarms()
{
    int count = 0;
    for (auto& alarm : alarmList) {
        if (getAlarmState(alarm) == AlarmState::ACTIVE) {
            count++;
        }
    }

    return count;
}

// Test for any currently active alarm
bool Alarms::Alarms::isAlarm()
{
    for (auto& alarm : alarmList) {
        if (getAlarmState(alarm) == AlarmState::ACTIVE) {
            return true;
        }
    }

    return false;
}

void Alarms::activateAlarm(tAlarm& alarm)
{
    setAlarmState(alarm, AlarmState::ACTIVE);
    alarm.buzzerOn = true;
    alarm.buzzerTime = millis();
}

void Alarms::suspendAlarm(tAlarm& alarm)
{
    setAlarmState(alarm, AlarmState::INACTIVE);
    alarm.suspTime = millis();
}

void Alarms::snoozeAlarm(tAlarm& alarm)
{
    setAlarmState(alarm, AlarmState::SNOOZE);
    alarm.snzTime = millis();
}

// Check alarm inactive time
bool Alarms::suspTimeOver(const tAlarm& alarm) const
{
    if (alarm.suspTime == 0) { // no suspend happened yet -> initial phase
        return true;
    }

    if ((millis() - alarm.suspTime) > alrmSuspTime) {
        return true;
    } else {
        return false;
    }
}

// Check snooze time
bool Alarms::snoozeTimeOver(const tAlarm& alarm) const
{
    if (alarm.snzTime == 0) { // no snooze happened yet -> initial phase
        return true;
    }

    if ((millis() - alarm.snzTime) > alarm.snzTimer) {
        return true;
    } else {
        return false;
    }
}

// Convert boat values from user input format to internal standard SI format
double Alarms::convertValueToSI(const double value, const BoatDataFmt valueFormat)
{
    double result = value;

    if (valueFormat == Alarms::BoatDataFmt::SPEED) {
        if (speedFormat == "km/h") {
            result /= 3.6; // Convert km/h to m/s
        } else if (speedFormat == "kn") {
            result /= 1.94384; // Convert kn to m/s
        } else { // value is given in SI m/s
            // No conversion needed
        }

    } else if (valueFormat == BoatDataFmt::WINDSPEED) {
        if (windspeedFormat == "km/h") {
            result /= 3.6; // Convert km/h to m/s
        } else if (windspeedFormat == "kn") {
            result /= 1.94384; // Convert kn to m/s
        } else if (windspeedFormat == "bft") {
            result = 0.836 * std::pow(value, 1.5); // WMO standard empirical formula: v = 0.836 * B^1.5
        } else { // value is given in SI m/s
            // No conversion needed
        }

    } else if (valueFormat == Alarms::BoatDataFmt::COURSE) {
        result *= DEG_TO_RAD;
        result = WindUtils::to2PI(result);

    } else if (valueFormat == Alarms::BoatDataFmt::WIND) {
        result *= DEG_TO_RAD;
        result = WindUtils::to2PI(result);

    } else if (valueFormat == Alarms::BoatDataFmt::ROTATION || valueFormat == Alarms::BoatDataFmt::ATTITUDE) {
        result *= DEG_TO_RAD;
        result = WindUtils::toPI(result);

    } else if (valueFormat == Alarms::BoatDataFmt::DEPTH) {
        if (lengthFormat == "ft") {
            result /= 3.28084; // Convert ft to m
        } else { // value is given in SI metres
            // No conversion needed
        }

    } else if (valueFormat == Alarms::BoatDataFmt::XTE || valueFormat == Alarms::BoatDataFmt::DISTANCE) {
        if (distanceFormat == "km") {
            result *= 1000;
        } else if (distanceFormat == "nm") {
            result *= 1852;
        } else { // value is given in SI metres
            // No conversion needed
        }

    } else if (valueFormat == Alarms::BoatDataFmt::TEMP) {
        if (tempFormat == "C") {
            result += 273.15;
        } else if (tempFormat == "F") {
            result = (result - 32.0) * (5.0 / 9.0) + 273.15; // Convert °F to K
        } else { // value is given in SI Kelvin
            // No conversion needed
        }
    }

    return result;
}