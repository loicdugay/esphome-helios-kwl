"""
Sous-plateforme sensor — Helios KWL EC 300 Pro
14 capteurs : températures (4), humidité (2), CO₂, vitesse fan,
              boost restant, code défaut, maintenance, bypass, santé, état boost
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_PARTS_PER_MILLION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
)

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_TEMPERATURE_OUTSIDE     = "temperature_outside"
CONF_TEMPERATURE_EXTRACT     = "temperature_extract"
CONF_TEMPERATURE_SUPPLY      = "temperature_supply"
CONF_TEMPERATURE_EXHAUST     = "temperature_exhaust"
CONF_HUMIDITY_SENSOR1        = "humidity_sensor1"
CONF_HUMIDITY_SENSOR2        = "humidity_sensor2"
CONF_CO2_CONCENTRATION       = "co2_concentration"
CONF_FAN_SPEED               = "fan_speed"
CONF_BOOST_REMAINING         = "boost_remaining"
CONF_FAULT_CODE              = "fault_code"
CONF_SERVICE_MONTHS_REMAINING = "service_months_remaining"
CONF_BYPASS_OPEN             = "bypass_open"
CONF_FAULT_INDICATOR         = "fault_indicator"
CONF_BOOST_STATE             = "boost_state"

# ── Icônes personnalisées ──────────────────────────────────────────────────────
ICON_FAN          = "mdi:fan"
ICON_SPEEDOMETER  = "mdi:speedometer"
ICON_TIMER        = "mdi:timer-outline"
ICON_ALERT        = "mdi:alert-circle-outline"
ICON_WRENCH       = "mdi:wrench-clock"
ICON_BYPASS       = "mdi:valve"
ICON_HEART        = "mdi:heart-pulse"
ICON_FIREPLACE    = "mdi:fireplace"

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Températures ──
        cv.Optional(CONF_TEMPERATURE_OUTSIDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
        ),
        cv.Optional(CONF_TEMPERATURE_EXTRACT): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
        ),
        cv.Optional(CONF_TEMPERATURE_SUPPLY): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
        ),
        cv.Optional(CONF_TEMPERATURE_EXHAUST): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_THERMOMETER,
        ),

        # ── Humidité ──
        cv.Optional(CONF_HUMIDITY_SENSOR1): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_WATER_PERCENT,
        ),
        cv.Optional(CONF_HUMIDITY_SENSOR2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_WATER_PERCENT,
        ),

        # ── CO₂ ──
        cv.Optional(CONF_CO2_CONCENTRATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:molecule-co2",
        ),

        # ── Vitesse ventilateur ──
        cv.Optional(CONF_FAN_SPEED): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_SPEEDOMETER,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── Boost restant ──
        cv.Optional(CONF_BOOST_REMAINING): sensor.sensor_schema(
            unit_of_measurement="min",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon=ICON_TIMER,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── Code défaut ──
        cv.Optional(CONF_FAULT_CODE): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            icon=ICON_ALERT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── Mois restants maintenance ──
        cv.Optional(CONF_SERVICE_MONTHS_REMAINING): sensor.sensor_schema(
            unit_of_measurement="mois",
            accuracy_decimals=0,
            icon=ICON_WRENCH,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── État bypass (0=hiver, 1=été) ──
        cv.Optional(CONF_BYPASS_OPEN): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            icon=ICON_BYPASS,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── Santé du système (0=optimal, 1=filtre, 2=défaut) ──
        cv.Optional(CONF_FAULT_INDICATOR): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            icon=ICON_HEART,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),

        # ── État boost (0=normal, 1=plein air, 2=cheminée) ──
        cv.Optional(CONF_BOOST_STATE): sensor.sensor_schema(
            unit_of_measurement="",
            accuracy_decimals=0,
            icon=ICON_FIREPLACE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_TEMPERATURE_OUTSIDE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE_OUTSIDE])
        cg.add(parent.set_temperature_outside_sensor(sens))

    if CONF_TEMPERATURE_EXTRACT in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE_EXTRACT])
        cg.add(parent.set_temperature_extract_sensor(sens))

    if CONF_TEMPERATURE_SUPPLY in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE_SUPPLY])
        cg.add(parent.set_temperature_supply_sensor(sens))

    if CONF_TEMPERATURE_EXHAUST in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE_EXHAUST])
        cg.add(parent.set_temperature_exhaust_sensor(sens))

    if CONF_HUMIDITY_SENSOR1 in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY_SENSOR1])
        cg.add(parent.set_humidity_sensor1(sens))

    if CONF_HUMIDITY_SENSOR2 in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY_SENSOR2])
        cg.add(parent.set_humidity_sensor2(sens))

    if CONF_CO2_CONCENTRATION in config:
        sens = await sensor.new_sensor(config[CONF_CO2_CONCENTRATION])
        cg.add(parent.set_co2_concentration_sensor(sens))

    if CONF_FAN_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_FAN_SPEED])
        cg.add(parent.set_fan_speed_sensor(sens))

    if CONF_BOOST_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_BOOST_REMAINING])
        cg.add(parent.set_boost_remaining_sensor(sens))

    if CONF_FAULT_CODE in config:
        sens = await sensor.new_sensor(config[CONF_FAULT_CODE])
        cg.add(parent.set_fault_code_sensor(sens))

    if CONF_SERVICE_MONTHS_REMAINING in config:
        sens = await sensor.new_sensor(config[CONF_SERVICE_MONTHS_REMAINING])
        cg.add(parent.set_service_months_remaining_sensor(sens))

    if CONF_BYPASS_OPEN in config:
        sens = await sensor.new_sensor(config[CONF_BYPASS_OPEN])
        cg.add(parent.set_bypass_open_sensor(sens))

    if CONF_FAULT_INDICATOR in config:
        sens = await sensor.new_sensor(config[CONF_FAULT_INDICATOR])
        cg.add(parent.set_fault_indicator_sensor(sens))

    if CONF_BOOST_STATE in config:
        sens = await sensor.new_sensor(config[CONF_BOOST_STATE])
        cg.add(parent.set_boost_state_sensor(sens))
