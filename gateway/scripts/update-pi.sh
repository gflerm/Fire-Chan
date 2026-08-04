#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "Run this updater with sudo." >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
INSTALL_DIR="/opt/ember"
CONFIG_FILE="/etc/ember/ember.env"

if [[ ! -x "${INSTALL_DIR}/venv/bin/python" || ! -f "${CONFIG_FILE}" ]]; then
  echo "Ember is not installed; run scripts/install-pi.sh first." >&2
  exit 1
fi

systemctl stop ember-gateway
install -d "${INSTALL_DIR}/gateway/ember_gateway"
cp -a "${SOURCE_DIR}/ember_gateway/." "${INSTALL_DIR}/gateway/ember_gateway/"
cp -a "${SOURCE_DIR}/requirements.txt" "${SOURCE_DIR}/personality.txt" \
  "${INSTALL_DIR}/gateway/"
chown -R ember:ember "${INSTALL_DIR}/gateway"
"${INSTALL_DIR}/venv/bin/pip" install -r "${INSTALL_DIR}/gateway/requirements.txt"

if ! grep -q '^EMBER_TIMEZONE=' "${CONFIG_FILE}"; then
  echo 'EMBER_TIMEZONE=Africa/Johannesburg' >> "${CONFIG_FILE}"
fi

systemctl start ember-gateway
sleep 2
TOKEN="$(sed -n 's/^EMBER_TOKEN=//p' "${CONFIG_FILE}")"
curl -fsS -H "X-Ember-Token: ${TOKEN}" http://127.0.0.1:8088/health
echo
