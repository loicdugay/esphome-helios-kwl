#pragma once
#include "esphome/components/select/select.h"
#include "../helios_kwl.h"

namespace esphome {
namespace helios_kwl {

class HeliosKwlSelectBase : public select::Select {
 public:
  void set_parent(HeliosKwlComponent *parent) { parent_ = parent; }
 protected:
  HeliosKwlComponent *parent_{nullptr};
};

// SANS accents — aligne avec OPTIONS_BOOST_FIREPLACE Python
class HeliosKwlBoostFireplaceSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_) parent_->control_boost_fireplace_mode(value == "Cycle Plein Air");
    publish_state(value);
  }
};

class HeliosKwlHumidityAutoSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_) parent_->control_humidity_auto_search(value == "Apprentissage auto");
    publish_state(value);
  }
};

// SANS accents — "Ventilation maximale forcee"
class HeliosKwlMaxSpeedContSelect : public HeliosKwlSelectBase {
 protected:
  void control(const std::string &value) override {
    if (parent_) parent_->control_max_speed_continuous(value == "Ventilation maximale forcee");
    publish_state(value);
  }
};

}  // namespace helios_kwl
}  // namespace esphome
