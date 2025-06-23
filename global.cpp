#include "global.h"
#include <QRegularExpression>
#include <QDebug>

// Global variables for multi-protocol communication
QStringList runtimeCommands = {};
int interval = 250;  // Default polling interval
QList<WJCommand> initializeCommands = {};
WJProtocol currentActiveProtocol = PROTOCOL_UNKNOWN;
WJModule currentActiveModule = MODULE_UNKNOWN;

// WJSensorData implementation
void WJSensorData::reset() {
    // Reset engine data
    engine.mafActual = 0.0;
    engine.mafSpecified = 0.0;
    engine.railPressureActual = 0.0;
    engine.railPressureSpecified = 0.0;
    engine.mapActual = 0.0;
    engine.mapSpecified = 0.0;
    engine.coolantTemp = 0.0;
    engine.intakeAirTemp = 0.0;
    engine.throttlePosition = 0.0;
    engine.engineRPM = 0.0;
    engine.injectionQuantity = 0.0;
    engine.injector1Correction = 0.0;
    engine.injector2Correction = 0.0;
    engine.injector3Correction = 0.0;
    engine.injector4Correction = 0.0;
    engine.injector5Correction = 0.0;
    engine.batteryVoltage = 0.0;
    engine.dataValid = false;
    engine.lastUpdate = 0;

    // Reset transmission data
    transmission.oilTemp = 0.0;
    transmission.inputSpeed = 0.0;
    transmission.outputSpeed = 0.0;
    transmission.torqueConverter = 0.0;
    transmission.currentGear = 0.0;
    transmission.linePresssure = 0.0;
    transmission.shiftSolenoidA = 0.0;
    transmission.shiftSolenoidB = 0.0;
    transmission.tccSolenoid = 0.0;
    transmission.dataValid = false;
    transmission.lastUpdate = 0;

    // Reset PCM data
    pcm.vehicleSpeed = 0.0;
    pcm.engineLoad = 0.0;
    pcm.fuelTrimST = 0.0;
    pcm.fuelTrimLT = 0.0;
    pcm.o2Sensor1 = 0.0;
    pcm.o2Sensor2 = 0.0;
    pcm.timingAdvance = 0.0;
    pcm.barometricPressure = 0.0;
    pcm.dataValid = false;
    pcm.lastUpdate = 0;

    // Reset ABS data
    abs.wheelSpeedFL = 0.0;
    abs.wheelSpeedFR = 0.0;
    abs.wheelSpeedRL = 0.0;
    abs.wheelSpeedRR = 0.0;
    abs.yawRate = 0.0;
    abs.lateralAccel = 0.0;
    abs.dataValid = false;
    abs.lastUpdate = 0;

    // Reset metadata
    currentProtocol = PROTOCOL_UNKNOWN;
    activeModule = MODULE_UNKNOWN;
    lastError.clear();
    globalLastUpdate = 0;
}

// WJ namespace implementation
namespace WJ {
// Module headers and addresses
namespace Headers {
const QString ENGINE_EDC15 = "8115F1";
const QString TRANSMISSION = "8118F1";
const QString PCM = "8110F1";
const QString ABS = "8128F1";
const QString AIRBAG = "8138F1";
const QString HVAC = "8158F1";
const QString BODY = "8148F1";
} // End Headers namespace

// Protocol configurations
namespace Protocols {
const QString KWP2000_FAST = "5";
const QString ISO_14230_4_KWP_FAST = "5";
const QString J1850_VPW = "1";
const int ISO_BAUD_RATE = 10400;
const int J1850_BAUD_RATE = 10400;
const int DEFAULT_TIMEOUT = 1000;
const int FAST_INIT_TIMEOUT = 4000;
const int PROTOCOL_SWITCH_TIMEOUT = 2000;
const int RESET_TIMEOUT = 7500;
} // End Protocols namespace

// Wakeup messages for different modules
namespace WakeupMessages {
const QString ENGINE_EDC15 = "8115F13E";
const QString TRANSMISSION = "8118F13E";
const QString PCM = "8110F13E";
const QString ABS = "8128F13E";
const QString AIRBAG = "8138F13E";
} // End WakeupMessages namespace

// Engine (EDC15) commands - KWP2000 Fast
namespace Engine {
const QString START_COMMUNICATION = "81";
const QString SECURITY_ACCESS_REQUEST = "27 01";
const QString SECURITY_ACCESS_KEY = "27 02 CD 46";
const QString START_DIAGNOSTIC_ROUTINE = "31 25 00";
const QString READ_DTC = "03";
const QString CLEAR_DTC = "04";
const QString READ_MAF_DATA = "21 20";
const QString READ_RAIL_PRESSURE_ACTUAL = "21 12";
const QString READ_RAIL_PRESSURE_SPEC = "21 22";
const QString READ_MAP_DATA = "21 15";
const QString READ_INJECTOR_DATA = "21 28";
const QString READ_MISC_DATA = "21 30";
const QString READ_COOLANT_TEMP = "21 05";
const QString READ_ENGINE_RPM = "21 0C";
const QString READ_VEHICLE_SPEED = "21 0D";
const QString READ_BATTERY_VOLTAGE = "ATRV";
} // End Engine namespace

// Transmission commands - J1850 VPW
namespace Transmission {
const QString READ_DTC = "03";
const QString CLEAR_DTC = "04";
const QString READ_TRANS_DATA = "01 00";
const QString READ_GEAR_RATIO = "01 A4";
const QString READ_SOLENOID_STATUS = "01 A5";
const QString READ_PRESSURE_DATA = "01 A6";
const QString READ_TEMP_DATA = "01 05";
const QString READ_SPEED_DATA = "01 0D";
} // End Transmission namespace

// PCM commands - J1850 VPW
namespace PCM {
const QString READ_DTC = "03";
const QString CLEAR_DTC = "04";
const QString READ_LIVE_DATA = "01 00";
const QString READ_FUEL_TRIM = "01 06";
const QString READ_O2_SENSORS = "01 14";
const QString READ_ENGINE_DATA = "01 0C";
const QString READ_EMISSION_DATA = "01 01";
const QString READ_FREEZE_FRAME = "02 00";
} // End PCM namespace

// ABS commands - J1850 VPW
namespace ABS {
const QString READ_DTC = "03";
const QString CLEAR_DTC = "04";
const QString READ_WHEEL_SPEEDS = "01 A0";
const QString READ_BRAKE_DATA = "01 A1";
const QString READ_STABILITY_DATA = "01 A2";
} // End ABS namespace

// Expected response prefixes
namespace Responses {
// KWP2000 Fast responses (Engine EDC15)
const QString ENGINE_MAF = "61 20";
const QString ENGINE_RAIL_PRESSURE = "61 12";
const QString ENGINE_INJECTOR = "61 28";
const QString ENGINE_DTC = "43";
const QString ENGINE_COMMUNICATION = "C1 EF 8F";
const QString ENGINE_SECURITY_ACCESS = "67 01";
const QString ENGINE_SECURITY = "67 01";
const QString ENGINE_SECURITY_KEY = "7F 27";
const QString ENGINE_DIAGNOSTIC = "71 25";

// J1850 responses (Other modules)
const QString TRANS_DTC = "43";
const QString TRANS_DATA = "41";
const QString PCM_DTC = "43";
const QString PCM_DATA = "41";
const QString ABS_DTC = "43";
const QString ABS_DATA = "41";

// Common successful responses
const QString OK = "OK";
const QString BUS_INIT_OK = "BUS INIT: OK";
const QString ELM327_ID = "ELM327";
} // End Responses namespace

// Enhanced error codes for both protocols
const QStringList ALL_ERROR_CODES = {
    "UNABLE TO CONNECT", "BUS BUSY", "BUS ERROR", "BUFFER FULL", "CAN ERROR",
    "DATA ERROR", "ERROR", "STOPPED", "TIMEOUT", "SEARCH", "SEARCHING",
    "NODATA", "NO DATA", "7F 27", "7F 31", "7F 21", "NEGATIVE RESPONSE",
    "BUS INIT: ERROR", "UNABLETOCONNECT", "NO RESPONSE", "PROTOCOL ERROR",
    "CHECKSUM ERROR", "FRAMING ERROR", "OVERFLOW", "PARITY ERROR"
};

const QStringList KWP2000_ERROR_CODES = {
    "7F 27", "7F 31", "7F 21", "BUS INIT: ERROR", "UNABLE TO CONNECT",
    "NO RESPONSE", "TIMEOUT", "CHECKSUM ERROR"
};

const QStringList J1850_ERROR_CODES = {
    "BUS BUSY", "BUS ERROR", "NO DATA", "BUFFER FULL", "PROTOCOL ERROR",
    "FRAMING ERROR", "OVERFLOW", "PARITY ERROR"
};

// Protocol validation helpers
namespace Validation {
inline bool isKWP2000Response(const QString& response) {
    return response.startsWith("61") || response.startsWith("67") ||
           response.startsWith("71") || response.startsWith("C1");
}

inline bool isOBDResponse(const QString& response) {
    return response.startsWith("41") || response.startsWith("43") ||
           response.startsWith("44") || response.startsWith("47");
}

inline bool isErrorResponse(const QString& response) {
    return ALL_ERROR_CODES.contains(response.toUpper()) ||
           response.startsWith("7F");
}
} // End Validation namespace
} // End WJ namespace

