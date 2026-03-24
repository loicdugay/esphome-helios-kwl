import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    UNIT_CELSIUS,
    UNIT_PERCENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    STATE_CLASS_MEASUREMENT,
)

from .. import CONF_HELIOS_KWL_ID, HELIOS_KWL_COMPONENT_SCHEMA

TYPES = {
    "fan_speed": sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temperature_outside": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temperature_exhaust": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temperature_inside": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temperature_incoming": sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "humidity1": sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "humidity2": sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_HUMIDITY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

CONFIG_SCHEMA = HELIOS_KWL_COMPONENT_SCHEMA.extend({cv.Optional(type): schema for type, schema in TYPES.items()})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(parent, f"set_{type}_sensor")(sens))