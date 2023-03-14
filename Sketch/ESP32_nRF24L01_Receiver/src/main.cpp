/*
    Simple receiver for nRF24L01 to receive anything and display it on screen
    July 2021

    Ported to ESP32 for final project, March 2022.
    nRF24L01 board powered by 3v3 ~12mA from the ESP32 Dev Board (AMS1117 3v3)
*/
#include <esp32-hal-log.h>
//#undef ARDUHAL_LOG_FORMAT
//#define ARDUHAL_LOG_FORMAT(letter, format) ARDUHAL_LOG_COLOR_ ## letter "[" #letter "][%s:%u] %s(): " format ARDUHAL_LOG_RESET_COLOR "\r\n", pathToFileName(__FILE__), __LINE__, __FUNCTION__

#include "globals.h"
#include "nRF24L01Helper.h"
#include "neoPixelHelper.h"
//#include "wifiHelper.h"
#include "pirHelper.hpp"
#include <Arduino.h>

// Max minutes sefore it's considered open "too long". First digit in formula is the minutes.
#define LONGOPENMILLIS ((10) * (60) * (1000) + (1000))

// What is a LOW voltage? Integer value so 3v31 ➡ 331
#define LOWVOLTAGE (330)

// Check low voltage and whether bin still open every X seconds
#define THIRTYSECONDS (30 * 1000)

// Forward declarations
void trackBinState(binTrackers& binTracker);
std::string getBinTrackerState(const binTrackers& binTracker);
void printTimeOpen(binTrackers& binTracker);
void printLowVoltage(binTrackers& binTracker);

// -----------------------------------------------------------------------------
// SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP
// -----------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // RTC
    // WIFI::rtcBegin();
    // WIFI::getTimeFromInternet();
    //WIFI::rtcEnd();

    // Beeper pin
    pinMode(beepPin, OUTPUT);
    digitalWrite(beepPin, LOW);

    // Initialise nRF24L01 module. Abort if not successful.
    if (!nRF::nRFL2401Init()) {
        while (1) {
            // TODO: do something with NeoPixels to indicate error state
            for (auto cnt = 0; cnt < 2; cnt++) {
                digitalWrite(beepPin, HIGH);
                delay(75);
                digitalWrite(beepPin, LOW);
                delay(100);
            }
            delay(1000);
        }
    }

    // Initialise trackers. Starting point is all bins closed.
    uint8_t count = 0;
    for (auto& bin : binTracker) {
        bin.prevBinMachineState = machineState::IDLE;
        bin.binMachineState = machineState::RESTING;
        bin.stateMs = 0;
        bin.isLowVoltage = false;
        bin.binData.binID = ++count;
        bin.binData.binLidState = nRF::binLidStates::CLOSED;
        bin.binData.batteryVoltage = 0;
    }

    // Test the NeoPixels
    neo::neoTaskSetup();
}

// ----------------------------------------------------------------------------------
// Main processing loop
// ----------------------------------------------------------------------------------
void loop()
{
    // Get bin data if any exists. If not there may still be work to do (eg long open bin lid)
    if (nRF::getRFData()) {
        log_d("Received data for bin %d, lid state %d", nRF::data.binID, nRF::data.binLidState);

        // Update the underlying Bin Lid data for this bin
        binTracker[nRF::data.binID - 1].binData = nRF::data;
    }

    // Any OPEN bin not sent data in last X minutes? (Heartbeat)
    // Has any bin lid been open longer than X minutes?
    // Has any bin got a low battery?
    // Inspect the State for each bin
    for (auto cnt = 0; cnt < 3; cnt++) {
        trackBinState(binTracker[cnt]);
    }

    // Ensure the neoPixel task is running for this bin
    // To do this, increase the task priority to 1
    static bool runOnce = false;
    if (!runOnce) {

        // Get the pointer to each neoPixel task handle
        for (TaskHandle_t* neoTask : neo::neoPixelTaskHandles) {
            // And set the priority to 1 (was zero)
            vTaskPrioritySet(*neoTask, 1);
            vTaskDelay(250);
        }
        runOnce = true;
    }

    // Check whether movement (if none, LEDs are switched off)
    pirH::isMovementDetected();
}

