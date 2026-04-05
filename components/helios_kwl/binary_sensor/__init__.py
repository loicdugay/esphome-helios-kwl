"""
Sous-plateforme binary_sensor — Helios KWL EC 300 Pro
9 capteurs binaires : états physiques et alarmes
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

# ── Clés de configuration ──────────────────────────────────────────────────────
CONF_PREHEATING_ACTIVE   = "preheating_active"
CONF_FREEZE_ALARM        = "freeze_alarm"
CONF_CO2_ALARM           = "co2_alarm"
CONF_FILTER_MAINTENANCE  = "filter_maintenance"
CONF_HEATING_INDICATOR   = "heating_indicator"
CONF_SUPPLY_FAN_RUNNING  = "supply_fan_running"
CONF_EXHAUST_FAN_RUNNING = "exhaust_fan_running"
CONF_EXTERNAL_CONTACT    = "external_contact"
CONF_FAULT_RELAY         = "fault_relay"

# ── Schéma de la sous-plateforme ──────────────────────────────────────────────
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),

        # ── Auto-dégivrage (préchauffage résistance avant échangeur) ──
        cv.Optional(CONF_PREHEATING_ACTIVE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_HEAT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:snowflake-melt",
        ),

        # ── Alerte givre (échangeur risque de geler) ──
        cv.Optional(CONF_FREEZE_ALARM): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:snowflake-alert",
        ),

        # ── Alerte CO₂ >5000 ppm ──
        cv.Optional(CONF_CO2_ALARM): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:molecule-co2",
        ),

        # ── Maintenance filtre requise ──
        cv.Optional(CONF_FILTER_MAINTENANCE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:air-filter",
        ),

        # ── Appoint de chaleur (post-échangeur, non installé sur EC300Pro) ──
        cv.Optional(CONF_HEATING_INDICATOR): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_HEAT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:radiator",
        ),

        # ── Ventilateur soufflage (logique inversée : 0=marche) ──
        cv.Optional(CONF_SUPPLY_FAN_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:fan",
        ),

        # ── Ventilateur extraction (logique inversée : 0=marche) ──
        cv.Optional(CONF_EXHAUST_FAN_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:fan-chevron-down",
        ),

        # ── Contact externe (bornes S) ──
        cv.Optional(CONF_EXTERNAL_CONTACT): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:electric-switch",
        ),

        # ── Relais défaut (0=ouvert/défaut, 1=fermé/normal) ──
        cv.Optional(CONF_FAULT_RELAY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:relay",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    if CONF_PREHEATING_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PREHEATING_ACTIVE])
        cg.add(parent.set_preheating_active_sensor(sens))

    if CONF_FREEZE_ALARM in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FREEZE_ALARM])
        cg.add(parent.set_freeze_alarm_sensor(sens))

    if CONF_CO2_ALARM in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CO2_ALARM])
        cg.add(parent.set_co2_alarm_sensor(sens))

    if CONF_FILTER_MAINTENANCE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FILTER_MAINTENANCE])
        cg.add(parent.set_filter_maintenance_sensor(sens))

    if CONF_HEATING_INDICATOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HEATING_INDICATOR])
        cg.add(parent.set_heating_indicator_sensor(sens))

    if CONF_SUPPLY_FAN_RUNNING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_SUPPLY_FAN_RUNNING])
        cg.add(parent.set_supply_fan_running_sensor(sens))

    if CONF_EXHAUST_FAN_RUNNING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_EXHAUST_FAN_RUNNING])
        cg.add(parent.set_exhaust_fan_running_sensor(sens))

    if CONF_EXTERNAL_CONTACT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_EXTERNAL_CONTACT])
        cg.add(parent.set_external_contact_sensor(sens))

    if CONF_FAULT_RELAY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAULT_RELAY])
        cg.add(parent.set_fault_relay_sensor(sens))
