#include "helios_kwl.h"

#include "esphome/core/log.h"

namespace esphome {
namespace helios_kwl_component {

static const char* TAG = "helios_kwl_component.component";

const int HeliosKwlComponent::TEMPERATURE[] = {
    -74, -70, -66, -62, -59, -56, -54, -52, -50, -48, -47, -46, -44, -43, -42, -41, -40, -39, -38, -37, -36, -35,
    -34, -33, -33, -32, -31, -30, -30, -29, -28, -28, -27, -27, -26, -25, -25, -24, -24, -23, -23, -22, -22, -21,
    -21, -20, -20, -19, -19, -19, -18, -18, -17, -17, -16, -16, -16, -15, -15, -14, -14, -14, -13, -13, -12, -12,
    -12, -11, -11, -11, -10, -10, -9,  -9,  -9,  -8,  -8,  -8,  -7,  -7,  -7,  -6,  -6,  -6,  -5,  -5,  -5,  -4,
    -4,  -4,  -3,  -3,  -3,  -2,  -2,  -2,  -1,  -1,  -1,  -1,  0,   0,   0,   1,   1,   1,   2,   2,   2,   3,
    3,   3,   4,   4,   4,   5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,
    10,  10,  11,  11,  11,  12,  12,  12,  13,  13,  13,  14,  14,  14,  15,  15,  15,  16,  16,  16,  17,  17,
    18,  18,  18,  19,  19,  19,  20,  20,  21,  21,  21,  22,  22,  22,  23,  23,  24,  24,  24,  25,  25,  26,
    26,  27,  27,  27,  28,  28,  29,  29,  30,  30,  31,  31,  32,  32,  33,  33,  34,  34,  35,  35,  36,  36,
    37,  37,  38,  38,  39,  40,  40,  41,  41,  42,  43,  43,  44,  45,  45,  46,  47,  48,  48,  49,  50,  51,
    52,  53,  53,  54,  55,  56,  57,  59,  60,  61,  62,  63,  65,  66,  68,  69,  71,  73,  75,  77,  79,  81,
    82,  86,  90,  93,  97,  100, 100, 100, 100, 100, 100, 100, 100, 100};

void HeliosKwlComponent::setup() {
  ESP_LOGI(TAG, "setup()");

  const std::vector<const EntityBase*> states{m_power_state,       m_bypass_state,    m_winter_mode_switch,
                                              m_heating_indicator, m_fault_indicator, m_service_reminder};
  if (std::any_of(states.cbegin(), states.cend(), [](const EntityBase* pointer) { return pointer != nullptr; })) {
    m_pollers.push_back([&]() { poll_states(); });
  }

  const std::vector<const EntityBase*> alarms{m_co2_alarm, m_freeze_alarm};
  if (std::any_of(alarms.cbegin(), alarms.cend(), [](const EntityBase* p) { return p != nullptr; })) {
    m_pollers.push_back([this]() { poll_alarms(); });
  }

  if (m_temperature_outside != nullptr) {
    m_pollers.push_back([this]() { poll_temperature_outside(); });
  }

  if (m_temperature_exhaust != nullptr) {
    m_pollers.push_back([this]() { poll_temperature_exhaust(); });
  }

  if (m_temperature_inside != nullptr) {
    m_pollers.push_back([this]() { poll_temperature_inside(); });
  }

  if (m_temperature_incoming != nullptr) {
    m_pollers.push_back([this]() { poll_temperature_incoming(); });
  }

  if (m_fan_speed != nullptr) {
    m_pollers.push_back([this]() { poll_fan_speed(); });
  }

  if (m_humidity_sensor1 != nullptr) {
    m_pollers.push_back([this]() { poll_humidity_sensors1(); });
  }

  if (m_humidity_sensor2 != nullptr) {
    m_pollers.push_back([this]() { poll_humidity_sensors2(); });
  }

  m_current_poller = m_pollers.cbegin();
}

void HeliosKwlComponent::update() {
  if (m_pollers.empty()) return;

  // Priority: re-poll speed/states immediately after a command was sent
  if (m_speed_poll_pending) {
    m_speed_poll_pending = false;
    poll_fan_speed();
    return;
  }
  if (m_state_poll_pending) {
    m_state_poll_pending = false;
    poll_states();
    return;
  }

  if (m_current_poller == m_pollers.cend()) {
    m_current_poller = m_pollers.cbegin();
  }

  (*m_current_poller)();
  std::advance(m_current_poller, 1);
}

void HeliosKwlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Helios KWL:");
  LOG_SENSOR("  ", "Fan speed", m_fan_speed);
  LOG_SENSOR("  ", "Temperature outside", m_temperature_outside);
  LOG_SENSOR("  ", "Temperature exhaust", m_temperature_exhaust);
  LOG_SENSOR("  ", "Temperature inside", m_temperature_inside);
  LOG_SENSOR("  ", "Temperature incoming", m_temperature_incoming);
  LOG_SENSOR("  ", "Humidity Sensor 1", m_humidity_sensor1);
  LOG_SENSOR("  ", "Humidity Sensor 2", m_humidity_sensor2);
  LOG_BINARY_SENSOR("  ", "Power state", m_power_state);
  LOG_BINARY_SENSOR("  ", "Bypass state", m_bypass_state);
  LOG_BINARY_SENSOR("  ", "Fault indicator", m_fault_indicator);
  LOG_BINARY_SENSOR("  ", "Service reminder", m_service_reminder);
  LOG_BINARY_SENSOR("  ", "CO2 alarm", m_co2_alarm);
  LOG_BINARY_SENSOR("  ", "Freeze alarm", m_freeze_alarm);
}

void HeliosKwlComponent::set_fan_speed(float speed) {
  // When sync_in_progress is true, this call comes from the YAML syncing
  // the fan entity with the sensor reading. No RS485 write needed.
  if (sync_in_progress) {
    return;
  }

  if (speed == 0.f) {
    set_state_flag(0, false);
  } else {
    assert(speed >= 0.f && speed <= 8.f);
    const uint8_t speed_byte = 0xFF >> (8 - static_cast<int>(speed * 8));
    if (set_value(0x29, speed_byte)) {
      ESP_LOGD(TAG, "Wrote speed: %02x", speed_byte);
      set_state_flag(0, true);
      m_speed_poll_pending = true;
    } else {
      ESP_LOGE(TAG, "Failed to set fan speed");
    }
  }
}

void HeliosKwlComponent::set_state_flag(uint8_t bit, bool state) {
  // When sync_in_progress is true, skip RS485 writes.
  if (sync_in_progress) {
    return;
  }

  if (auto value = poll_register(0xA3)) {
    if (state == ((*value >> bit) & 0x01)) {
      ESP_LOGD(TAG, "State flag already set");
    } else {
      if (state) {
        *value |= 0x01 << bit;
      } else {
        *value &= ~(0x01 << bit);
      }
      if (set_value(0xA3, *value)) {
        ESP_LOGD(TAG, "Wrote state flag to: %x", *value);
        m_state_poll_pending = true;
      } else {
        ESP_LOGE(TAG, "Failed to set state flag");
      }
    }
  } else {
    ESP_LOGE(TAG, "Unable to poll register 0xA3");
  }
}

void HeliosKwlComponent::poll_temperature_outside() {
  if (const auto value = poll_register(0x32)) {
    m_temperature_outside->publish_state(TEMPERATURE[*value]);
  }
}

void HeliosKwlComponent::poll_temperature_exhaust() {
  if (const auto value = poll_register(0x33)) {
    m_temperature_exhaust->publish_state(TEMPERATURE[*value]);
  }
}

void HeliosKwlComponent::poll_temperature_inside() {
  if (const auto value = poll_register(0x34)) {
    m_temperature_inside->publish_state(TEMPERATURE[*value]);
  }
}

void HeliosKwlComponent::poll_temperature_incoming() {
  if (const auto value = poll_register(0x35)) {
    m_temperature_incoming->publish_state(TEMPERATURE[*value]);
  }
}

void HeliosKwlComponent::poll_fan_speed() {
  if (const auto value = poll_register(0x29)) {
    m_fan_speed->publish_state(count_ones(*value));
  }
}

void HeliosKwlComponent::poll_humidity_sensors1() {
    if (const auto value = poll_register(0x2F)) {
      if (*value < 0x33) {
        m_humidity_sensor1->publish_state(0.0f);
        return;
      }
      float humidity1 = (*value - 51) / 2.04f;
      if (humidity1 > 100.0f) humidity1 = 100.0f;
      m_humidity_sensor1->publish_state(humidity1);
    }
}

void HeliosKwlComponent::poll_humidity_sensors2() {
  if (const auto value = poll_register(0x30)) {
    if (*value < 0x33) {
      m_humidity_sensor2->publish_state(0.0f);
      return;
    }
    float humidity2 = (*value - 51) / 2.04f;
    if (humidity2 > 100.0f) humidity2 = 100.0f;
    m_humidity_sensor2->publish_state(humidity2);
  }
}

void HeliosKwlComponent::poll_states() {
  if (const auto value = poll_register(0xA3)) {
    if (m_power_state != nullptr) {
      m_power_state->publish_state(*value & (0x01 << 0));
    }
    const bool bypass_state = *value & (0x01 << 3);
    if (m_bypass_state != nullptr) {
      m_bypass_state->publish_state(bypass_state);
    }
    if (m_winter_mode_switch != nullptr) {
      m_winter_mode_switch->publish_state(bypass_state);
    }
    if (m_heating_indicator != nullptr) {
      m_heating_indicator->publish_state(*value & (0x01 << 5));
    }
    if (m_fault_indicator != nullptr) {
      m_fault_indicator->publish_state(*value & (0x01 << 6));
    }
    if (m_service_reminder != nullptr) {
      m_service_reminder->publish_state(*value & (0x01 << 7));
    }
  }
}

optional<uint8_t> HeliosKwlComponent::poll_register(uint8_t address) {
  // Flush read buffer
  flush_read_buffer();

  // Write
  Datagram temp = {0x01, ADDRESS, MAINBOARD, 0x00, address};
  temp[5] = checksum(temp.cbegin(), temp.cend());
  write_array(temp);
  flush();

  // Read
  if (const auto response = read_array<6>()) {
    const auto& array = *response;
    if (check_crc(array.cbegin(), array.cend())) {
      if (array[1] == MAINBOARD && array[2] == ADDRESS && array[3] == address) {
        return array[4];
      } else {
        const auto hex = format_hex_pretty(array.data(), array.size());
        ESP_LOGW(TAG, "Wrong response (bus collision?): %s", hex.c_str());
      }
    } else {
      const auto hex = format_hex_pretty(array.data(), array.size());
      ESP_LOGE(TAG, "Bad checksum for response: %s", hex.c_str());
    }
  }
  return {};
}

bool HeliosKwlComponent::set_value(uint8_t address, uint8_t value) {
  // Helios protocol: 3-message write sequence (Annex B)
  // The VMC confirms by updating the register value, readable at next poll.
  // We do NOT wait for ACK bytes as they are unreliable on a busy bus.

  // Message 1: Broadcast to all remote controls (0x20)
  Datagram msg1 = {0x01, ADDRESS, 0x20, address, value, 0};
  msg1[5] = checksum(msg1.cbegin(), std::prev(msg1.cend()));
  flush_read_buffer();
  write_array(msg1);
  flush();
  delay(5);

  // Message 2: Broadcast to all motherboards (0x10)
  Datagram msg2 = {0x01, ADDRESS, 0x10, address, value, 0};
  msg2[5] = checksum(msg2.cbegin(), std::prev(msg2.cend()));
  write_array(msg2);
  flush();
  delay(5);

  // Message 3: Direct to motherboard 1 (0x11) + doubled checksum
  Datagram msg3 = {0x01, ADDRESS, MAINBOARD, address, value, 0};
  msg3[5] = checksum(msg3.cbegin(), std::prev(msg3.cend()));
  write_array(msg3);
  write_byte(msg3[5]);  // Checksum sent twice per Helios protocol
  flush();

  ESP_LOGD(TAG, "Wrote register 0x%02X = 0x%02X (3-message sequence)", address, value);
  return true;
}

void HeliosKwlComponent::flush_read_buffer() {
  const uint32_t start = millis();
  uint32_t last_byte_time = millis();
  while (millis() - last_byte_time < 10) {
    if (millis() - start > 200) {
      ESP_LOGW(TAG, "flush_read_buffer: safety timeout (200ms)");
      break;
    }
    while (available()) {
      read();
      last_byte_time = millis();
    }
    yield();
  }
}

uint8_t HeliosKwlComponent::count_ones(uint8_t byte) {
  static const uint8_t NIBBLE_LOOKUP[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
  return NIBBLE_LOOKUP[byte & 0x0F] + NIBBLE_LOOKUP[byte >> 4];
}

void HeliosKwlComponent::poll_alarms() {
  if (const auto value = poll_register(0x6D)) {
    if (m_co2_alarm != nullptr) {
      m_co2_alarm->publish_state(*value & (0x01 << 6));
    }
    if (m_freeze_alarm != nullptr) {
      m_freeze_alarm->publish_state(*value & (0x01 << 7));
    }
  }
}

}  // namespace helios_kwl_component
}  // namespace esphome