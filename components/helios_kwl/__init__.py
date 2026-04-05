"""
Composant ESPHome — Helios KWL EC 300 Pro
Protocole RS485 Vallox/Helios, phase 1
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ADDRESS

CODEOWNERS = ["@loicdugay"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = [
    "sensor",
    "binary_sensor",
    "switch",
    "number",
    "select",
    "button",
    "text_sensor",
    "fan",
]
MULTI_CONF = False

# ── Namespace C++ ──────────────────────────────────────────────────────────────
helios_kwl_ns = cg.esphome_ns.namespace("helios_kwl")
HeliosKwlComponent = helios_kwl_ns.class_(
    "HeliosKwlComponent", uart.UARTDevice, cg.PollingComponent
)

# ── Clé de configuration ──────────────────────────────────────────────────────
CONF_HELIOS_KWL_ID = "helios_kwl_id"

# ── Schéma principal ──────────────────────────────────────────────────────────
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HeliosKwlComponent),
            cv.Optional(CONF_ADDRESS, default=0x2F): cv.hex_uint8_t,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