// FIXED: Complete initialization sequence with proper ELM327 setup
QList<WJCommand> WJCommands::getInitSequence(WJProtocol protocol) {
    QList<WJCommand> commands;

    if (protocol == PROTOCOL_ISO_14230_4_KWP_FAST) {
        // STEP 1: Basic ELM327 setup - CRITICAL FOUNDATION
        commands.append(WJCommand("ATZ", "ELM327", "Reset ELM327", 7500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));
        commands.append(WJCommand("ATE0", "OK", "Echo off", 1500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));
        commands.append(WJCommand("ATL0", "OK", "Linefeed off", 1000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));
        commands.append(WJCommand("ATH0", "OK", "Headers off", 1000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));
        commands.append(WJCommand("ATS0", "OK", "Spaces off", 1000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));

        // STEP 2: Protocol setup - MUST come before ECU-specific commands
        commands.append(WJCommand("ATSP5", "OK", "Set protocol KWP2000 Fast", 2000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));

        // STEP 3: ECU-specific setup - NOW wakeup and header will work
        commands.append(WJCommand("ATWM8115F13E", "OK", "Set wakeup message for EDC15", 1500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));
        commands.append(WJCommand("ATSH8115F1", "OK", "Set header for EDC15", 1500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));

        // STEP 4: Fast initialization - THIS SHOULD NOW WORK
        commands.append(WJCommand("ATFI", "BUS INIT: OK", "Fast initialization", 4000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, true));

        // STEP 5: Verify protocol is active
        commands.append(WJCommand("ATDP", "ISO 14230-4", "Verify protocol", 1500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, false));

        // STEP 6: ECU communication sequence - exactly as in WJdiag
        commands.append(WJCommand("81", "C1 EF 8F", "Start communication", 3000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, false));
        commands.append(WJCommand("27 01", "67 01", "Security access request", 3000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, false));
        commands.append(WJCommand("27 02 CD 46", "7F 27", "Security access key", 3000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, false));
        commands.append(WJCommand("31 25 00", "71 25", "Start diagnostic routine", 3000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15, false));
    }
    else if (protocol == PROTOCOL_J1850_VPW) {
        // J1850 VPW initialization for Transmission, PCM, ABS, etc.
        commands.append(WJCommand("ATZ", "ELM327", "Reset ELM327", 7500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, true));
        commands.append(WJCommand("ATE0", "OK", "Echo off", 1500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, true));
        commands.append(WJCommand("ATL0", "OK", "Linefeed off", 1000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));
        commands.append(WJCommand("ATH1", "OK", "Headers on", 1000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));
        commands.append(WJCommand("ATS0", "OK", "Spaces off", 1000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));
        commands.append(WJCommand("ATST32", "OK", "Set timeout for J1850", 1000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));
        commands.append(WJCommand("ATSP1", "OK", "Set protocol J1850 VPW", 2000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, true));
        commands.append(WJCommand("ATDP", "J1850 VPW", "Verify protocol", 1500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));

        // J1850 doesn't use wakeup messages or fast init like KWP2000
        commands.append(WJCommand("ATMA", "", "Monitor all messages", 3000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION, false));
    }

    return commands;
}

QList<WJCommand> WJCommands::getProtocolSwitchCommands(WJProtocol fromProtocol, WJProtocol toProtocol) {
    QList<WJCommand> commands;
    if (fromProtocol == toProtocol) {
        return commands; // No switch needed
    }

    // Always reset when switching protocols
    commands.append(WJCommand("ATZ", "ELM327", "Reset for protocol switch", 5000, toProtocol, MODULE_UNKNOWN));
    commands.append(WJCommand("ATE0", "OK", "Echo off", 1000, toProtocol, MODULE_UNKNOWN));
    commands.append(WJCommand("ATL0", "OK", "Linefeed off", 500, toProtocol, MODULE_UNKNOWN));
    commands.append(WJCommand("ATS0", "OK", "Spaces off", 500, toProtocol, MODULE_UNKNOWN));

    if (toProtocol == PROTOCOL_ISO_14230_4_KWP_FAST) {
        commands.append(WJCommand("ATSP5", "OK", "Switch to KWP2000 Fast", 2000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15));
        commands.append(WJCommand("ATH0", "OK", "Headers off for KWP", 500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15));
    }
    else if (toProtocol == PROTOCOL_J1850_VPW) {
        commands.append(WJCommand("ATSP1", "OK", "Switch to J1850 VPW", 2000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));
        commands.append(WJCommand("ATH1", "OK", "Headers on for J1850", 500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));
        commands.append(WJCommand("ATST32", "OK", "Set J1850 timeout", 500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));
    }

    return commands;
}

QList<WJCommand> WJCommands::getModuleInitCommands(WJModule module) {
    QList<WJCommand> commands;
    WJProtocol protocol = WJUtils::getProtocolFromModule(module);

    switch (module) {
    case MODULE_ENGINE_EDC15:
        // EDC15 Engine - uses KWP2000 Fast with specific addresses
        commands.append(WJCommand("ATWM8115F13E", "OK", "Set EDC15 wakeup message", 1000, protocol, module));
        commands.append(WJCommand("ATSH8115F1", "OK", "Set EDC15 header", 1000, protocol, module));
        break;

    case MODULE_TRANSMISSION:
        // Transmission - ALSO uses KWP2000 Fast (not J1850 VPW)
        commands.append(WJCommand("ATWM8118F13E", "OK", "Set TCM wakeup message", 1000, protocol, module));
        commands.append(WJCommand("ATSH8118F1", "OK", "Set TCM header", 1000, protocol, module));
        break;

    case MODULE_PCM:
        // PCM - typically J1850 VPW
        if (protocol == PROTOCOL_J1850_VPW) {
            commands.append(WJCommand("ATMA", "", "Monitor all for PCM", 2000, protocol, module));
        }
        break;

    case MODULE_ABS:
        // ABS - typically J1850 VPW
        if (protocol == PROTOCOL_J1850_VPW) {
            commands.append(WJCommand("ATMA", "", "Monitor all for ABS", 2000, protocol, module));
        }
        break;

    default:
        break;
    }

    return commands;
}

// Additional helper function for complete module connection
QList<WJCommand> WJCommands::getCompleteModuleConnection(WJModule module) {
    QList<WJCommand> commands;
    WJProtocol protocol = WJUtils::getProtocolFromModule(module);

    if (module == MODULE_ENGINE_EDC15) {
        // Complete EDC15 connection sequence
        commands.append(WJCommand("ATWM8115F13E", "OK", "Set EDC15 wakeup message", 1000, protocol, module));
        commands.append(WJCommand("ATSH8115F1", "OK", "Set EDC15 header", 1000, protocol, module));
        commands.append(WJCommand("ATFI", "BUS INIT: OK", "Fast initialization", 3000, protocol, module));

        // Communication sequence
        commands.append(WJCommand("81", "C1 EF 8F", "Start communication", 3000, protocol, module));
        commands.append(WJCommand("27 01", "67 01", "Security access request", 3000, protocol, module));
        commands.append(WJCommand("27 02 CD 46", "7F 27", "Security access key", 3000, protocol, module));
        commands.append(WJCommand("31 25 00", "71 25", "Start diagnostic routine", 3000, protocol, module));
    }

    return commands;
}

QList<WJCommand> WJCommands::getDiagnosticCommands(WJModule module) {
    QList<WJCommand> commands;
    WJProtocol protocol = WJUtils::getProtocolFromModule(module);

    switch (module) {
    case MODULE_ENGINE_EDC15:
        // KWP2000 diagnostic commands for EDC15
        commands.append(WJCommand(WJ::Engine::READ_DTC, WJ::Responses::ENGINE_DTC, "Read Engine DTCs", 3000, protocol, module));
        commands.append(WJCommand(WJ::Engine::READ_MAF_DATA, WJ::Responses::ENGINE_MAF, "Read MAF Data", 2000, protocol, module));
        commands.append(WJCommand(WJ::Engine::READ_RAIL_PRESSURE_ACTUAL, WJ::Responses::ENGINE_RAIL_PRESSURE, "Read Rail Pressure", 2000, protocol, module));
        commands.append(WJCommand(WJ::Engine::READ_INJECTOR_DATA, WJ::Responses::ENGINE_INJECTOR, "Read Injector Data", 2000, protocol, module));
        commands.append(WJCommand(WJ::Engine::READ_MISC_DATA, WJ::Responses::ENGINE_RAIL_PRESSURE, "Read Misc Data", 2000, protocol, module));
        break;

    case MODULE_TRANSMISSION:
        commands.append(WJCommand(WJ::Transmission::READ_DTC, WJ::Responses::TRANS_DTC, "Read Transmission DTCs", 3000, protocol, module));
        commands.append(WJCommand(WJ::Transmission::READ_TRANS_DATA, WJ::Responses::TRANS_DATA, "Read Transmission Data", 2000, protocol, module));
        commands.append(WJCommand(WJ::Transmission::READ_GEAR_RATIO, WJ::Responses::TRANS_DATA, "Read Gear Ratio", 2000, protocol, module));
        break;

    case MODULE_PCM:
        commands.append(WJCommand(WJ::PCM::READ_DTC, WJ::Responses::PCM_DTC, "Read PCM DTCs", 3000, protocol, module));
        commands.append(WJCommand(WJ::PCM::READ_LIVE_DATA, WJ::Responses::PCM_DATA, "Read Live Data", 2000, protocol, module));
        commands.append(WJCommand(WJ::PCM::READ_FUEL_TRIM, WJ::Responses::PCM_DATA, "Read Fuel Trim", 2000, protocol, module));
        break;

    case MODULE_ABS:
        commands.append(WJCommand(WJ::ABS::READ_DTC, WJ::Responses::ABS_DTC, "Read ABS DTCs", 3000, protocol, module));
        commands.append(WJCommand(WJ::ABS::READ_WHEEL_SPEEDS, WJ::Responses::ABS_DATA, "Read Wheel Speeds", 2000, protocol, module));
        break;

    default:
        break;
    }

    return commands;
}

