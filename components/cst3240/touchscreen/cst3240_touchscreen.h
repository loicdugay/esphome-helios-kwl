#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace cst3240 {

class CST3240Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  /// @brief Initialize the CST3240 touchscreen.
  void setup() override;
  void dump_config() override;
  bool can_proceed() override { return this->setup_done_; }

  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

 protected:
  /// @brief Update touch data (called periodically by ESPHome)
  void update_touches() override;

  /// @brief Perform the internal setup routine for the CST3240 touchscreen.
  void setup_internal_();

  i2c::ErrorCode read_register16_(uint16_t reg, uint8_t *data, size_t len);
  i2c::ErrorCode write_register16_(uint16_t reg, const uint8_t *data, size_t len);

  bool setup_done_{false};

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
};

}  // namespace cst3240
}  // namespace esphome
