#include "Esp32Rmt.h"

#define RMT_RX_ACTIVE_LEVEL 0

#define RMT_CLOCK_DIV     100
#define RMT_TICK_10_US    (80000000/RMT_CLOCK_DIV/100000)

#define NEC_HEADER_HIGH_US    9000
#define NEC_HEADER_LOW_US     4500
#define NEC_BIT_ONE_HIGH_US    560
#define NEC_BIT_ONE_LOW_US   (2250-NEC_BIT_ONE_HIGH_US)
#define NEC_BIT_ZERO_HIGH_US   560
#define NEC_BIT_ZERO_LOW_US  (1120-NEC_BIT_ZERO_HIGH_US)
#define NEC_BIT_END            560
#define NEC_BIT_MARGIN         200

#define NEC_ITEM_DURATION(d)     ((d & 0x7fff) * 10/RMT_TICK_10_US)
#define NEC_DATA_ITEM_NUM          34
#define rmt_item32_t_TIMEOUT_US  9500

#if RECV_NEC
// Build register value of RMT waveform for one NEC data bit
static inline void NEC_fill_item_level(rmt_item32_t* item,
                                       int           high_us,
                                       int           low_us)
{
  item->level0 = 1;
  item->duration0 = (high_us) / 10 * RMT_TICK_10_US;
  item->level1 = 0;
  item->duration1 = (low_us) / 10 * RMT_TICK_10_US;
}

static inline bool NEC_is_in_range(int duration_ticks,
                                   int target_us,
                                   int margin_us)
{
  if ((NEC_ITEM_DURATION(duration_ticks) < (target_us + margin_us))
      && (NEC_ITEM_DURATION(duration_ticks) > (target_us - margin_us)))
  {
    return true;
  }
  return false;
}

static bool NEC_is_header(rmt_item32_t *item)
{
  if ((item->level0 == RMT_RX_ACTIVE_LEVEL
        && item->level1 != RMT_RX_ACTIVE_LEVEL)
      && NEC_is_in_range(item->duration0,
                         NEC_HEADER_HIGH_US,
                         NEC_BIT_MARGIN)
      && NEC_is_in_range(item->duration1,
                         NEC_HEADER_LOW_US,
                         NEC_BIT_MARGIN))
  {
    return true;
  }
  return false;
}

static bool NEC_is_bit_one(rmt_item32_t *item)
{
  if ((item->level0 == RMT_RX_ACTIVE_LEVEL
        && item->level1 != RMT_RX_ACTIVE_LEVEL)
      && NEC_is_in_range(item->duration0,
                         NEC_BIT_ONE_HIGH_US,
                         NEC_BIT_MARGIN)
      && NEC_is_in_range(item->duration1,
                         NEC_BIT_ONE_LOW_US,
                         NEC_BIT_MARGIN))
  {
    return true;
  }
  return false;
}

static bool NEC_is_bit_zero(rmt_item32_t *item)
{
  if ((item->level0 == RMT_RX_ACTIVE_LEVEL
        && item->level1 != RMT_RX_ACTIVE_LEVEL)
      && NEC_is_in_range(item->duration0,
                         NEC_BIT_ZERO_HIGH_US,
                         NEC_BIT_MARGIN)
      && NEC_is_in_range(item->duration1,
                         NEC_BIT_ZERO_LOW_US,
                         NEC_BIT_MARGIN))
  {
    return true;
  }
  return false;
}

static int NEC_parse_items(rmt_item32_t *item,
                           int           item_num,
                           uint32_t     *ir_data)
{
  if (item_num < NEC_DATA_ITEM_NUM) {
    return -1;
  }

  if (!NEC_is_header(item++)) {
    return -1;
  }

  uint32_t data = 0;
  int i = 0;
  for (; i < 32; ++i, ++item) {
    if (NEC_is_bit_one(item)) {
      data = (data << 1) | 1;
    } else if (NEC_is_bit_zero(item)) {
      data = (data << 1) | 0;
    } else {
      return -1;
    }
  }

  *ir_data = data;
  return i;
}

void Esp32Rmt::_NEC_rx_init(void)
{
  rmt_config_t rmt_rx;
  rmt_rx.rmt_mode = RMT_MODE_RX;
  rmt_rx.channel = _recv_channel;
  rmt_rx.clk_div = RMT_CLOCK_DIV;
  rmt_rx.gpio_num = _recv_pin;
  rmt_rx.mem_block_num = 1;
  rmt_rx.rx_config.filter_en = false;
  rmt_rx.rx_config.idle_threshold = rmt_item32_t_TIMEOUT_US
                                    / 10 * (RMT_TICK_10_US);

  rmt_config(&rmt_rx);
  rmt_driver_install(rmt_rx.channel, 1000, 0);
}

