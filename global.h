#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QDateTime>

// Forward declarations
class ELM;
class SettingsManager;
class ConnectionManager;

// Communication protocols used in Jeep WJ
enum WJProtocol {
    PROTOCOL_UNKNOWN,
    PROTOCOL_ISO_14230_4_KWP_FAST,    // For EDC15 Engine ECU
    PROTOCOL_J1850_VPW,    // For Transmission, PCM, ABS, etc.
    PROTOCOL_AUTO_DETECT
};

// Jeep WJ module types
enum WJModule {
    MODULE_UNKNOWN,
    MODULE_ENGINE_EDC15,   // Engine Control Unit (EDC15)
    MODULE_TRANSMISSION,   // Transmission Control Module
    MODULE_PCM,           // Powertrain Control Module
    MODULE_ABS,           // Anti-lock Braking System
    MODULE_AIRBAG,        // Supplemental Restraint System
    MODULE_HVAC,          // Heating, Ventilation, Air Conditioning
    MODULE_BODY,          // Body Control Module
    MODULE_RADIO          // Radio/Infotainment
};

// Enhanced initialization states for dual protocol support
enum WJInitState {
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_DETECTING_PROTOCOL,
    STATE_RESETTING,
    STATE_CONFIGURING_PROTOCOL,
    STATE_CONFIGURING_ISO9141,
    STATE_CONFIGURING_J1850,
    STATE_SETTING_WAKEUP,
    STATE_SETTING_HEADER,
    STATE_FAST_INIT,
    STATE_START_COMMUNICATION,
    STATE_SECURITY_ACCESS,
    STATE_DIAGNOSTIC_ROUTINE,
    STATE_READY_ISO9141,
    STATE_READY_J1850,
    STATE_READY,
    STATE_SWITCHING_PROTOCOL,
    STATE_ERROR
};

// Enhanced ELM327 command structure for multi-protocol support
struct WJCommand {
    QString command;
    QString expectedResponse;
    QString description;
    int timeoutMs;
    bool isCritical;
    WJProtocol requiredProtocol;
    WJModule targetModule;

    WJCommand() : timeoutMs(1000), isCritical(false), requiredProtocol(PROTOCOL_UNKNOWN), targetModule(MODULE_UNKNOWN) {}

    WJCommand(const QString& cmd, const QString& resp, const QString& desc,
              int timeout = 1000, WJProtocol protocol = PROTOCOL_UNKNOWN,
              WJModule module = MODULE_UNKNOWN, bool critical = false)
        : command(cmd), expectedResponse(resp), description(desc),
        timeoutMs(timeout), isCritical(critical), requiredProtocol(protocol), targetModule(module) {}
};

// Jeep WJ comprehensive sensor data structure
struct WJSensorData {
    // Engine (EDC15) data - ISO_14230_4_KWP_FAST
    struct EngineData {
        double mafActual;           // g/s
        double mafSpecified;        // g/s
        double railPressureActual;  // bar
        double railPressureSpecified; // bar
        double mapActual;           // mbar
        double mapSpecified;        // mbar
        double coolantTemp;         // °C
        double intakeAirTemp;       // °C
        double throttlePosition;    // %
        double engineRPM;           // rpm
        double injectionQuantity;   // mg
        double injector1Correction; // mg
        double injector2Correction; // mg
        double injector3Correction; // mg
        double injector4Correction; // mg
        double injector5Correction; // mg
        double batteryVoltage;      // V
        bool dataValid;
        qint64 lastUpdate;
    } engine;

    // Transmission data - J1850
    struct TransmissionData {
        double oilTemp;             // °C
        double inputSpeed;          // rpm
        double outputSpeed;         // rpm
        double torqueConverter;     // %
        double currentGear;         // gear number
        double linePresssure;       // psi
        double shiftSolenoidA;      // %
        double shiftSolenoidB;      // %
        double tccSolenoid;         // %
        bool dataValid;
        qint64 lastUpdate;
    } transmission;

    // PCM data - J1850
    struct PCMData {
        double vehicleSpeed;        // km/h
        double engineLoad;          // %
        double fuelTrimST;          // %
        double fuelTrimLT;          // %
        double o2Sensor1;           // V
        double o2Sensor2;           // V
        double timingAdvance;       // degrees
        double barometricPressure;  // kPa
        bool dataValid;
        qint64 lastUpdate;
    } pcm;

