"""Wider update slots for a board with 4 MB of flash (firmware 0.33.1 of those boards), and the way a screen gets them.

`flash_layout:` builds a screen with partitions-4mb-wide.csv, beside this file, in place of ESPHome's own table for 4 MB:
the settings move to the 16 KB in front of the first slot that nothing used, and each update slot grows from 1,835,008
to 2,031,616 bytes. The component (flash_layout.h) moves a screen that is out there to that table with everything it
kept. packages/hardware/flash-4mb.yaml includes it for every board with 4 MB of flash, and packages/bridge.yaml is the
smallest firmware that has it; docs/FLASH_LAYOUT.md has the whole story.
"""
import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, ota
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["esp32"]
_LOGGER = logging.getLogger(__name__)

WIDE_PARTITIONS = Path(__file__).parent / "partitions-4mb-wide.csv"

flash_layout_ns = cg.esphome_ns.namespace("flash_layout")
FlashLayout = flash_layout_ns.class_("FlashLayout", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(FlashLayout)}).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # The settings are copied once more when an update begins (FlashLayout::on_ota_global_state).
    ota.request_ota_state_listeners()
    # The wide table as this build's partitions.csv, the way `esp32: partitions:` hands ESPHome a file. A table the
    # screen's own YAML names there was registered first and stays: the component then finds a table it does not know
    # on the screen and leaves its flash alone.
    if not esp32.add_extra_build_file("partitions.csv", WIDE_PARTITIONS):
        _LOGGER.warning("This screen names a partition table of its own, so it keeps that one")
