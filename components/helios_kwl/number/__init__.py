"""number — Phase 2C — avec RawExpression fix"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP, ENTITY_CATEGORY_CONFIG, UNIT_CELSIUS, UNIT_PERCENT, UNIT_MINUTE)
from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent

HeliosKwlNumber = helios_kwl_ns.class_("HeliosKwlNumber", number.Number)

def _ns(mn,mx,st,u,ic):
    return number.number_schema(HeliosKwlNumber, entity_category=ENTITY_CATEGORY_CONFIG, unit_of_measurement=u, icon=ic).extend({
        cv.Optional(CONF_MIN_VALUE, default=mn): cv.float_, cv.Optional(CONF_MAX_VALUE, default=mx): cv.float_, cv.Optional(CONF_STEP, default=st): cv.positive_float})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_HELIOS_KWL_ID): cv.use_id(HeliosKwlComponent),
    cv.Optional("basic_fan_speed"):     _ns(1,8,1,"","mdi:fan-minus"),
    cv.Optional("max_fan_speed"):       _ns(1,8,1,"","mdi:fan-plus"),
    cv.Optional("bypass_temp"):         _ns(0,25,1,UNIT_CELSIUS,"mdi:thermometer-chevron-up"),
    cv.Optional("preheating_temp"):     _ns(-6,15,1,UNIT_CELSIUS,"mdi:snowflake-thermometer"),
    cv.Optional("frost_alarm_temp"):    _ns(-6,15,1,UNIT_CELSIUS,"mdi:thermometer-alert"),
    cv.Optional("frost_hysteresis"):    _ns(1,10,1,UNIT_CELSIUS,"mdi:thermometer-lines"),
    cv.Optional("co2_setpoint"):        _ns(500,2000,50,"ppm","mdi:molecule-co2"),
    cv.Optional("humidity_setpoint"):   _ns(1,99,1,UNIT_PERCENT,"mdi:water-percent"),
    cv.Optional("regulation_interval"): _ns(1,15,1,UNIT_MINUTE,"mdi:clock-outline"),
    cv.Optional("supply_fan_percent"):  _ns(65,100,1,UNIT_PERCENT,"mdi:fan-speed-1"),
    cv.Optional("exhaust_fan_percent"): _ns(65,100,1,UNIT_PERCENT,"mdi:fan-speed-2"),
    cv.Optional("service_interval"):    _ns(1,15,1,"mois","mdi:wrench-clock"),
})

_MAP = [
    ("basic_fan_speed","set_basic_fan_speed_number","set_uint8_setter","control_basic_fan_speed"),
    ("max_fan_speed","set_max_fan_speed_number","set_uint8_setter","control_max_fan_speed"),
    ("bypass_temp","set_bypass_temp_number","set_float_setter","control_bypass_temp"),
    ("preheating_temp","set_preheating_temp_number","set_float_setter","control_preheating_temp"),
    ("frost_alarm_temp","set_frost_alarm_temp_number","set_float_setter","control_frost_alarm_temp"),
    ("frost_hysteresis","set_frost_hysteresis_number","set_float_setter","control_frost_hysteresis"),
    ("co2_setpoint","set_co2_setpoint_number","set_uint16_setter","control_co2_setpoint"),
    ("humidity_setpoint","set_humidity_setpoint_number","set_uint8_setter","control_humidity_setpoint"),
    ("regulation_interval","set_regulation_interval_number","set_uint8_setter","control_regulation_interval"),
    ("supply_fan_percent","set_supply_fan_percent_number","set_uint8_setter","control_supply_fan_percent"),
    ("exhaust_fan_percent","set_exhaust_fan_percent_number","set_uint8_setter","control_exhaust_fan_percent"),
    ("service_interval","set_service_interval_number","set_uint8_setter","control_service_interval"),
]

async def to_code(config):
    parent = await cg.get_variable(config[CONF_HELIOS_KWL_ID])
    for ck, ps, nsm, cm in _MAP:
        if ck not in config: continue
        num = await number.new_number(config[ck], min_value=config[ck][CONF_MIN_VALUE], max_value=config[ck][CONF_MAX_VALUE], step=config[ck][CONF_STEP])
        cg.add(getattr(parent, ps)(num))
        cg.add(num.set_parent(parent))
        cg.add(getattr(num, nsm)(cg.RawExpression(f"&helios_kwl::HeliosKwlComponent::{cm}")))