WJModuleConfig WJCommands::getModuleConfig(WJModule module) {
    WJModuleConfig config;
    config.module = module;

    switch (module) {
    case MODULE_ENGINE_EDC15:
        config.protocol = PROTOCOL_ISO_14230_4_KWP_FAST;
        config.ecuHeader = WJ::Headers::ENGINE_EDC15;
        config.wakeupMessage = WJ::WakeupMessages::ENGINE_EDC15;
        config.defaultTimeout = 2000;
        break;

    case MODULE_TRANSMISSION:
        config.protocol = PROTOCOL_J1850_VPW;
        config.ecuHeader = WJ::Headers::TRANSMISSION;
        config.wakeupMessage = WJ::WakeupMessages::TRANSMISSION;
        config.defaultTimeout = 1000;
        break;

    case MODULE_PCM:
        config.protocol = PROTOCOL_J1850_VPW;
        config.ecuHeader = WJ::Headers::PCM;
        config.wakeupMessage = WJ::WakeupMessages::PCM;
        config.defaultTimeout = 1000;
        break;

    case MODULE_ABS:
        config.protocol = PROTOCOL_J1850_VPW;
        config.ecuHeader = WJ::Headers::ABS;
        config.defaultTimeout = 1000;
        break;

    default:
        config.protocol = PROTOCOL_UNKNOWN;
        config.defaultTimeout = 1000;
        break;
    }

    return config;
}

QList<WJCommand> WJCommands::getProtocolDetectionCommands() {
    QList<WJCommand> commands;

    // Try ISO_14230_4_KWP_FAST first (engine)
    commands.append(WJCommand("ATSP5", "OK", "Try ISO_14230_4_KWP_FAST", 1000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15));
    commands.append(WJCommand("ATSH" + WJ::Headers::ENGINE_EDC15, "OK", "Set engine header for test", 500, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15));
    commands.append(WJCommand("81", "C1", "Test engine communication", 2000, PROTOCOL_ISO_14230_4_KWP_FAST, MODULE_ENGINE_EDC15));

    // Try J1850 VPW (transmission/other modules)
    commands.append(WJCommand("ATSP1", "OK", "Try J1850 VPW", 1000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));
    commands.append(WJCommand("ATSH" + WJ::Headers::TRANSMISSION, "OK", "Set transmission header for test", 500, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));
    commands.append(WJCommand("0100", "41", "Test transmission communication", 2000, PROTOCOL_J1850_VPW, MODULE_TRANSMISSION));

    return commands;
}

// Enhanced utility functions implementation
namespace WJUtils {

WJProtocol detectProtocol(const QString& response) {
    if (response.contains("C1") || response.contains("67")) {
        return PROTOCOL_ISO_14230_4_KWP_FAST; // Engine EDC15 response
    }
    else if (response.contains("41") || response.contains("43")) {
        return PROTOCOL_J1850_VPW; // J1850 response
    }
    return PROTOCOL_UNKNOWN;
}

bool isProtocolAvailable(WJProtocol protocol) {
    return (protocol == PROTOCOL_ISO_14230_4_KWP_FAST || protocol == PROTOCOL_J1850_VPW);
}

QString getProtocolName(WJProtocol protocol) {
    switch (protocol) {
    case PROTOCOL_ISO_14230_4_KWP_FAST: return "ISO_14230_4_KWP_FAST";
    case PROTOCOL_J1850_VPW: return "J1850 VPW";
    case PROTOCOL_AUTO_DETECT: return "Auto Detect";
    default: return "Unknown";
    }
}

QString getModuleName(WJModule module) {
    switch (module) {
    case MODULE_ENGINE_EDC15: return "Engine (EDC15)";
    case MODULE_TRANSMISSION: return "Transmission";
    case MODULE_PCM: return "PCM";
    case MODULE_ABS: return "ABS";
    case MODULE_AIRBAG: return "Airbag";
    case MODULE_HVAC: return "HVAC";
    case MODULE_BODY: return "Body Control";
    case MODULE_RADIO: return "Radio";
    default: return "Unknown";
    }
}

double convertTemperature(int rawValue) {
    return (rawValue / 10.0) - 273.15;
}

double convertPressure(int rawValue) {
    return rawValue / 10.0;
}

double convertPercentage(int rawValue) {
    return rawValue / 100.0;
}

double convertSpeed(int rawValue) {
    return rawValue * 0.1;
}

double convertVoltage(int rawValue) {
    return rawValue / 1000.0;
}

QString formatDTCCode(int byte1, int byte2, WJProtocol protocol) {
    QString prefix;
    int firstDigit = (byte1 >> 6) & 0x03;

    switch (firstDigit) {
    case 0: prefix = "P0"; break;
    case 1: prefix = "P1"; break;
    case 2: prefix = "P2"; break;
    case 3: prefix = "P3"; break;
    default: prefix = "P0"; break;
    }

    int secondDigit = (byte1 >> 4) & 0x03;
    int thirdDigit = byte1 & 0x0F;
    int fourthDigit = (byte2 >> 4) & 0x0F;
    int fifthDigit = byte2 & 0x0F;

    return QString("%1%2%3%4%5")
        .arg(prefix)
        .arg(secondDigit)
        .arg(thirdDigit, 1, 16, QChar('0')).toUpper()
        .arg(fourthDigit, 1, 16, QChar('0')).toUpper()
        .arg(fifthDigit, 1, 16, QChar('0')).toUpper();
}

bool isValidHexData(const QString& data) {
    QString trimmed = data.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QRegularExpression hexPattern("^[0-9A-Fa-f\\s]+$");
    return hexPattern.match(trimmed).hasMatch() && trimmed.length() >= 2;
}

bool isValidResponse(const QString& response, WJProtocol protocol) {
    QString cleaned = cleanData(response, protocol);
    QString upper = cleaned.toUpper();

    if (upper.contains("OK") || upper.contains("ELM327") || upper.contains("BUS INIT") ||
        upper.contains("ISO_14230_4_KWP_FAST") || upper.contains("J1850") ||
        upper.contains("CAN") || upper.contains("AUTO") || cleaned.trimmed() == ".") {
        return true;
    }

    if (protocol == PROTOCOL_ISO_14230_4_KWP_FAST) {
        return upper.contains("C1") || upper.contains("67") || upper.contains("61") ||
               upper.contains("43") || upper.contains("7F") || upper.contains("83") ||
               upper.contains("C7") || isValidHexData(cleaned);
    }
    else if (protocol == PROTOCOL_J1850_VPW) {
        return upper.contains("41") || upper.contains("43") || upper.contains("7F") ||
               isValidHexData(cleaned);
    }

    return false;
}

bool isError(const QString& response, WJProtocol protocol) {
    if (response.isEmpty()) {
        return false;
    }

    QString upperResponse = response.toUpper();

    if (protocol == PROTOCOL_ISO_14230_4_KWP_FAST) {
        for (const QString& error : WJ::KWP2000_ERROR_CODES) {
            if (upperResponse.contains(error)) {
                return true;
            }
        }
    }
    else if (protocol == PROTOCOL_J1850_VPW) {
        for (const QString& error : WJ::J1850_ERROR_CODES) {
            if (upperResponse.contains(error)) {
                return true;
            }
        }
    }

    for (const QString& error : WJ::ALL_ERROR_CODES) {
        if (upperResponse.contains(error)) {
            return true;
        }
    }

    return false;
}

QString cleanData(const QString& input, WJProtocol protocol) {
    QString cleaned = input.trimmed().simplified().toUpper();
    cleaned.remove(QRegularExpression("[\\n\\t\\r]"));
    cleaned.remove(">");
    cleaned.remove("?");
    cleaned.remove(QChar(0xFFFD));
    cleaned.remove(QChar(0x00));

    if (protocol == PROTOCOL_J1850_VPW) {
        cleaned.remove(",");
    }

    return cleaned;
}

QList<int> parseHexBytes(const QString& data) {
    QList<int> bytes;
    QStringList parts = data.split(" ", Qt::SkipEmptyParts);

    for (const QString& part : parts) {
        if (part.length() == 2) {
            bool ok;
            int byte = part.toInt(&ok, 16);
            if (ok) {
                bytes.append(byte);
            }
        }
    }

    return bytes;
}

int bytesToInt16(int highByte, int lowByte) {
    return (highByte << 8) | lowByte;
}

QString getTimestamp() {
    return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
}

WJModule getModuleFromHeader(const QString& header) {
    if (header.contains(WJ::Headers::ENGINE_EDC15)) return MODULE_ENGINE_EDC15;
    if (header.contains(WJ::Headers::TRANSMISSION)) return MODULE_TRANSMISSION;
    if (header.contains(WJ::Headers::PCM)) return MODULE_PCM;
    if (header.contains(WJ::Headers::ABS)) return MODULE_ABS;
    if (header.contains(WJ::Headers::AIRBAG)) return MODULE_AIRBAG;
    if (header.contains(WJ::Headers::HVAC)) return MODULE_HVAC;
    if (header.contains(WJ::Headers::BODY)) return MODULE_BODY;
    return MODULE_UNKNOWN;
}

WJProtocol getProtocolFromModule(WJModule module) {
    switch (module) {
    case MODULE_ENGINE_EDC15:
        return PROTOCOL_ISO_14230_4_KWP_FAST;
    case MODULE_TRANSMISSION:
    case MODULE_PCM:
    case MODULE_ABS:
    case MODULE_AIRBAG:
    case MODULE_HVAC:
    case MODULE_BODY:
    case MODULE_RADIO:
        return PROTOCOL_J1850_VPW;
    default:
        return PROTOCOL_UNKNOWN;
    }
}

} // namespace WJUtils