    // ABS data - J1850
    struct ABSData {
        double wheelSpeedFL;        // km/h
        double wheelSpeedFR;        // km/h
        double wheelSpeedRL;        // km/h
        double wheelSpeedRR;        // km/h
        double yawRate;             // deg/s
        double lateralAccel;        // g
        bool dataValid;
        qint64 lastUpdate;
    } abs;

    // Metadata
    WJProtocol currentProtocol;
    WJModule activeModule;
    QString lastError;
    qint64 globalLastUpdate;

    WJSensorData() { reset(); }
    void reset();
};

// Enhanced diagnostic trouble code structure for multi-module support
struct WJ_DTC {
    QString code;           // e.g., "P0123"
    QString description;    // Human readable description
    WJModule sourceModule;  // Which module reported this DTC
    WJProtocol protocol;    // Which protocol was used to read it
    bool pending;          // Is it a pending code
    bool confirmed;        // Is it a confirmed code
    int occurrence;        // How many times it occurred
    qint64 timestamp;      // When it was read

    WJ_DTC() : sourceModule(MODULE_UNKNOWN), protocol(PROTOCOL_UNKNOWN),
        pending(false), confirmed(false), occurrence(0), timestamp(0) {}

    WJ_DTC(const QString& dtcCode, const QString& desc, WJModule module,
           WJProtocol prot, bool isPending = false)
        : code(dtcCode), description(desc), sourceModule(module), protocol(prot),
        pending(isPending), confirmed(!isPending), occurrence(1),
        timestamp(QDateTime::currentMSecsSinceEpoch()) {}
};

// Jeep WJ module configuration
struct WJModuleConfig {
    WJModule module;
    WJProtocol protocol;
    QString ecuHeader;
    QString wakeupMessage;
    QStringList initCommands;
    QStringList diagnosticCommands;
    int defaultTimeout;

    WJModuleConfig() : module(MODULE_UNKNOWN), protocol(PROTOCOL_UNKNOWN), defaultTimeout(1000) {}
};

// Jeep WJ Specific Constants
namespace WJ {
// Module headers and addresses
namespace Headers {
extern const QString ENGINE_EDC15;      // ISO_14230_4_KWP_FAST
extern const QString TRANSMISSION;      // J1850 VPW
extern const QString PCM;              // J1850 VPW
extern const QString ABS;              // J1850 VPW
extern const QString AIRBAG;           // J1850 VPW
extern const QString HVAC;             // J1850 VPW
extern const QString BODY;             // J1850 VPW
}

// Protocol configurations
namespace Protocols {
extern const QString ISO_14230_4_KWP_FAST;
extern const QString J1850_VPW;
extern const int ISO_BAUD_RATE;
extern const int J1850_BAUD_RATE;
extern const int DEFAULT_TIMEOUT;
extern const int FAST_INIT_TIMEOUT;
extern const int PROTOCOL_SWITCH_TIMEOUT;
extern const int RESET_TIMEOUT;
}

// Wakeup messages for different modules
namespace WakeupMessages {
extern const QString ENGINE_EDC15;
extern const QString TRANSMISSION;
extern const QString PCM;
extern const QString ABS;
extern const QString AIRBAG;
}

// Engine (EDC15) commands - ISO_14230_4_KWP_FAST
namespace Engine {
extern const QString START_COMMUNICATION;
extern const QString SECURITY_ACCESS_REQUEST;
extern const QString SECURITY_ACCESS_KEY;
extern const QString START_DIAGNOSTIC_ROUTINE;
extern const QString READ_DTC;
extern const QString CLEAR_DTC;
extern const QString READ_MAF_DATA;
extern const QString READ_RAIL_PRESSURE_ACTUAL;
extern const QString READ_RAIL_PRESSURE_SPEC;
extern const QString READ_MAP_DATA;
extern const QString READ_INJECTOR_DATA;
extern const QString READ_MISC_DATA;
extern const QString READ_COOLANT_TEMP;
extern const QString READ_ENGINE_RPM;
extern const QString READ_VEHICLE_SPEED;
extern const QString READ_BATTERY_VOLTAGE;
}

// Transmission commands - J1850 VPW
namespace Transmission {
extern const QString READ_DTC;
extern const QString CLEAR_DTC;
extern const QString READ_TRANS_DATA;
extern const QString READ_GEAR_RATIO;
extern const QString READ_SOLENOID_STATUS;
extern const QString READ_PRESSURE_DATA;
extern const QString READ_TEMP_DATA;
extern const QString READ_SPEED_DATA;
}

// PCM commands - J1850 VPW
namespace PCM {
extern const QString READ_DTC;
extern const QString CLEAR_DTC;
extern const QString READ_LIVE_DATA;
extern const QString READ_FUEL_TRIM;
extern const QString READ_O2_SENSORS;
extern const QString READ_ENGINE_DATA;
extern const QString READ_EMISSION_DATA;
extern const QString READ_FREEZE_FRAME;
}

// ABS commands - J1850 VPW
namespace ABS {
extern const QString READ_DTC;
extern const QString CLEAR_DTC;
extern const QString READ_WHEEL_SPEEDS;
extern const QString READ_BRAKE_DATA;
extern const QString READ_STABILITY_DATA;
}

// Expected response prefixes
namespace Responses {
// ISO_14230_4_KWP_FAST responses (Engine)
extern const QString ENGINE_MAF;
extern const QString ENGINE_RAIL_PRESSURE;
extern const QString ENGINE_INJECTOR;
extern const QString ENGINE_DTC;
extern const QString ENGINE_COMMUNICATION;
extern const QString ENGINE_SECURITY_ACCESS;
extern const QString ENGINE_SECURITY;
extern const QString ENGINE_SECURITY_KEY;
extern const QString ENGINE_DIAGNOSTIC;

// J1850 responses (Other modules)
extern const QString TRANS_DTC;
extern const QString TRANS_DATA;
extern const QString PCM_DTC;
extern const QString PCM_DATA;
extern const QString ABS_DTC;
extern const QString ABS_DATA;

// Common responses
extern const QString OK;
extern const QString BUS_INIT_OK;
extern const QString ELM327_ID;
}

// Error codes for both protocols
extern const QStringList ALL_ERROR_CODES;
extern const QStringList KWP2000_ERROR_CODES;
extern const QStringList J1850_ERROR_CODES;

// Protocol validation helpers
namespace Validation {
bool isKWP2000Response(const QString& response);
bool isOBDResponse(const QString& response);
bool isErrorResponse(const QString& response);
}
}

