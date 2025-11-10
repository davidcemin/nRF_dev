#!/bin/bash
# Patch to remove WiFi driver dependency check

KCONFIG_FILE="/opt/nordic/ncs/v2.9.2/zephyr/drivers/wifi/nrf_wifi/Kconfig.nrfwifi"

# Backup original
cp "$KCONFIG_FILE" "$KCONFIG_FILE.backup"

# Remove the depends on line
sed -i.tmp '/depends on.*DT_HAS_NORDIC_NRF700/,/DT_HAS_NORDIC_NRF7000_QSPI_ENABLED/d' "$KCONFIG_FILE"

echo "Patched $KCONFIG_FILE"
echo "Backup saved as $KCONFIG_FILE.backup"