// Enhanced DTC database implementation
namespace WJ_DTCs {

static const QHash<QString, QString> ENGINE_DTC_DESCRIPTIONS = {
    {"P0016", "Crankshaft/Camshaft Position Correlation"},
    {"P0087", "Fuel Rail/System Pressure Too Low"},
    {"P0088", "Fuel Rail/System Pressure Too High"},
    {"P0089", "Fuel Pressure Regulator Performance"},
    {"P0180", "Fuel Temperature Sensor Circuit"},
    {"P0201", "Injector Circuit/Open - Cylinder 1"},
    {"P0202", "Injector Circuit/Open - Cylinder 2"},
    {"P0203", "Injector Circuit/Open - Cylinder 3"},
    {"P0204", "Injector Circuit/Open - Cylinder 4"},
    {"P0205", "Injector Circuit/Open - Cylinder 5"},
    {"P0234", "Engine Over Boost Condition"},
    {"P0235", "Turbocharger Boost Sensor Circuit"},
    {"P0299", "Turbocharger Underboost Condition"},
    {"P0335", "Crankshaft Position Sensor Circuit"},
    {"P0340", "Camshaft Position Sensor Circuit"},
    {"P0380", "Glow Plug/Heater Circuit"},
    {"P0401", "Exhaust Gas Recirculation Flow Insufficient"},
    {"P0562", "System Voltage Low"},
    {"P0563", "System Voltage High"}
};

static const QHash<QString, QString> TRANSMISSION_DTC_DESCRIPTIONS = {
    {"P0700", "Transmission Control System"},
    {"P0701", "Transmission Control System Range/Performance"},
    {"P0702", "Transmission Control System Electrical"},
    {"P0703", "Torque Converter/Brake Switch B Circuit"},
    {"P0706", "Transmission Range Sensor Circuit Range/Performance"},
    {"P0711", "Transmission Fluid Temperature Sensor Circuit Range/Performance"},
    {"P0712", "Transmission Fluid Temperature Sensor Circuit Low"},
    {"P0713", "Transmission Fluid Temperature Sensor Circuit High"},
    {"P0715", "Input/Turbine Speed Sensor Circuit"},
    {"P0720", "Output Speed Sensor Circuit"},
    {"P0725", "Engine Speed Input Circuit"},
    {"P0731", "Gear 1 Incorrect Ratio"},
    {"P0732", "Gear 2 Incorrect Ratio"},
    {"P0733", "Gear 3 Incorrect Ratio"},
    {"P0734", "Gear 4 Incorrect Ratio"},
    {"P0740", "Torque Converter Clutch Circuit"},
    {"P0743", "Torque Converter Clutch Circuit Electrical"},
    {"P0750", "Shift Solenoid A"},
    {"P0755", "Shift Solenoid B"},
    {"P1740", "Torque Converter Clutch System Stuck Off"},
    {"P1765", "Transmission Relay"},
    {"P1899", "Park/Neutral Position Switch Stuck in Park or in Gear"}
};

static const QHash<QString, QString> PCM_DTC_DESCRIPTIONS = {
    {"P0100", "Mass or Volume Air Flow Circuit"},
    {"P0105", "Manifold Absolute Pressure/Barometric Pressure Circuit"},
    {"P0110", "Intake Air Temperature Circuit"},
    {"P0115", "Engine Coolant Temperature Circuit"},
    {"P0120", "Throttle/Pedal Position Sensor/Switch A Circuit"},
    {"P0125", "Insufficient Coolant Temperature for Closed Loop Fuel Control"},
    {"P0130", "O2 Circuit (Bank 1, Sensor 1)"},
    {"P0135", "O2 Sensor Heater Circuit (Bank 1, Sensor 1)"},
    {"P0140", "O2 Circuit (Bank 1, Sensor 2)"},
    {"P0171", "System Too Lean (Bank 1)"},
    {"P0172", "System Too Rich (Bank 1)"},
    {"P0300", "Random/Multiple Cylinder Misfire Detected"},
    {"P0301", "Cylinder 1 Misfire Detected"},
    {"P0302", "Cylinder 2 Misfire Detected"},
    {"P0303", "Cylinder 3 Misfire Detected"},
    {"P0304", "Cylinder 4 Misfire Detected"},
    {"P0305", "Cylinder 5 Misfire Detected"},
    {"P0306", "Cylinder 6 Misfire Detected"},
    {"P0420", "Catalyst System Efficiency Below Threshold"},
    {"P0440", "Evaporative Emission Control System"},
    {"P0500", "Vehicle Speed Sensor"},
    {"P1689", "No Communication with TCM"}
};

static const QHash<QString, QString> ABS_DTC_DESCRIPTIONS = {
    {"C1200", "ABS Pump Motor Circuit"},
    {"C1201", "ABS Pump Motor Relay Circuit"},
    {"C1210", "ABS Inlet Valve Circuit"},
    {"C1215", "ABS Outlet Valve Circuit"},
    {"C1220", "Front Left Wheel Speed Sensor Circuit"},
    {"C1225", "Front Right Wheel Speed Sensor Circuit"},
    {"C1230", "Rear Left Wheel Speed Sensor Circuit"},
    {"C1235", "Rear Right Wheel Speed Sensor Circuit"},
    {"C1240", "ABS System Relay Circuit"},
    {"C1250", "ABS Control Module Internal"},
    {"C1260", "ABS Hydraulic Unit Internal"},
    {"C1270", "ABS System Communication"},
    {"C1280", "Brake Fluid Level Low"},
    {"C1290", "ABS Warning Lamp Circuit"}
};

static const QStringList CRITICAL_ENGINE_DTCS = {
    "P0087", "P0088", "P0234", "P0299", "P0335", "P0340", "P0562", "P0563"
};

static const QStringList CRITICAL_TRANSMISSION_DTCS = {
    "P0700", "P0711", "P0715", "P0720", "P1765"
};

static const QStringList CRITICAL_ABS_DTCS = {
    "C1200", "C1250", "C1260", "C1280"
};

QString getDTCDescription(const QString& dtcCode, WJModule module) {
    switch (module) {
    case MODULE_ENGINE_EDC15:
        return ENGINE_DTC_DESCRIPTIONS.value(dtcCode, "Unknown Engine DTC");
    case MODULE_TRANSMISSION:
        return TRANSMISSION_DTC_DESCRIPTIONS.value(dtcCode, "Unknown Transmission DTC");
    case MODULE_PCM:
        return PCM_DTC_DESCRIPTIONS.value(dtcCode, "Unknown PCM DTC");
    case MODULE_ABS:
        return ABS_DTC_DESCRIPTIONS.value(dtcCode, "Unknown ABS DTC");
    default:
        QString desc = ENGINE_DTC_DESCRIPTIONS.value(dtcCode);
        if (!desc.isEmpty()) return desc;
        desc = TRANSMISSION_DTC_DESCRIPTIONS.value(dtcCode);
        if (!desc.isEmpty()) return desc;
        desc = PCM_DTC_DESCRIPTIONS.value(dtcCode);
        if (!desc.isEmpty()) return desc;
        return ABS_DTC_DESCRIPTIONS.value(dtcCode, "Unknown DTC Code");
    }
}

QStringList getKnownDTCs(WJModule module) {
    switch (module) {
    case MODULE_ENGINE_EDC15: return ENGINE_DTC_DESCRIPTIONS.keys();
    case MODULE_TRANSMISSION: return TRANSMISSION_DTC_DESCRIPTIONS.keys();
    case MODULE_PCM: return PCM_DTC_DESCRIPTIONS.keys();
    case MODULE_ABS: return ABS_DTC_DESCRIPTIONS.keys();
    default: return QStringList();
    }
}

bool isCriticalDTC(const QString& dtcCode, WJModule module) {
    switch (module) {
    case MODULE_ENGINE_EDC15: return CRITICAL_ENGINE_DTCS.contains(dtcCode);
    case MODULE_TRANSMISSION: return CRITICAL_TRANSMISSION_DTCS.contains(dtcCode);
    case MODULE_ABS: return CRITICAL_ABS_DTCS.contains(dtcCode);
    default:
        return CRITICAL_ENGINE_DTCS.contains(dtcCode) ||
               CRITICAL_TRANSMISSION_DTCS.contains(dtcCode) ||
               CRITICAL_ABS_DTCS.contains(dtcCode);
    }
}

QStringList getEngineSpecificDTCs() {
    return ENGINE_DTC_DESCRIPTIONS.keys();
}

QStringList getTransmissionSpecificDTCs() {
    return TRANSMISSION_DTC_DESCRIPTIONS.keys();
}

QStringList getPCMSpecificDTCs() {
    return PCM_DTC_DESCRIPTIONS.keys();
}

QStringList getABSSpecificDTCs() {
    return ABS_DTC_DESCRIPTIONS.keys();
}

} // namespace WJ_DTCs