// Global variables for multi-protocol communication
extern QStringList runtimeCommands;
extern int interval;
extern QList<WJCommand> initializeCommands;
extern WJProtocol currentActiveProtocol;
extern WJModule currentActiveModule;

// Enhanced command sequences for both protocols
namespace WJCommands {
// Get initialization sequence for specific protocol
QList<WJCommand> getInitSequence(WJProtocol protocol);

// Get protocol switching commands
QList<WJCommand> getProtocolSwitchCommands(WJProtocol fromProtocol, WJProtocol toProtocol);

// Get module-specific initialization
QList<WJCommand> getModuleInitCommands(WJModule module);

// Complete module connection
QList<WJCommand> getCompleteModuleConnection(WJModule module);

// Get diagnostic commands for specific module
QList<WJCommand> getDiagnosticCommands(WJModule module);

// Get module configuration
WJModuleConfig getModuleConfig(WJModule module);

// Protocol detection commands
QList<WJCommand> getProtocolDetectionCommands();
}

// Enhanced utility functions for multi-protocol support
namespace WJUtils {
// Protocol management
WJProtocol detectProtocol(const QString& response);
bool isProtocolAvailable(WJProtocol protocol);
QString getProtocolName(WJProtocol protocol);
QString getModuleName(WJModule module);

// Data conversion
double convertTemperature(int rawValue);
double convertPressure(int rawValue);
double convertPercentage(int rawValue);
double convertSpeed(int rawValue);
double convertVoltage(int rawValue);

// DTC formatting for both protocols
QString formatDTCCode(int byte1, int byte2, WJProtocol protocol);

// Protocol-specific validation
bool isValidHexData(const QString& data);
bool isValidResponse(const QString& response, WJProtocol protocol);
bool isError(const QString& response, WJProtocol protocol);

// Data cleaning and parsing
QString cleanData(const QString& input, WJProtocol protocol);
QList<int> parseHexBytes(const QString& data);
int bytesToInt16(int highByte, int lowByte);

// Utility functions
QString getTimestamp();
WJModule getModuleFromHeader(const QString& header);
WJProtocol getProtocolFromModule(WJModule module);
}

