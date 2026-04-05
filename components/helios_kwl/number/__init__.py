"""
Sous-plateforme number — Helios KWL EC 300 Pro
12 réglages numériques : vitesses, températures, seuils, entretien, puissance
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_MINUTE,
)

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Classe C++ ────────────────────────────────────────────────────────────────
HeliosKwlNumber = helios_kwl_ns.class_("HeliosKwlNumber", number.Number)

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_BASIC_FAN_SPEED        = "basic_fan_speed"
CONF_MAX_FAN_SPEED          = "max_fan_speed"
CONF_BYPASS_TEMP            = "bypass_temp"
CONF_PREHEATING_TEMP        = "preheating_temp"
CONF_FROST_ALARM_TEMP       = "frost_alarm_temp"
CONF_FROST_HYSTERESIS       = "frost_hysteresis"
CONF_CO2_SETPOINT           = "co2_setpoint"
CONF_HUMIDITY_SETPOINT      = "humidity_setpoint"
CONF_REGULATION_INTERVAL    = "regulation_interval"
CONF_SUPPLY_FAN_PERCENT     = "supply_fan_percent"
CONF_EXHAUST_FAN_PERCENT    = "exhaust_fan_percent"
CONF_SERVICE_INTERVAL       = "service_interval"

# ── Unités personnalisées ──────────────────────────────────────────────────────
UNIT_PPM   = "ppm"
UNIT_STEPS = ""
UNIT_MONTH = "mois"

# ── Helper : schéma number ────────────────────────────────────────────────────
def _number_schema(min_val, max_val, step, unit, icon):
    return number.number_schema(
        HeliosKwlNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=unit,
        icon=icon,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=min_val): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=max_val): cv.float_,
            cv.Optional(CONF_STEP, default=step): cv.positive_float,
        }
    )


# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),
        cv.Optional(CONF_BASIC_FAN_SPEED):     _number_schema(1,   8,    1, UNIT_STEPS,   "mdi:fan-minus"),
        cv.Optional(CONF_MAX_FAN_SPEED):       _number_schema(1,   8,    1, UNIT_STEPS,   "mdi:fan-plus"),
        cv.Optional(CONF_BYPASS_TEMP):         _number_schema(0,   25,   1, UNIT_CELSIUS, "mdi:thermometer-chevron-up"),
        cv.Optional(CONF_PREHEATING_TEMP):     _number_schema(-6,  15,   1, UNIT_CELSIUS, "mdi:snowflake-thermometer"),
        cv.Optional(CONF_FROST_ALARM_TEMP):    _number_schema(-6,  15,   1, UNIT_CELSIUS, "mdi:thermometer-alert"),
        cv.Optional(CONF_FROST_HYSTERESIS):    _number_schema(1,   10,   1, UNIT_CELSIUS, "mdi:thermometer-lines"),
        cv.Optional(CONF_CO2_SETPOINT):        _number_schema(500, 2000, 50, UNIT_PPM,    "mdi:molecule-co2"),
        cv.Optional(CONF_HUMIDITY_SETPOINT):   _number_schema(1,   99,   1, UNIT_PERCENT, "mdi:water-percent"),
        cv.Optional(CONF_REGULATION_INTERVAL): _number_schema(1,   15,   1, UNIT_MINUTE,  "mdi:clock-outline"),
        cv.Optional(CONF_SUPPLY_FAN_PERCENT):  _number_schema(65,  100,  1, UNIT_PERCENT, "mdi:fan-speed-1"),
        cv.Optional(CONF_EXHAUST_FAN_PERCENT): _number_schema(65,  100,  1, UNIT_PERCENT, "mdi:fan-speed-2"),
        cv.Optional(CONF_SERVICE_INTERVAL):    _number_schema(1,   15,   1, UNIT_MONTH,   "mdi:wrench-clock"),
    }
)

# ── Table : clé config → setter sur le parent ─────────────────────────────────
_MAPPING = [
    (CONF_BASIC_FAN_SPEED,     "set_basic_fan_speed_number"),
    (CONF_MAX_FAN_SPEED,       "set_max_fan_speed_number"),
    (CONF_BYPASS_TEMP,         "set_bypass_temp_number"),
    (CONF_PREHEATING_TEMP,     "set_preheating_temp_number"),
    (CONF_FROST_ALARM_TEMP,    "set_frost_alarm_temp_number"),
    (CONF_FROST_HYSTERESIS,    "set_frost_hysteresis_number"),
    (CONF_CO2_SETPOINT,        "set_co2_setpoint_number"),
    (CONF_HUMIDITY_SETPOINT,   "set_humidity_setpoint_number"),
    (CONF_REGULATION_INTERVAL, "set_regulation_interval_number"),
    (CONF_SUPPLY_FAN_PERCENT,  "set_supply_fan_percent_number"),
    (CONF_EXHAUST_FAN_PERCENT, "set_exhaust_fan_percent_number"),
    (CONF_SERVICE_INTERVAL,    "set_service_interval_number"),
]


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    for conf_key, setter in _MAPPING:
        if conf_key not in config:
            continue
        num = await number.new_number(
            config[conf_key],
            min_value=config[conf_key][CONF_MIN_VALUE],
            max_value=config[conf_key][CONF_MAX_VALUE],
            step=config[conf_key][CONF_STEP],
        )
        cg.add(getattr(parent, setter)(num))
