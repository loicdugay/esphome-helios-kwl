#include "cst3240_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst3240 {

static const char *const TAG = "cst3240.touchscreen";

static const uint8_t PRIMARY_ADDRESS = 0x5A;  // default I²C address for CST3240
static const uint16_t REG_TOUCH_DATA = 0xD000;
static const uint16_t REG_CHIP_ID   = 0xD109;
static const size_t MAX_TOUCHES = 10;

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL)); \
    return; \
  }

void CST3240Touchscreen::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    this->set_timeout(50, [this] { this->setup_internal_(); });
    return;
  }
  this->setup_internal_();
}

void CST3240Touchscreen::setup_internal_() {
  // optional INT pin
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
  }

  // probe the chip with a dummy write to REG_CHIP_ID
  uint8_t dummy = 0;
  i2c::ErrorCode err = this->write_register16_(REG_CHIP_ID, &dummy, 0);
  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("CST3240 not responding"));
    return;
  }

  this->setup_done_ = true;
  ESP_LOGI(TAG, "CST3240 initialized at 0x%02X", this->address_);
}

void CST3240Touchscreen::update_touches() {
  // this->skip_update_ = true;
  if (!this->setup_done_)
    return;

  uint8_t buf[7] = {0};
  i2c::ErrorCode err = this->read_register16_(REG_TOUCH_DATA, buf, sizeof(buf));
  ERROR_CHECK(err);

  uint8_t event = (buf[0] & 0x04) ? 1 : 0;
  uint8_t num_touches = buf[0] & 0x0F;
  if (event == 0 || num_touches == 0 || num_touches > MAX_TOUCHES)
    return;

  // this->skip_update_ = false;

  for (uint8_t i = 0; i < 1; i++) {
    // CST3240 encodes only one touch in 7 bytes, so multiple touches would need extra regs
    // For now, handle just the first touch
    uint16_t x = (buf[1] << 4) | ((buf[3] >> 4) & 0x0F);
    uint16_t y = (buf[2] << 4) | (buf[3] & 0x0F);
    this->add_raw_touch_position_(i, x, y);
    ESP_LOGD(TAG, "Touch %d -> X=%u Y=%u", i, x, y);
  } 

  // acknowledge event
  uint8_t over = 0xAB;
  this->write_register16_(REG_TOUCH_DATA, &over, 1);
}

void CST3240Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST3240 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

// --- helpers ---

i2c::ErrorCode CST3240Touchscreen::read_register16_(uint16_t reg, uint8_t *data, size_t len) {
  uint8_t regbuf[2] = {uint8_t(reg >> 8), uint8_t(reg & 0xFF)};
  auto err = this->write(regbuf, 2);
  if (err != i2c::ERROR_OK) return err;
  return this->read(data, len);
}

i2c::ErrorCode CST3240Touchscreen::write_register16_(uint16_t reg, const uint8_t *data, size_t len) {
  uint8_t buf[32];
  if (len + 2 > sizeof(buf)) return i2c::ERROR_TOO_LARGE;
  buf[0] = reg >> 8;
  buf[1] = reg & 0xFF;
  if (data && len > 0) memcpy(&buf[2], data, len);
  return this->write(buf, len + 2);
}

}  // namespace cst3240
}  // namespace esphome