// Enhanced DTC database for all Jeep WJ modules
namespace WJ_DTCs {
// Get DTC description for any module
QString getDTCDescription(const QString& dtcCode, WJModule module = MODULE_UNKNOWN);

// Get all known DTC codes for specific module
QStringList getKnownDTCs(WJModule module);

// Check if DTC is critical for any module
bool isCriticalDTC(const QString& dtcCode, WJModule module = MODULE_UNKNOWN);

// Get module-specific DTC categories
QStringList getEngineSpecificDTCs();
QStringList getTransmissionSpecificDTCs();
QStringList getPCMSpecificDTCs();
QStringList getABSSpecificDTCs();
}

// Enhanced data parser for multi-protocol support
class WJDataParser {
public:
    // Engine data parsing (ISO_14230_4_KWP_FAST)
    static bool parseEngineMAFData(const QString& data, WJSensorData& sensorData);
    static bool parseEngineRailPressureData(const QString& data, WJSensorData& sensorData);
    static bool parseEngineMAPData(const QString& data, WJSensorData& sensorData);
    static bool parseEngineInjectorData(const QString& data, WJSensorData& sensorData);
    static bool parseEngineMiscData(const QString& data, WJSensorData& sensorData);
    static bool parseEngineBatteryVoltage(const QString& data, WJSensorData& sensorData);

    // Transmission data parsing (J1850)
    static bool parseTransmissionData(const QString& data, WJSensorData& sensorData);
    static bool parseTransmissionSolenoids(const QString& data, WJSensorData& sensorData);
    static bool parseTransmissionSpeeds(const QString& data, WJSensorData& sensorData);

    // PCM data parsing (J1850)
    static bool parsePCMData(const QString& data, WJSensorData& sensorData);
    static bool parsePCMFuelTrim(const QString& data, WJSensorData& sensorData);
    static bool parsePCMO2Sensors(const QString& data, WJSensorData& sensorData);

    // ABS data parsing (J1850)
    static bool parseABSWheelSpeeds(const QString& data, WJSensorData& sensorData);
    static bool parseABSStabilityData(const QString& data, WJSensorData& sensorData);

    // Fault code parsing for all modules
    static QList<WJ_DTC> parseEngineFaultCodes(const QString& data);
    static QList<WJ_DTC> parseTransmissionFaultCodes(const QString& data);
    static QList<WJ_DTC> parsePCMFaultCodes(const QString& data);
    static QList<WJ_DTC> parseABSFaultCodes(const QString& data);
    static QList<WJ_DTC> parseGenericFaultCodes(const QString& data, WJModule module, WJProtocol protocol);

private:
    static QList<int> extractBytes(const QString& data, int startIndex, int count);
    static double convertRawToPhysical(int rawValue, double factor, double offset = 0.0);
    static bool validateResponseFormat(const QString& data, const QString& expectedPrefix);
};

// Enhanced interface for multi-protocol communication
class WJInterface {
public:
    virtual ~WJInterface() = default;

    // Protocol management
    virtual bool setProtocol(WJProtocol protocol) = 0;
    virtual WJProtocol getCurrentProtocol() const = 0;
    virtual bool switchToModule(WJModule module) = 0;

    // Connection management
    virtual bool initializeConnection(WJProtocol protocol = PROTOCOL_AUTO_DETECT) = 0;
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;

    // Communication
    virtual bool sendCommand(const QString& command, WJModule targetModule = MODULE_UNKNOWN) = 0;
    virtual QString readResponse(int timeoutMs = 1000) = 0;
    virtual bool sendCommandAndWaitResponse(const QString& command, QString& response,
                                            WJModule targetModule = MODULE_UNKNOWN, int timeoutMs = 1000) = 0;

    // Error handling
    virtual QString getLastError() const = 0;
    virtual bool hasError() const = 0;
    virtual void clearError() = 0;
};

// Enhanced diagnostic session manager for complete WJ support
class WJDiagnosticSession {
public:
    WJDiagnosticSession();
    ~WJDiagnosticSession();

    // Session management
    bool startSession(WJInterface* interface);
    void endSession();
    bool isSessionActive() const { return sessionActive; }

    // Protocol and module management
    bool switchToModule(WJModule module);
    WJModule getCurrentModule() const { return activeModule; }
    WJProtocol getCurrentProtocol() const { return activeProtocol; }