// Enhanced data parser implementation
bool WJDataParser::parseEngineMAFData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ENGINE_MAF)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 8) {
        return false;
    }

    try {
        sensorData.engine.mafActual = bytes[6];
        sensorData.engine.mafSpecified = bytes[7];
        sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.engine.dataValid = true;
        sensorData.currentProtocol = PROTOCOL_ISO_14230_4_KWP_FAST;
        sensorData.activeModule = MODULE_ENGINE_EDC15;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseEngineRailPressureData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ENGINE_RAIL_PRESSURE)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 12) {
        return false;
    }

    try {
        int railActualRaw = WJUtils::bytesToInt16(bytes[10], bytes[11]);
        sensorData.engine.railPressureActual = WJUtils::convertPressure(railActualRaw);
        sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.engine.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseEngineMAPData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ENGINE_RAIL_PRESSURE)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 10) {
        return false;
    }

    try {
        int mapActualRaw = WJUtils::bytesToInt16(bytes[8], bytes[9]);
        sensorData.engine.mapActual = mapActualRaw;
        sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.engine.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseEngineInjectorData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ENGINE_INJECTOR)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 14) {
        return false;
    }

    try {
        int rpmRaw = WJUtils::bytesToInt16(bytes[2], bytes[3]);
        sensorData.engine.engineRPM = rpmRaw;

        int iqRaw = WJUtils::bytesToInt16(bytes[4], bytes[5]);
        sensorData.engine.injectionQuantity = WJUtils::convertPercentage(iqRaw);

        if (bytes.size() >= 28) {
            int inj1Raw = WJUtils::bytesToInt16(bytes[18], bytes[19]);
            sensorData.engine.injector1Correction = (inj1Raw - 32768) / 100.0;

            int inj2Raw = WJUtils::bytesToInt16(bytes[20], bytes[21]);
            sensorData.engine.injector2Correction = (inj2Raw - 32768) / 100.0;

            int inj3Raw = WJUtils::bytesToInt16(bytes[22], bytes[23]);
            sensorData.engine.injector3Correction = (inj3Raw - 32768) / 100.0;

            int inj4Raw = WJUtils::bytesToInt16(bytes[24], bytes[25]);
            sensorData.engine.injector4Correction = (inj4Raw - 32768) / 100.0;

            if (bytes.size() >= 28) {
                int inj5Raw = WJUtils::bytesToInt16(bytes[26], bytes[27]);
                sensorData.engine.injector5Correction = (inj5Raw - 32768) / 100.0;
            }
        }

        sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.engine.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseEngineMiscData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ENGINE_RAIL_PRESSURE)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 16) {
        return false;
    }

    try {
        int coolantRaw = WJUtils::bytesToInt16(bytes[2], bytes[3]);
        sensorData.engine.coolantTemp = WJUtils::convertTemperature(coolantRaw);

        int iatRaw = WJUtils::bytesToInt16(bytes[4], bytes[5]);
        sensorData.engine.intakeAirTemp = WJUtils::convertTemperature(iatRaw);

        int tpsRaw = WJUtils::bytesToInt16(bytes[14], bytes[15]);
        sensorData.engine.throttlePosition = WJUtils::convertPercentage(tpsRaw);

        sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.engine.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseEngineBatteryVoltage(const QString& data, WJSensorData& sensorData) {
    if (!data.contains("V")) {
        return false;
    }

    try {
        QString voltageStr = data;
        voltageStr.remove("V").remove(" ").remove("\r").remove("\n");

        bool ok;
        double voltage = voltageStr.toDouble(&ok);
        if (ok && voltage > 0.0 && voltage < 30.0) {
            sensorData.engine.batteryVoltage = voltage;
            sensorData.engine.lastUpdate = QDateTime::currentMSecsSinceEpoch();
            sensorData.engine.dataValid = true;
            return true;
        }
    } catch (...) {
        return false;
    }

    return false;
}

// Transmission data parsing (J1850)
bool WJDataParser::parseTransmissionData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::TRANS_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 8) {
        return false;
    }

    try {
        if (bytes.size() > 3) {
            int tempRaw = bytes[3];
            sensorData.transmission.oilTemp = tempRaw - 40;
        }

        if (bytes.size() > 4) {
            sensorData.transmission.currentGear = bytes[4] & 0x0F;
        }

        if (bytes.size() > 6) {
            int pressureRaw = WJUtils::bytesToInt16(bytes[5], bytes[6]);
            sensorData.transmission.linePresssure = pressureRaw * 0.1;
        }

        sensorData.transmission.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.transmission.dataValid = true;
        sensorData.currentProtocol = PROTOCOL_J1850_VPW;
        sensorData.activeModule = MODULE_TRANSMISSION;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseTransmissionSpeeds(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::TRANS_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 8) {
        return false;
    }

    try {
        if (bytes.size() > 4) {
            int inputSpeedRaw = WJUtils::bytesToInt16(bytes[2], bytes[3]);
            sensorData.transmission.inputSpeed = inputSpeedRaw;
        }

        if (bytes.size() > 6) {
            int outputSpeedRaw = WJUtils::bytesToInt16(bytes[4], bytes[5]);
            sensorData.transmission.outputSpeed = outputSpeedRaw;
        }

        sensorData.transmission.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.transmission.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseTransmissionSolenoids(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::TRANS_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 6) {
        return false;
    }

    try {
        if (bytes.size() > 3) {
            sensorData.transmission.shiftSolenoidA = WJUtils::convertPercentage(bytes[3]);
        }

        if (bytes.size() > 4) {
            sensorData.transmission.shiftSolenoidB = WJUtils::convertPercentage(bytes[4]);
        }

        if (bytes.size() > 5) {
            sensorData.transmission.tccSolenoid = WJUtils::convertPercentage(bytes[5]);
        }

        sensorData.transmission.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.transmission.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

// PCM data parsing (J1850)
bool WJDataParser::parsePCMData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::PCM_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 8) {
        return false;
    }

    try {
        if (bytes.size() > 3) {
            sensorData.pcm.vehicleSpeed = bytes[3];
        }

        if (bytes.size() > 4) {
            sensorData.pcm.engineLoad = WJUtils::convertPercentage(bytes[4]);
        }

        if (bytes.size() > 6) {
            sensorData.pcm.barometricPressure = bytes[6];
        }

        sensorData.pcm.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.pcm.dataValid = true;
        sensorData.currentProtocol = PROTOCOL_J1850_VPW;
        sensorData.activeModule = MODULE_PCM;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parsePCMFuelTrim(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::PCM_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 6) {
        return false;
    }

    try {
        if (bytes.size() > 3) {
            sensorData.pcm.fuelTrimST = (bytes[3] - 128) * 100.0 / 128.0;
        }

        if (bytes.size() > 4) {
            sensorData.pcm.fuelTrimLT = (bytes[4] - 128) * 100.0 / 128.0;
        }

        sensorData.pcm.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.pcm.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parsePCMO2Sensors(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::PCM_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 6) {
        return false;
    }

    try {
        if (bytes.size() > 3) {
            sensorData.pcm.o2Sensor1 = bytes[3] * 0.005;
        }

        if (bytes.size() > 4) {
            sensorData.pcm.o2Sensor2 = bytes[4] * 0.005;
        }

        sensorData.pcm.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.pcm.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

// ABS data parsing (J1850)
bool WJDataParser::parseABSWheelSpeeds(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ABS_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 10) {
        return false;
    }

    try {
        if (bytes.size() >= 10) {
            sensorData.abs.wheelSpeedFL = WJUtils::bytesToInt16(bytes[2], bytes[3]) * 0.1;
            sensorData.abs.wheelSpeedFR = WJUtils::bytesToInt16(bytes[4], bytes[5]) * 0.1;
            sensorData.abs.wheelSpeedRL = WJUtils::bytesToInt16(bytes[6], bytes[7]) * 0.1;
            sensorData.abs.wheelSpeedRR = WJUtils::bytesToInt16(bytes[8], bytes[9]) * 0.1;
        }

        sensorData.abs.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.abs.dataValid = true;
        sensorData.currentProtocol = PROTOCOL_J1850_VPW;
        sensorData.activeModule = MODULE_ABS;
        return true;
    } catch (...) {
        return false;
    }
}

bool WJDataParser::parseABSStabilityData(const QString& data, WJSensorData& sensorData) {
    if (!data.startsWith(WJ::Responses::ABS_DATA)) {
        return false;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 8) {
        return false;
    }

    try {
        if (bytes.size() > 4) {
            int yawRaw = WJUtils::bytesToInt16(bytes[3], bytes[4]);
            sensorData.abs.yawRate = (yawRaw - 32768) * 0.1;
        }

        if (bytes.size() > 6) {
            int accelRaw = WJUtils::bytesToInt16(bytes[5], bytes[6]);
            sensorData.abs.lateralAccel = (accelRaw - 32768) * 0.01;
        }

        sensorData.abs.lastUpdate = QDateTime::currentMSecsSinceEpoch();
        sensorData.abs.dataValid = true;
        return true;
    } catch (...) {
        return false;
    }
}

// Fault code parsing for all modules
QList<WJ_DTC> WJDataParser::parseEngineFaultCodes(const QString& data) {
    return parseGenericFaultCodes(data, MODULE_ENGINE_EDC15, PROTOCOL_ISO_14230_4_KWP_FAST);
}

QList<WJ_DTC> WJDataParser::parseTransmissionFaultCodes(const QString& data) {
    return parseGenericFaultCodes(data, MODULE_TRANSMISSION, PROTOCOL_J1850_VPW);
}

QList<WJ_DTC> WJDataParser::parsePCMFaultCodes(const QString& data) {
    return parseGenericFaultCodes(data, MODULE_PCM, PROTOCOL_J1850_VPW);
}

QList<WJ_DTC> WJDataParser::parseABSFaultCodes(const QString& data) {
    return parseGenericFaultCodes(data, MODULE_ABS, PROTOCOL_J1850_VPW);
}

QList<WJ_DTC> WJDataParser::parseGenericFaultCodes(const QString& data, WJModule module, WJProtocol protocol) {
    QList<WJ_DTC> dtcList;

    if (!data.startsWith("43")) {
        return dtcList;
    }

    QList<int> bytes = WJUtils::parseHexBytes(data);
    if (bytes.size() < 2) {
        return dtcList;
    }

    try {
        int numDTCs = bytes[1];

        if (numDTCs == 0) {
            return dtcList;
        }

        for (int i = 2; i < bytes.size() - 1; i += 2) {
            if (i + 1 < bytes.size()) {
                int byte1 = bytes[i];
                int byte2 = bytes[i + 1];

                if (byte1 == 0 && byte2 == 0) {
                    continue;
                }

                QString dtcCode = WJUtils::formatDTCCode(byte1, byte2, protocol);
                QString description = WJ_DTCs::getDTCDescription(dtcCode, module);

                WJ_DTC dtc;
                dtc.code = dtcCode;
                dtc.description = description;
                dtc.sourceModule = module;
                dtc.protocol = protocol;
                dtc.pending = false;
                dtc.confirmed = true;
                dtc.occurrence = 1;
                dtc.timestamp = QDateTime::currentMSecsSinceEpoch();

                dtcList.append(dtc);
            }
        }
    } catch (...) {
        // Return partial list if parsing fails
    }

    return dtcList;
}

QList<int> WJDataParser::extractBytes(const QString& data, int startIndex, int count) {
    QList<int> bytes = WJUtils::parseHexBytes(data);
    QList<int> result;

    for (int i = 0; i < count && (startIndex + i) < bytes.size(); ++i) {
        result.append(bytes[startIndex + i]);
    }

    return result;
}

double WJDataParser::convertRawToPhysical(int rawValue, double factor, double offset) {
    return (rawValue * factor) + offset;
}

bool WJDataParser::validateResponseFormat(const QString& data, const QString& expectedPrefix) {
    return data.trimmed().toUpper().startsWith(expectedPrefix.toUpper());
}

// Enhanced diagnostic session manager implementation
WJDiagnosticSession::WJDiagnosticSession()
    : interface(nullptr), sessionActive(false), activeModule(MODULE_UNKNOWN),
    activeProtocol(PROTOCOL_UNKNOWN), engineSecurityAccess(false) {
}

WJDiagnosticSession::~WJDiagnosticSession() {
    endSession();
}

bool WJDiagnosticSession::startSession(WJInterface* iface) {
    if (!iface) {
        lastError = "Invalid interface";
        return false;
    }

    interface = iface;

    if (!interface->initializeConnection(PROTOCOL_AUTO_DETECT)) {
        lastError = "Failed to initialize connection: " + interface->getLastError();
        return false;
    }

    sessionActive = true;
    return true;
}

void WJDiagnosticSession::endSession() {
    if (interface) {
        interface->disconnect();
        interface = nullptr;
    }

    sessionActive = false;
    activeModule = MODULE_UNKNOWN;
    activeProtocol = PROTOCOL_UNKNOWN;
    engineSecurityAccess = false;
    lastError.clear();
}

bool WJDiagnosticSession::switchToModule(WJModule module) {
    if (!sessionActive || !interface) {
        lastError = "Session not active";
        return false;
    }

    WJProtocol requiredProtocol = WJUtils::getProtocolFromModule(module);

    if (activeProtocol != requiredProtocol) {
        if (!switchProtocolIfNeeded(module)) {
            lastError = "Failed to switch protocol for module";
            return false;
        }
    }

    if (!initializeModule(module)) {
        lastError = "Failed to initialize module";
        return false;
    }

    activeModule = module;
    return true;
}

bool WJDiagnosticSession::readAllFaultCodes(QList<WJ_DTC>& allDTCs) {
    allDTCs.clear();

    QList<WJModule> modules = {MODULE_ENGINE_EDC15, MODULE_TRANSMISSION, MODULE_PCM, MODULE_ABS};

    for (WJModule module : modules) {
        if (switchToModule(module)) {
            QList<WJ_DTC> moduleDTCs;

            switch (module) {
            case MODULE_ENGINE_EDC15:
                readEngineFaultCodes(moduleDTCs);
                break;
            case MODULE_TRANSMISSION:
                readTransmissionFaultCodes(moduleDTCs);
                break;
            case MODULE_PCM:
                readPCMFaultCodes(moduleDTCs);
                break;
            case MODULE_ABS:
                readABSFaultCodes(moduleDTCs);
                break;
            default:
                break;
            }

            allDTCs.append(moduleDTCs);
        }
    }

    return !allDTCs.isEmpty();
}

bool WJDiagnosticSession::clearAllFaultCodes() {
    QList<WJModule> modules = {MODULE_ENGINE_EDC15, MODULE_TRANSMISSION, MODULE_PCM, MODULE_ABS};
    bool success = true;

    for (WJModule module : modules) {
        if (switchToModule(module)) {
            switch (module) {
            case MODULE_ENGINE_EDC15:
                success &= clearEngineFaultCodes();
                break;
            case MODULE_TRANSMISSION:
                success &= clearTransmissionFaultCodes();
                break;
            case MODULE_PCM:
                success &= clearPCMFaultCodes();
                break;
            case MODULE_ABS:
                success &= clearABSFaultCodes();
                break;
            default:
                break;
            }
        }
    }

    return success;
}

bool WJDiagnosticSession::readAllSensorData(WJSensorData& data) {
    data.reset();
    bool success = false;

    if (readEngineData(data.engine)) {
        success = true;
    }

    if (readTransmissionData(data.transmission)) {
        success = true;
    }

    if (readPCMData(data.pcm)) {
        success = true;
    }

    if (readABSData(data.abs)) {
        success = true;
    }

    if (success) {
        data.globalLastUpdate = QDateTime::currentMSecsSinceEpoch();
    }

    return success;
}

bool WJDiagnosticSession::readEngineData(WJSensorData::EngineData& engineData) {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        return false;
    }

    WJSensorData fullData;
    QString response;

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_MAF_DATA, response, MODULE_ENGINE_EDC15)) {
        WJDataParser::parseEngineMAFData(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_RAIL_PRESSURE_ACTUAL, response, MODULE_ENGINE_EDC15)) {
        WJDataParser::parseEngineRailPressureData(response, fullData);
        WJDataParser::parseEngineMAPData(response, fullData);
        WJDataParser::parseEngineMiscData(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_INJECTOR_DATA, response, MODULE_ENGINE_EDC15)) {
        WJDataParser::parseEngineInjectorData(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_BATTERY_VOLTAGE, response, MODULE_ENGINE_EDC15)) {
        WJDataParser::parseEngineBatteryVoltage(response, fullData);
    }

    engineData = fullData.engine;
    return engineData.dataValid;
}

bool WJDiagnosticSession::readEngineFaultCodes(QList<WJ_DTC>& dtcs) {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_DTC, response, MODULE_ENGINE_EDC15, 3000)) {
        dtcs = WJDataParser::parseEngineFaultCodes(response);
        return true;
    }

    return false;
}

