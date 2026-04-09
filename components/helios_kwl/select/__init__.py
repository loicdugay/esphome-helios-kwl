"""select — options SANS accents"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG
from .. import helios_kwl_ns, CONF_HELIOS_KWL_ID, HeliosKwlComponent
HeliosKwlBoostFireplaceSelect=helios_kwl_ns.class_("HeliosKwlBoostFireplaceSelect",select.Select)
HeliosKwlHumidityAutoSelect=helios_kwl_ns.class_("HeliosKwlHumidityAutoSelect",select.Select)
HeliosKwlMaxSpeedContSelect=helios_kwl_ns.class_("HeliosKwlMaxSpeedContSelect",select.Select)
OPTIONS_BOOST_FIREPLACE=["Cycle Cheminee","Cycle Plein Air"]
OPTIONS_HUMIDITY_AUTO=["Seuil manuel","Apprentissage auto"]
OPTIONS_MAX_SPEED=["Normal","Ventilation maximale forcee"]
CONFIG_SCHEMA=cv.Schema({cv.GenerateID(CONF_HELIOS_KWL_ID):cv.use_id(HeliosKwlComponent),cv.Optional("boost_fireplace_mode"):select.select_schema(HeliosKwlBoostFireplaceSelect,entity_category=ENTITY_CATEGORY_CONFIG,icon="mdi:fireplace"),cv.Optional("humidity_auto_search"):select.select_schema(HeliosKwlHumidityAutoSelect,entity_category=ENTITY_CATEGORY_CONFIG,icon="mdi:water-percent-alert"),cv.Optional("max_speed_continuous"):select.select_schema(HeliosKwlMaxSpeedContSelect,entity_category=ENTITY_CATEGORY_CONFIG,icon="mdi:fan-speed-3")})
async def to_code(config):
    parent=await cg.get_variable(config[CONF_HELIOS_KWL_ID])
    for key,opts in[("boost_fireplace_mode",OPTIONS_BOOST_FIREPLACE),("humidity_auto_search",OPTIONS_HUMIDITY_AUTO),("max_speed_continuous",OPTIONS_MAX_SPEED)]:
        if key in config:
            sel=await select.new_select(config[key],options=opts);cg.add(sel.set_parent(parent));cg.add(getattr(parent,f"set_{key}_select")(sel))