    // Comprehensive diagnostic operations
    bool readAllFaultCodes(QList<WJ_DTC>& allDTCs);
    bool clearAllFaultCodes();
    bool readAllSensorData(WJSensorData& data);

    // Engine-specific operations (ISO_14230_4_KWP_FAST)
    bool readEngineData(WJSensorData::EngineData& engineData);
    bool readEngineFaultCodes(QList<WJ_DTC>& dtcs);
    bool clearEngineFaultCodes();

    // Transmission-specific operations (J1850)
    bool readTransmissionData(WJSensorData::TransmissionData& transData);
    bool readTransmissionFaultCodes(QList<WJ_DTC>& dtcs);
    bool clearTransmissionFaultCodes();

    // PCM-specific operations (J1850)
    bool readPCMData(WJSensorData::PCMData& pcmData);
    bool readPCMFaultCodes(QList<WJ_DTC>& dtcs);
    bool clearPCMFaultCodes();

    // ABS-specific operations (J1850)
    bool readABSData(WJSensorData::ABSData& absData);
    bool readABSFaultCodes(QList<WJ_DTC>& dtcs);
    bool clearABSFaultCodes();

    // Individual sensor readings with automatic protocol switching
    bool readEngineMAF(double& actual, double& specified);
    bool readEngineRailPressure(double& actual, double& specified);
    bool readTransmissionTemp(double& temp);
    bool readTransmissionGear(double& gear);
    bool readVehicleSpeed(double& speed);
    bool readWheelSpeeds(double& fl, double& fr, double& rl, double& rr);

    // Error handling
    QString getLastError() const { return lastError; }

private:
    WJInterface* interface;
    bool sessionActive;
    WJModule activeModule;
    WJProtocol activeProtocol;
    bool engineSecurityAccess;
    QString lastError;

    // Internal helper methods
    bool performEngineSecurityAccess();
    bool switchProtocolIfNeeded(WJModule targetModule);
    bool initializeModule(WJModule module);
    bool validateModuleResponse(const QString& response, WJModule module);
};

// Legacy compatibility
using ELM327Command = WJCommand;
using EDC15State = WJInitState;
using EDC15SensorData = WJSensorData;
using EDC15_DTC = WJ_DTC;

namespace EDC15Commands {
QList<ELM327Command> getInitSequence();
QList<ELM327Command> getAlternativeInit();
QStringList getDiagnosticCommands();
}

// Legacy constants for backward compatibility
extern const char* ERROR[];
extern const int ERROR_COUNT;
extern const QString PCM_ECU_HEADER;
extern const QString TRANS_ECU_HEADER;
extern const QString RESET;
extern const QString GET_PROTOCOL;

// Legacy EDC15 namespace for backward compatibility
namespace EDC15 {
extern const QString ECU_HEADER;
extern const QString WAKEUP_MESSAGE;
extern const QString PROTOCOL;
extern const int BAUD_RATE;
extern const int DEFAULT_TIMEOUT;
extern const int FAST_INIT_TIMEOUT;
extern const int SECURITY_TIMEOUT;

extern const QString START_COMMUNICATION;
extern const QString SECURITY_ACCESS_REQUEST;
extern const QString SECURITY_ACCESS_KEY;
extern const QString START_DIAGNOSTIC_ROUTINE;
extern const QString READ_DTC;
extern const QString CLEAR_DTC;

extern const QString READ_MAF_DATA;
extern const QString READ_RAIL_PRESSURE_ACTUAL;
extern const QString READ_RAIL_PRESSURE_SPEC;
extern const QString READ_MAP_DATA;
extern const QString READ_MAP_SPEC;
extern const QString READ_INJECTOR_DATA;
extern const QString READ_MISC_DATA;
extern const QString READ_BATTERY_VOLTAGE;

extern const QString RESPONSE_MAF;
extern const QString RESPONSE_RAIL_PRESSURE;
extern const QString RESPONSE_RAIL_PRESSURE_SPEC;
extern const QString RESPONSE_INJECTOR;
extern const QString RESPONSE_DTC;
extern const QString RESPONSE_COMMUNICATION;
extern const QString RESPONSE_SECURITY_ACCESS;
extern const QString RESPONSE_SECURITY_GRANTED;
extern const QString RESPONSE_DIAGNOSTIC_ROUTINE;

extern const QStringList ERROR_CODES;
}

#endif // GLOBAL_H