bool WJDiagnosticSession::clearEngineFaultCodes() {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        return false;
    }

    QString response;
    return interface->sendCommandAndWaitResponse(WJ::Engine::CLEAR_DTC, response, MODULE_ENGINE_EDC15, 3000);
}

bool WJDiagnosticSession::readTransmissionData(WJSensorData::TransmissionData& transData) {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        return false;
    }

    WJSensorData fullData;
    QString response;

    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_TRANS_DATA, response, MODULE_TRANSMISSION)) {
        WJDataParser::parseTransmissionData(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_SPEED_DATA, response, MODULE_TRANSMISSION)) {
        WJDataParser::parseTransmissionSpeeds(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_SOLENOID_STATUS, response, MODULE_TRANSMISSION)) {
        WJDataParser::parseTransmissionSolenoids(response, fullData);
    }

    transData = fullData.transmission;
    return transData.dataValid;
}

bool WJDiagnosticSession::readTransmissionFaultCodes(QList<WJ_DTC>& dtcs) {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_DTC, response, MODULE_TRANSMISSION, 2000)) {
        dtcs = WJDataParser::parseTransmissionFaultCodes(response);
        return true;
    }

    return false;
}

bool WJDiagnosticSession::clearTransmissionFaultCodes() {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        return false;
    }

    QString response;
    return interface->sendCommandAndWaitResponse(WJ::Transmission::CLEAR_DTC, response, MODULE_TRANSMISSION, 3000);
}

bool WJDiagnosticSession::readPCMData(WJSensorData::PCMData& pcmData) {
    if (!switchToModule(MODULE_PCM)) {
        return false;
    }

    WJSensorData fullData;
    QString response;

    if (interface->sendCommandAndWaitResponse(WJ::PCM::READ_LIVE_DATA, response, MODULE_PCM)) {
        WJDataParser::parsePCMData(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::PCM::READ_FUEL_TRIM, response, MODULE_PCM)) {
        WJDataParser::parsePCMFuelTrim(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::PCM::READ_O2_SENSORS, response, MODULE_PCM)) {
        WJDataParser::parsePCMO2Sensors(response, fullData);
    }

    pcmData = fullData.pcm;
    return pcmData.dataValid;
}

