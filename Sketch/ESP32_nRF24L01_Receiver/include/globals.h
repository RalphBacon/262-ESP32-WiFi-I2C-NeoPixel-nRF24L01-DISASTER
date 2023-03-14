#pragma once
#include "Arduino.h"
// #include <ErriezDS3231.h>
#include "nRF24L01Helper.h"

// State Machine States (unique names from bin lid)
enum class machineState : uint8_t {
    RESTING,
    IDLE,
    ACTIVE,
    LONGACTIVE
};

// Each bin is tracked independently
struct binTrackers {
    machineState prevBinMachineState;
    machineState binMachineState;
    unsigned long stateMs;
    bool isLowVoltage;
    nRF::binLidData binData; // ID, Lid State, Battery Voltage
};

// Three structs for the bins
binTrackers binTracker[3];

// Semaphore to avoid clashes on access to binTrackers
SemaphoreHandle_t binSemaphore;

// How long before calm idle state (seconds)?
#define RESTING_DELAY 300

// Whether LEDs should be off (if in RESTING state)
volatile bool isQuietTime = false;
//#define QUIETTIME_END 7
//#define QUIETTIME_START 20

// Global time struct
// tm timeinfo;

// DS3231 RTC object
// ErriezDS3231 rtc;

// PIR module
#define vssPIR 33
#define gndPIR 32
#define sigPIR 17
#define MOVEMENT_TIMEOUT_SECONDS (15 * (60 * 1000))
unsigned long pirMillis = 0;