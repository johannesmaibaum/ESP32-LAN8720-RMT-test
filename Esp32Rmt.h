// Esp32Rmt.h - Library to abstract away the details of the RMT driver from
// ESP-IDF and let's you send and receive remote control commands from your
// Arduino sketches.
//
// https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/rmt.html
//
// Created by Johannes Maibaum in September 2018.
// Released into the public domain.
#ifndef ESP32RMT_H
#define ESP32RMT_H
#include "Arduino.h"

extern "C" {
#include "driver/rmt.h"
}

#define RECV_NEC 1
#define SEND_NEC 1

class Esp32Rmt
{
  public:
    Esp32Rmt(unsigned recv_pin, unsigned send_pin);

#if RECV_NEC
    bool enable_ir_reception(void);
    bool disable_ir_reception(void);
    bool recv_NEC(void);

    uint32_t received_data;
#endif  // RECV_NEC

#if SEND_NEC
    void send_NEC(uint32_t data, uint8_t repeats = 1);
#endif  // SEND_NEC

  private:
    gpio_num_t    _recv_pin;
    gpio_num_t    _send_pin;
    rmt_channel_t _recv_channel;
    rmt_channel_t _send_channel;
    bool          _receiving;

#if RECV_NEC
    void _NEC_rx_init(void);
#endif  // RECV_NEC

#if SEND_NEC
    void _NEC_tx_init(void);
#endif  // SEND_NEC
};

#endif  // ESP32RMT_H