bool WJDiagnosticSession::readPCMFaultCodes(QList<WJ_DTC>& dtcs) {
    if (!switchToModule(MODULE_PCM)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::PCM::READ_DTC, response, MODULE_PCM, 2000)) {
        dtcs = WJDataParser::parsePCMFaultCodes(response);
        return true;
    }

    return false;
}

bool WJDiagnosticSession::clearPCMFaultCodes() {
    if (!switchToModule(MODULE_PCM)) {
        return false;
    }

    QString response;
    return interface->sendCommandAndWaitResponse(WJ::PCM::CLEAR_DTC, response, MODULE_PCM, 3000);
}

bool WJDiagnosticSession::readABSData(WJSensorData::ABSData& absData) {
    if (!switchToModule(MODULE_ABS)) {
        return false;
    }

    WJSensorData fullData;
    QString response;

    if (interface->sendCommandAndWaitResponse(WJ::ABS::READ_WHEEL_SPEEDS, response, MODULE_ABS)) {
        WJDataParser::parseABSWheelSpeeds(response, fullData);
    }

    if (interface->sendCommandAndWaitResponse(WJ::ABS::READ_STABILITY_DATA, response, MODULE_ABS)) {
        WJDataParser::parseABSStabilityData(response, fullData);
    }

    absData = fullData.abs;
    return absData.dataValid;
}

bool WJDiagnosticSession::readABSFaultCodes(QList<WJ_DTC>& dtcs) {
    if (!switchToModule(MODULE_ABS)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::ABS::READ_DTC, response, MODULE_ABS, 2000)) {
        dtcs = WJDataParser::parseABSFaultCodes(response);
        return true;
    }

    return false;
}

bool WJDiagnosticSession::clearABSFaultCodes() {
    if (!switchToModule(MODULE_ABS)) {
        return false;
    }

    QString response;
    return interface->sendCommandAndWaitResponse(WJ::ABS::CLEAR_DTC, response, MODULE_ABS, 3000);
}

// Individual sensor reading methods with automatic protocol switching
bool WJDiagnosticSession::readEngineMAF(double& actual, double& specified) {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_MAF_DATA, response, MODULE_ENGINE_EDC15)) {
        WJSensorData data;
        if (WJDataParser::parseEngineMAFData(response, data)) {
            actual = data.engine.mafActual;
            specified = data.engine.mafSpecified;
            return true;
        }
    }

    return false;
}

bool WJDiagnosticSession::readEngineRailPressure(double& actual, double& specified) {
    if (!switchToModule(MODULE_ENGINE_EDC15)) {
        return false;
    }

    WJSensorData data;
    QString response;

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_RAIL_PRESSURE_ACTUAL, response, MODULE_ENGINE_EDC15)) {
        WJDataParser::parseEngineRailPressureData(response, data);
    }

    if (interface->sendCommandAndWaitResponse(WJ::Engine::READ_RAIL_PRESSURE_SPEC, response, MODULE_ENGINE_EDC15)) {
        QList<int> bytes = WJUtils::parseHexBytes(response);
        if (bytes.size() >= 12) {
            int railSpecRaw = WJUtils::bytesToInt16(bytes[9], bytes[10]);
            data.engine.railPressureSpecified = WJUtils::convertPressure(railSpecRaw);
        }
    }

    if (data.engine.dataValid) {
        actual = data.engine.railPressureActual;
        specified = data.engine.railPressureSpecified;
        return true;
    }

    return false;
}

bool WJDiagnosticSession::readTransmissionTemp(double& temp) {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_TEMP_DATA, response, MODULE_TRANSMISSION)) {
        WJSensorData data;
        if (WJDataParser::parseTransmissionData(response, data)) {
            temp = data.transmission.oilTemp;
            return true;
        }
    }

    return false;
}

bool WJDiagnosticSession::readTransmissionGear(double& gear) {
    if (!switchToModule(MODULE_TRANSMISSION)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::Transmission::READ_TRANS_DATA, response, MODULE_TRANSMISSION)) {
        WJSensorData data;
        if (WJDataParser::parseTransmissionData(response, data)) {
            gear = data.transmission.currentGear;
            return true;
        }
    }

    return false;
}

bool WJDiagnosticSession::readVehicleSpeed(double& speed) {
    if (!switchToModule(MODULE_PCM)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::PCM::READ_LIVE_DATA, response, MODULE_PCM)) {
        WJSensorData data;
        if (WJDataParser::parsePCMData(response, data)) {
            speed = data.pcm.vehicleSpeed;
            return true;
        }
    }

    return false;
}

bool WJDiagnosticSession::readWheelSpeeds(double& fl, double& fr, double& rl, double& rr) {
    if (!switchToModule(MODULE_ABS)) {
        return false;
    }

    QString response;
    if (interface->sendCommandAndWaitResponse(WJ::ABS::READ_WHEEL_SPEEDS, response, MODULE_ABS)) {
        WJSensorData data;
        if (WJDataParser::parseABSWheelSpeeds(response, data)) {
            fl = data.abs.wheelSpeedFL;
            fr = data.abs.wheelSpeedFR;
            rl = data.abs.wheelSpeedRL;
            rr = data.abs.wheelSpeedRR;
            return true;
        }
    }

    return false;
}

// Private helper methods
bool WJDiagnosticSession::performEngineSecurityAccess() {
    if (engineSecurityAccess) {
        return true;
    }

    QString response;

    if (!interface->sendCommandAndWaitResponse(WJ::Engine::SECURITY_ACCESS_REQUEST, response, MODULE_ENGINE_EDC15, 2000)) {
        return false;
    }

    if (!response.contains("67 01")) {
        return false;
    }

    if (!interface->sendCommandAndWaitResponse(WJ::Engine::SECURITY_ACCESS_KEY, response, MODULE_ENGINE_EDC15, 2000)) {
        return false;
    }

    if (response.contains("67 02")) {
        engineSecurityAccess = true;
        return true;
    }

    return true;
}

bool WJDiagnosticSession::switchProtocolIfNeeded(WJModule targetModule) {
    WJProtocol requiredProtocol = WJUtils::getProtocolFromModule(targetModule);

    if (activeProtocol == requiredProtocol) {
        return true;
    }

    QList<WJCommand> switchCommands = WJCommands::getProtocolSwitchCommands(activeProtocol, requiredProtocol);

    for (const WJCommand& cmd : switchCommands) {
        QString response;
        if (!interface->sendCommandAndWaitResponse(cmd.command, response, targetModule, cmd.timeoutMs)) {
            if (cmd.isCritical) {
                return false;
            }
        }
    }

    activeProtocol = requiredProtocol;
    return true;
}

bool WJDiagnosticSession::initializeModule(WJModule module) {
    QList<WJCommand> initCommands = WJCommands::getModuleInitCommands(module);

    for (const WJCommand& cmd : initCommands) {
        QString response;
        if (!interface->sendCommandAndWaitResponse(cmd.command, response, module, cmd.timeoutMs)) {
            if (cmd.isCritical) {
                return false;
            }
        }
    }

    if (module == MODULE_ENGINE_EDC15) {
        QString response;
        if (interface->sendCommandAndWaitResponse(WJ::Engine::START_COMMUNICATION, response, module, 2000)) {
            if (response.contains("C1")) {
                performEngineSecurityAccess();
                interface->sendCommandAndWaitResponse(WJ::Engine::START_DIAGNOSTIC_ROUTINE, response, module, 2000);
            }
        }
    }

    return true;
}

bool WJDiagnosticSession::validateModuleResponse(const QString& response, WJModule module) {
    if (response.isEmpty()) {
        return false;
    }

    WJProtocol moduleProtocol = WJUtils::getProtocolFromModule(module);

    if (WJUtils::isError(response, moduleProtocol)) {
        return false;
    }

    return WJUtils::isValidResponse(response, moduleProtocol);
}

// Legacy compatibility functions
namespace EDC15Utils = WJUtils;
namespace EDC15_DTCs = WJ_DTCs;
using EDC15DataParser = WJDataParser;

const char* ERROR[] = {
    "UNABLE", "BUS BUSY", "BUS ERROR", "BUFFER FULL", "CAN ERROR", "DATA ERROR",
    "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA", "NO DATA",
    "UNABLETOCONNECT", "<", ">", "7F", "NEGATIVE RESPONSE", "BUS INIT: ERROR"
};
const int ERROR_COUNT = 19;

const QString PCM_ECU_HEADER = "ATSH" + WJ::Headers::PCM;
const QString TRANS_ECU_HEADER = "ATSH" + WJ::Headers::TRANSMISSION;
const QString RESET = "ATZ";
const QString GET_PROTOCOL = "ATDP";

