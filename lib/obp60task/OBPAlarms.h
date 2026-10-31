#pragma once
#include "Pagedata.h"
#include "obp60task.h"
#include <unordered_map>

class Alarms {
public:
    enum class AlarmState {
        INACTIVE,
        ACTIVE,
        SNOOZE
    };

    enum class SignalType {
        MESSAGE,
        LED,
        BUZZER
    };

    enum class BoatDataFmt : uint8_t {
        SPEED,
        WINDSPEED,
        COURSE,
        WIND,
        DEPTH,
        ROTATION,
        POSITION,
        LAT,
        LON,
        DISTANCE,
        DOP,
        XTE,
        TEMP,
        TIME,
        ATTITUDE,
        PRESSUREBAR,
        PRESSUREPA,
        HUMIDITY,
        VOLT,
        CURRENT,
        RPM,
        LITRE,
        CBM,
        OTHER
    };

    struct tAlarm {
        GwApi::BoatValue* boatValue = nullptr; // the monitored boat value
        BoatDataFmt format; // short form for boatValue->getFormat()
        bool alarmSet = false; // Alarm user setting [OFF, ON]
        double latitude = 0.0; // Alarm target latitude position
        double longitude = 0.0; // Alarm target longitude position
        double highLimit = 0.0; // Upper threshold for alarm
        double lowLimit = 0.0; // Lower threshold for alarm
        double hysteresis = 0.0; // Threshold margin for check of low/high limit miss
        bool hitHighLimit = false; // marks that high threshold was reached
        bool hitLowLimit = false; // marks that low threshold was reached
        AlarmState state = AlarmState::INACTIVE; // Alarm state
        ulong almDelay = 0; // Alarm delay time setting for this alarm in millis
        ulong pendingSince = 0;    // millis() when alarm condition was first detected, 0 = no pending condition
        bool pendingHigh = false;  // pending condition was caused by high limit
        bool pendingLow = false;   // pending condition was caused by low limit
        ulong snzTimer = 0; // Snooze time setting for this alarm in millis
        ulong snzTime = 0; // Current snooze time in millis
        ulong suspTime = 0; // Alarm suspend time in millis
        SignalType signal = SignalType::MESSAGE; // how to signal MESSAGE | LED | BUZZER (for future use)
        bool buzzerOn = false; // Indicates buzzer to ring for this alarm occurence
        ulong buzzerTime = 0; // Stores start time of buzzer sound
        uint8_t alarmTime = 0; // seconds until alarm disappeares without user interaction (for future use)
        bool initialized = false; // Indicates a successfully initialized alarm
    };

private:
    static constexpr size_t MAX_ALARMS = 5; // max. number of alarm definitions
    static constexpr const char* POSITION_ALARM = "GPSP"; // boat data value name for position alarms selected by LAT or LON type
    static constexpr const char* TIME_ALARM = "ALMT"; // alarm value for time alarms, fed from GPST or RTC
    static constexpr double NO_HIGH_LIMIT =  std::numeric_limits<double>::infinity();
    static constexpr double NO_LOW_LIMIT  = -std::numeric_limits<double>::infinity();
    static const std::map<BoatDataFmt, double> hysteresis;
    static constexpr double HYSTERESIS = 0.04; // 4 percent hysteresis if no other value applies

    std::vector<tAlarm> alarmList; // array for list of boatValues with alarms specified
    BoatValueList& m_boatValueList;
    CommonData* m_common;
    GwConfigHandler* m_config;
    GwLog* logger;
    GwApi* m_api;

    // User settings for various boat data formats
    static String lengthFormat; // [m|ft]
    static String distanceFormat; // [m|km|nm]
    static String speedFormat; // [m/s|km/h|kn]
    static String windspeedFormat; // [m/s|km/h|kn|bft]
    static String tempFormat; // [K|C|F]
    static uint buzzerPower; // [0..100]
    static double timeZoneSec; // local time offset to UTC in seconds (from config timeZone)
    static ulong alrmSuspTime; // time in millis for each alarm being suspended after confirmation

    static double convertValueToSI(const double value, const BoatDataFmt valueFormat); // Convert boat values from user input format to internal standard SI format

public:
    Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log, GwApi* api);
    ~Alarms() = default;
    void readConfig(GwConfigHandler* config);
    void initAlarmData(); // Initialize alarm parameters for each alarm type
    std::vector<tAlarm>* getAlarmList() { return &alarmList; } // Get full list of defined alarms
    int getNoOfAlarms() { return alarmList.size(); } // Get number of user defined alarms
    tAlarm* getAlarm(int index); // Get alarm data for <index>
    tAlarm* getActiveAlarm(); // Get 1st active alarm in list
    void checkAlarms(); // Test current boat values for alarm conditions and manage alarm flag
    bool checkHighHit(const tAlarm& alarm, bool withHysteresis = false) const; // check whether boatvalue exceeds high threshold
    bool checkLowHit(const tAlarm& alarm, bool withHysteresis = false) const; // check whether boatvalue exceeds high threshold
    int countAlarms(); // Count no. of active alarms
    bool isAlarm(); // Test for any currently active alarm
    bool isAlarm(tAlarm& alarm){ return getAlarmState(alarm) == AlarmState::ACTIVE; } // Test if <alarm> is currently active
    void setAlarmState(tAlarm& alarm, AlarmState state) { alarm.state = state; } // Set alarm state [ACTIVE, INACTIVE, SNOOZE]
    AlarmState getAlarmState(const tAlarm& alarm) const { return alarm.state; }
    bool triggeredByHigh (const tAlarm& alarm) const { return alarm.hitHighLimit; }
    bool triggeredByLow (const tAlarm& alarm) const { return alarm.hitLowLimit; }
    bool suspTimeOver(const tAlarm& alarm) const;
    bool snoozeTimeOver(const tAlarm& alarm) const;
    void activateAlarm(tAlarm& alarm);
    void suspendAlarm(tAlarm& alarm);
    void snoozeAlarm(tAlarm& alarm);
    double timeSecToLocal(double utcSec) const; // Convert time in seconds of day from UTC to local
    double timeSecToUTC(double localSec) const; // Convert time in seconds of day from local to UTC
};