// Common tracker code
void trackBinState(binTrackers& binTracker)
{
    // Inspect the machine state for the current bin
    switch (binTracker.binMachineState) {
    case machineState::RESTING:
        // Has it just changed?
        if (binTracker.prevBinMachineState != machineState::RESTING) {
            binTracker.stateMs = millis();
            binTracker.prevBinMachineState = machineState::RESTING;
            log_d("Bin %d now %s", binTracker.binData.binID, getBinTrackerState(binTracker).c_str());
        }

        // Bin lid now open? Move to next State.
        if (binTracker.binData.binLidState == nRF::binLidStates::OPEN) {
            binTracker.binMachineState = machineState::ACTIVE;
        }

        break;

    case machineState::IDLE:

        // Has it just been shut?
        if (binTracker.prevBinMachineState != machineState::IDLE) {
            binTracker.stateMs = millis();
            binTracker.prevBinMachineState = machineState::IDLE;
            log_d("Bin %d now %s", binTracker.binData.binID, getBinTrackerState(binTracker).c_str());
        }

        // Bin lid now open? Move to next State.
        if (binTracker.binData.binLidState == nRF::binLidStates::OPEN) {
            binTracker.binMachineState = machineState::ACTIVE;
            break;
        }

        // Long time in IDLE state? Move to RESTING state
        if (millis() - binTracker.stateMs > (RESTING_DELAY * 1000)) {
            binTracker.binMachineState = machineState::RESTING;
        }

        // If previously shut then no action required (this is most common state)
        break;

    case machineState::ACTIVE:

        // Has it just opened?
        if (binTracker.prevBinMachineState != machineState::ACTIVE) {
            binTracker.prevBinMachineState = machineState::ACTIVE;
            log_d("Bin %d now %s", binTracker.binData.binID, getBinTrackerState(binTracker).c_str());

            // Start the clock running to time the lid open state
            binTracker.stateMs = millis();
        }

        // How long has it been this way? Move to next State if too long.
        if (millis() - binTracker.stateMs > LONGOPENMILLIS) {
            binTracker.binMachineState = machineState::LONGACTIVE;
            break;
        }

        // Print every so often
        printTimeOpen(binTracker);

        // Check the voltage every so often
        printLowVoltage(binTracker);

        // Now closed?
        if (binTracker.binData.binLidState == nRF::binLidStates::CLOSED) {
            binTracker.binMachineState = machineState::IDLE;
            break;
        }

        break;

    case machineState::LONGACTIVE: // implicit OPEN

        // New state?
        if (binTracker.prevBinMachineState != machineState::LONGACTIVE) {
            binTracker.prevBinMachineState = machineState::LONGACTIVE;
            log_d("Bin %d now %s", binTracker.binData.binID, getBinTrackerState(binTracker).c_str());
        }

        // Only display this every so often
        printTimeOpen(binTracker);

        // Check the voltage every so often
        printLowVoltage(binTracker);

        // Now closed? Move to next state
        if (binTracker.binData.binLidState == nRF::binLidStates::CLOSED) {
            binTracker.binMachineState = machineState::IDLE;
            break;
        }

        break;

    default:
        log_e("Default bin state %d identified", binTracker.binMachineState);
    }
}

// Machine state printable string (eg IDLE rather than 0)
std::string getBinTrackerState(const binTrackers& binTracker)
{
    switch (binTracker.binMachineState) {
    case machineState::RESTING:
        return "RESTING";

    case machineState::IDLE:
        return "IDLE";

    case machineState::ACTIVE:
        return "ACTIVE";

    case machineState::LONGACTIVE:
        return "LONG OPEN";

    default:
        return "UNKNOWN";
    }
}

// Bin Lid open time
void printTimeOpen(binTrackers& binTracker)
{
    static unsigned long openMillis[3] = { 0 };

    if (millis() - openMillis[binTracker.binData.binID] > THIRTYSECONDS) {
        uint16_t secs = ((millis() - binTracker.stateMs) / 1000);
        uint16_t mins = secs / 60;
        secs = secs % 60;
        log_i("Bin %d has been open for %02d:%02d", binTracker.binData.binID, mins, secs);
        openMillis[binTracker.binData.binID] = millis();
    }
}

// Whether low voltage
void printLowVoltage(binTrackers& binTracker)
{
    static unsigned long voltMillis[3] = { 0 };

    // Report every minute
    if (millis() - voltMillis[binTracker.binData.binID] > THIRTYSECONDS || voltMillis[binTracker.binData.binID] == 0) {
        if (binTracker.binData.batteryVoltage < LOWVOLTAGE) {
            // Set the flag
            binTracker.isLowVoltage = true;

            // Log it
            log_d(
                "Bin %d battery voltage of %5.2f is below minimum of %5.2f",
                binTracker.binData.binID,
                float(binTracker.binData.batteryVoltage / 100.0),
                float(LOWVOLTAGE / 100.0));
        } else {
            // Never clear the flag once set; only a battery change/charge can reset this
        }

        // Reset the delay
        voltMillis[binTracker.binData.binID] = millis();
    }
}