bool Esp32Rmt::enable_ir_reception(void)
{
  if (_receiving)  // No need to enable
    return true;

  if (rmt_rx_start(_recv_channel, true) != ESP_OK)
    return false;

  _receiving = true;
  return true;
}

bool Esp32Rmt::disable_ir_reception(void)
{
  if (!_receiving)  // No need to disable
    return true;

  if (rmt_rx_stop(_recv_channel) != ESP_OK)
    return false;

  _receiving = false;
  return true;
}

bool Esp32Rmt::recv_NEC(void)
{
  if (!_receiving)  // Cannot receive anything if not receiving
    return false;

  // Reset current value
  received_data = 0;

  size_t rx_size = 0;
  RingbufHandle_t rb = nullptr;
  rmt_get_ringbuf_handle(_recv_channel, &rb);
  if (rb) {
    rmt_item32_t *item = static_cast<rmt_item32_t*>(xRingbufferReceive(
        rb, &rx_size, 1000));
    if (item) {
      int offset = 0;
      for (;;) {
        int res = NEC_parse_items(item + offset, rx_size / 4 - offset,
                                  &received_data);
        if (res > 0) {
          offset += res + 1;
        } else {
          break;
        }
      }
      vRingbufferReturnItem(rb, static_cast<void *>(item));
    }
  }

  if (received_data > 0)
    return true;
  else
    return false;
}
#endif  // RECV_NEC

#if SEND_NEC
static void NEC_fill_item_header(rmt_item32_t* item)
{
  NEC_fill_item_level(item, NEC_HEADER_HIGH_US, NEC_HEADER_LOW_US);
}

static void NEC_fill_item_bit_one(rmt_item32_t* item)
{
  NEC_fill_item_level(item, NEC_BIT_ONE_HIGH_US, NEC_BIT_ONE_LOW_US);
}

static void NEC_fill_item_bit_zero(rmt_item32_t* item)
{
  NEC_fill_item_level(item, NEC_BIT_ZERO_HIGH_US, NEC_BIT_ZERO_LOW_US);
}

static void NEC_fill_item_end(rmt_item32_t *item)
{
  NEC_fill_item_level(item, NEC_BIT_END, 0x7fff);
}

static int NEC_build_items(uint8_t       channel,
                           rmt_item32_t *item,
                           int           item_num,
                           uint32_t      data)
{
  int i = 0;
  if (item_num < NEC_DATA_ITEM_NUM) {
    return -1;
  }

  NEC_fill_item_header(item++);
  ++i;

  for (int j = 31; j >= 0; --j, ++item, ++i) {
    if (data & (1 << j)) {
      NEC_fill_item_bit_one(item);
    } else {
      NEC_fill_item_bit_zero(item);
    }
  }

  NEC_fill_item_end(item);
  ++i;
  return i;
}

void Esp32Rmt::_NEC_tx_init(void)
{
  rmt_config_t rmt_tx;
  rmt_tx.rmt_mode = RMT_MODE_TX;
  rmt_tx.channel = _send_channel;
  rmt_tx.clk_div = RMT_CLOCK_DIV;
  rmt_tx.gpio_num = _send_pin;
  rmt_tx.mem_block_num = 1;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_freq_hz = 38000;
  rmt_tx.tx_config.carrier_duty_percent = 33;
  rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
  rmt_tx.tx_config.carrier_en = true;
  rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  rmt_tx.tx_config.idle_output_en = false;

  rmt_config(&rmt_tx);
  rmt_driver_install(rmt_tx.channel, 1000, 0);
}

void Esp32Rmt::send_NEC(uint32_t ir_data, uint8_t repeats)
{
  // Build raw data to send
  rmt_item32_t* item = new rmt_item32_t[NEC_DATA_ITEM_NUM * repeats];
  int item_num = NEC_DATA_ITEM_NUM * repeats;
  int i, offset = 0;
  for (;;) {
    i = NEC_build_items(_send_channel, item + offset, item_num - offset,
                        ir_data);
    if (i < 0)
      break;
    offset += i;
  }

  // Send them
  rmt_write_items(_send_channel, item, item_num, true);
  // Setting wait_tx_done (last arg) to true blocks, so the following call
  // should not be needed.
  //esp_err_t ret = rmt_wait_tx_done(_send_channel, portMAX_DELAY);

  delete[] item;
}
#endif  // SEND_NEC
