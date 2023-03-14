#pragma once
#include <globals.h>
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// Start of namespace
namespace nRF {

// Beep pins (temporary)
#define beepPin 13

// How long to wait before "Bin Lid Is Open" beep reminder
#define FIVEMINUTES ((5) * 60 * 1000)

// This is just the way the RF24 library works:
// Hardware configuration: Set up nRF24L01 radio on SPI bus (pins 10, 11, 12, 13) plus pins 7 & 8
// For ESP32, pins are MOSI 23, CE 22, CSN 21, MISO 19, SCK 18
RF24 radio(22, 21); // CE, CSN

byte addresses[][6] = {"1Node", "2Node"};

// Use class to ensure only these values
enum class binLidStates : uint8_t {
    CLOSED,
    OPEN
};

// Data structure
struct binLidData {
    uint8_t binID;
    binLidStates binLidState;
    uint16_t batteryVoltage;
} data;

// -----------------------------------------------------------------------------
// SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP   SETUP
// -----------------------------------------------------------------------------
int nRFL2401Init() {
    Serial.println("THIS IS THE Bin Lid RECEIVER CODE - YOU NEED THE OTHER ARDUINO TO TRANSMIT");

    // Initiate the radio object
    if (!radio.begin()) {
        log_e("Radio module not responding");
        return 0;
    };

    // Whilst testing we don't want both receivers to auto-acknowledge.
    // TODO: BUG: Remove this statement on completion of project!
    //radio.setAutoAck(false);

    // Ensure IRQ only goes LOW on TX fail (the event we need to know about). 1=ignore, 0=process
    // https://www.instructables.com/How-to-Use-the-NRF24L01s-IRQ-Pin-to-Generate-an-In/
    radio.maskIRQ(1, 0, 1);

    // Set the transmit power to lowest available to prevent power supply related issues
    radio.setPALevel(RF24_PA_HIGH);

    // Set the speed of the transmission to the quickest available
    radio.setDataRate(RF24_2MBPS);

    // Use a channel unlikely to be used by WiF i, Microwave ovens etc
    radio.setChannel(124);

    // Open a writing and reading pipe on each radio, with opposite addresses
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(1, addresses[1]);

    // Start the radio listening for data
    radio.startListening();

    // All done
    log_d("Listening for signals on channel %d...", radio.getChannel());
    return 1;
}

// ----------------------------------------------------------------------------------
// We are LISTENING on this device only (although we do transmit an auto-acknowledge
// of any packets received)
// ----------------------------------------------------------------------------------
bool getRFData() {
    // Is there any data for us to get?
    if (radio.available()) {

        // Go and read the data and put it into that variable
        while (radio.available()) {
            radio.read(&data, sizeof(data));
            log_d("Received from bin %d: lid data: %d battery voltage: %5.2f", data.binID, data.binLidState, (float)data.batteryVoltage / 100);

            // Bin open? Make a reminder sound. Very annoying.
            static unsigned long beepMillis = millis();
            if (data.binLidState == binLidStates::OPEN && millis() - beepMillis > FIVEMINUTES) {
                digitalWrite(beepPin, HIGH);
                delay(100);
                digitalWrite(beepPin, LOW);
                beepMillis = millis();
            }
        }

        // Tell caller we have data
        return true;
    }

    // No data
    return false;
}

} // namespace RF