// Legacy EDC15 namespace for backward compatibility
namespace EDC15 {
const QString ECU_HEADER = WJ::Headers::ENGINE_EDC15;
const QString WAKEUP_MESSAGE = WJ::WakeupMessages::ENGINE_EDC15;
const QString PROTOCOL = WJ::Protocols::ISO_14230_4_KWP_FAST;
const int BAUD_RATE = WJ::Protocols::ISO_BAUD_RATE;
const int DEFAULT_TIMEOUT = WJ::Protocols::DEFAULT_TIMEOUT;
const int FAST_INIT_TIMEOUT = WJ::Protocols::FAST_INIT_TIMEOUT;
const int SECURITY_TIMEOUT = 2000;

const QString START_COMMUNICATION = WJ::Engine::START_COMMUNICATION;
const QString SECURITY_ACCESS_REQUEST = WJ::Engine::SECURITY_ACCESS_REQUEST;
const QString SECURITY_ACCESS_KEY = WJ::Engine::SECURITY_ACCESS_KEY;
const QString START_DIAGNOSTIC_ROUTINE = WJ::Engine::START_DIAGNOSTIC_ROUTINE;
const QString READ_DTC = WJ::Engine::READ_DTC;
const QString CLEAR_DTC = WJ::Engine::CLEAR_DTC;

const QString READ_MAF_DATA = WJ::Engine::READ_MAF_DATA;
const QString READ_RAIL_PRESSURE_ACTUAL = WJ::Engine::READ_RAIL_PRESSURE_ACTUAL;
const QString READ_RAIL_PRESSURE_SPEC = WJ::Engine::READ_RAIL_PRESSURE_SPEC;
const QString READ_MAP_DATA = WJ::Engine::READ_MAP_DATA;
const QString READ_MAP_SPEC = WJ::Engine::READ_MAP_DATA;
const QString READ_INJECTOR_DATA = WJ::Engine::READ_INJECTOR_DATA;
const QString READ_MISC_DATA = WJ::Engine::READ_MISC_DATA;
const QString READ_BATTERY_VOLTAGE = WJ::Engine::READ_BATTERY_VOLTAGE;

const QString RESPONSE_MAF = WJ::Responses::ENGINE_MAF;
const QString RESPONSE_RAIL_PRESSURE = WJ::Responses::ENGINE_RAIL_PRESSURE;
const QString RESPONSE_RAIL_PRESSURE_SPEC = WJ::Responses::ENGINE_RAIL_PRESSURE;
const QString RESPONSE_INJECTOR = WJ::Responses::ENGINE_INJECTOR;
const QString RESPONSE_DTC = WJ::Responses::ENGINE_DTC;
const QString RESPONSE_COMMUNICATION = WJ::Responses::ENGINE_COMMUNICATION;
const QString RESPONSE_SECURITY_ACCESS = WJ::Responses::ENGINE_SECURITY;
const QString RESPONSE_SECURITY_GRANTED = "67 02";
const QString RESPONSE_DIAGNOSTIC_ROUTINE = "71 25";

const QStringList ERROR_CODES = WJ::KWP2000_ERROR_CODES;
}

// Legacy EDC15Commands namespace for backward compatibility
namespace EDC15Commands {
QList<ELM327Command> getInitSequence() {
    QList<WJCommand> wjCommands = WJCommands::getInitSequence(PROTOCOL_ISO_14230_4_KWP_FAST);
    QList<ELM327Command> legacyCommands;

    for (const WJCommand& wjCmd : wjCommands) {
        ELM327Command legacyCmd;
        legacyCmd.command = wjCmd.command;
        legacyCmd.expectedResponse = wjCmd.expectedResponse;
        legacyCmd.description = wjCmd.description;
        legacyCmd.timeoutMs = wjCmd.timeoutMs;
        legacyCmd.isCritical = wjCmd.isCritical;
        legacyCommands.append(legacyCmd);
    }

    return legacyCommands;
}

QList<ELM327Command> getAlternativeInit() {
    QList<ELM327Command> commands;
    commands.append(ELM327Command("ATZ", "ELM327", "Reset ELM327", 7500));
    commands.append(ELM327Command("ATE0", "OK", "Echo off", 500));
    commands.append(ELM327Command("ATSP5", "OK", "Set protocol ISO_14230_4_KWP_FAST", 1000));
    commands.append(ELM327Command("ATSH" + WJ::Headers::ENGINE_EDC15, "OK", "Set ECU header", 500));
    commands.append(ELM327Command("ATFI", "BUS INIT", "Fast initialization", 5000));
    commands.append(ELM327Command("81", "C1", "Start communication", 2000));
    return commands;
}


// Legacy compatibility functions
namespace EDC15Utils = WJUtils;
namespace EDC15_DTCs = WJ_DTCs;
using EDC15DataParser = WJDataParser;

const char* ERROR[] = {
    "UNABLE", "BUS BUSY", "BUS ERROR", "BUFFER FULL", "CAN ERROR", "DATA ERROR",
    "ERROR", "STOPPED", "TIMEOUT", "?", "SEARCH", "NODATA", "NO DATA",
    "UNABLETOCONNECT", "<", ">", "7F", "NEGATIVE RESPONSE", "BUS INIT: ERROR"
};
const int ERROR_COUNT = 19;

const QString PCM_ECU_HEADER = "ATSH" + WJ::Headers::PCM;
const QString TRANS_ECU_HEADER = "ATSH" + WJ::Headers::TRANSMISSION;
const QString RESET = "ATZ";
const QString GET_PROTOCOL = "ATDP";

// Legacy EDC15 namespace for backward compatibility
namespace EDC15 {
const QString ECU_HEADER = WJ::Headers::ENGINE_EDC15;
const QString WAKEUP_MESSAGE = WJ::WakeupMessages::ENGINE_EDC15;
const QString PROTOCOL = WJ::Protocols::ISO_14230_4_KWP_FAST;
const int BAUD_RATE = WJ::Protocols::ISO_BAUD_RATE;
const int DEFAULT_TIMEOUT = WJ::Protocols::DEFAULT_TIMEOUT;
const int FAST_INIT_TIMEOUT = WJ::Protocols::FAST_INIT_TIMEOUT;
const int SECURITY_TIMEOUT = 2000;

const QString START_COMMUNICATION = WJ::Engine::START_COMMUNICATION;
const QString SECURITY_ACCESS_REQUEST = WJ::Engine::SECURITY_ACCESS_REQUEST;
const QString SECURITY_ACCESS_KEY = WJ::Engine::SECURITY_ACCESS_KEY;
const QString START_DIAGNOSTIC_ROUTINE = WJ::Engine::START_DIAGNOSTIC_ROUTINE;
const QString READ_DTC = WJ::Engine::READ_DTC;
const QString CLEAR_DTC = WJ::Engine::CLEAR_DTC;

const QString READ_MAF_DATA = WJ::Engine::READ_MAF_DATA;
const QString READ_RAIL_PRESSURE_ACTUAL = WJ::Engine::READ_RAIL_PRESSURE_ACTUAL;
const QString READ_RAIL_PRESSURE_SPEC = WJ::Engine::READ_RAIL_PRESSURE_SPEC;
const QString READ_MAP_DATA = WJ::Engine::READ_MAP_DATA;
const QString READ_MAP_SPEC = WJ::Engine::READ_MAP_DATA;
const QString READ_INJECTOR_DATA = WJ::Engine::READ_INJECTOR_DATA;
const QString READ_MISC_DATA = WJ::Engine::READ_MISC_DATA;
const QString READ_BATTERY_VOLTAGE = WJ::Engine::READ_BATTERY_VOLTAGE;

const QString RESPONSE_MAF = WJ::Responses::ENGINE_MAF;
const QString RESPONSE_RAIL_PRESSURE = WJ::Responses::ENGINE_RAIL_PRESSURE;
const QString RESPONSE_RAIL_PRESSURE_SPEC = WJ::Responses::ENGINE_RAIL_PRESSURE;
const QString RESPONSE_INJECTOR = WJ::Responses::ENGINE_INJECTOR;
const QString RESPONSE_DTC = WJ::Responses::ENGINE_DTC;
const QString RESPONSE_COMMUNICATION = WJ::Responses::ENGINE_COMMUNICATION;
const QString RESPONSE_SECURITY_ACCESS = WJ::Responses::ENGINE_SECURITY;
const QString RESPONSE_SECURITY_GRANTED = "67 02";
const QString RESPONSE_DIAGNOSTIC_ROUTINE = "71 25";

const QStringList ERROR_CODES = WJ::KWP2000_ERROR_CODES;
}

// Legacy EDC15Commands namespace for backward compatibility
namespace EDC15Commands {
QList<ELM327Command> getInitSequence() {
    QList<WJCommand> wjCommands = WJCommands::getInitSequence(PROTOCOL_ISO_14230_4_KWP_FAST);
    QList<ELM327Command> legacyCommands;

    for (const WJCommand& wjCmd : wjCommands) {
        ELM327Command legacyCmd;
        legacyCmd.command = wjCmd.command;
        legacyCmd.expectedResponse = wjCmd.expectedResponse;
        legacyCmd.description = wjCmd.description;
        legacyCmd.timeoutMs = wjCmd.timeoutMs;
        legacyCmd.isCritical = wjCmd.isCritical;
        legacyCommands.append(legacyCmd);
    }

    return legacyCommands;
}

QList<ELM327Command> getAlternativeInit() {
    QList<ELM327Command> commands;
    commands.append(ELM327Command("ATZ", "ELM327", "Reset ELM327", 7500));
    commands.append(ELM327Command("ATE0", "OK", "Echo off", 500));
    commands.append(ELM327Command("ATSP5", "OK", "Set protocol ISO_14230_4_KWP_FAST", 1000));
    commands.append(ELM327Command("ATSH" + WJ::Headers::ENGINE_EDC15, "OK", "Set ECU header", 500));
    commands.append(ELM327Command("ATFI", "BUS INIT", "Fast initialization", 5000));
    commands.append(ELM327Command("81", "C1", "Start communication", 2000));
    return commands;
}

QStringList getDiagnosticCommands() {
    QList<WJCommand> commands = WJCommands::getDiagnosticCommands(MODULE_ENGINE_EDC15);
    QStringList commandStrings;

    for (const WJCommand& cmd : commands) {
        commandStrings.append(cmd.command);
    }

    return commandStrings;
}
}
}
