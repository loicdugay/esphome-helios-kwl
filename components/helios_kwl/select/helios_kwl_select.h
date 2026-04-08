#pragma once
#include "esphome/components/select/select.h"
#include "../helios_kwl.h"
namespace esphome { namespace helios_kwl {
class HeliosKwlSelectBase : public select::Select {
 public: void set_parent(HeliosKwlComponent *p) { parent_ = p; }
 protected: HeliosKwlComponent *parent_{nullptr};
};
class HeliosKwlBoostFireplaceSelect : public HeliosKwlSelectBase {
 protected: void control(const std::string &v) override { if (parent_) parent_->control_boost_fireplace_mode(v == "Cycle Plein Air"); publish_state(v); }
};
class HeliosKwlHumidityAutoSelect : public HeliosKwlSelectBase {
 protected: void control(const std::string &v) override { if (parent_) parent_->control_humidity_auto_search(v == "Apprentissage auto"); publish_state(v); }
};
class HeliosKwlMaxSpeedContSelect : public HeliosKwlSelectBase {
 protected: void control(const std::string &v) override { if (parent_) parent_->control_max_speed_continuous(v == "Ventilation maximale forcee"); publish_state(v); }
};
} }
