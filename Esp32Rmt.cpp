#include "Esp32Rmt.h"

Esp32Rmt::Esp32Rmt(unsigned recv_pin, unsigned send_pin)
  : _recv_pin{static_cast<gpio_num_t>(recv_pin)}
  , _send_pin{static_cast<gpio_num_t>(send_pin)}
  , _recv_channel{RMT_CHANNEL_0}
  , _send_channel{RMT_CHANNEL_1}
  , _receiving{false}
{
#if RECV_NEC
  received_data = 0;
  _NEC_rx_init();
#endif  // RECV_NEC

#if SEND_NEC
  _NEC_tx_init();
#endif  // SEND_NEC
}
