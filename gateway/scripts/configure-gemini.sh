#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "Run this setup with sudo." >&2
  exit 1
fi

CONFIG_FILE="/etc/ember/ember.env"
if [[ ! -f "${CONFIG_FILE}" ]]; then
  echo "Ember is not installed; run scripts/install-pi.sh first." >&2
  exit 1
fi

read -rsp "Paste the Gemini API key (input is hidden): " GEMINI_KEY
echo
if [[ ${#GEMINI_KEY} -lt 20 ]]; then
  echo "The API key looks too short; no changes were made." >&2
  exit 1
fi

umask 077
TEMP_FILE="$(mktemp /etc/ember/ember.env.XXXXXX)"
cleanup() {
  rm -f -- "${TEMP_FILE}"
}
trap cleanup EXIT

awk '!/^(LLM_PROVIDER|LLM_FALLBACK_TO_OLLAMA|GEMINI_API_KEY|GEMINI_BASE_URL|GEMINI_MODEL)=/' \
  "${CONFIG_FILE}" > "${TEMP_FILE}"
printf '%s\n' \
  'LLM_PROVIDER=gemini' \
  'LLM_FALLBACK_TO_OLLAMA=true' \
  "GEMINI_API_KEY=${GEMINI_KEY}" \
  'GEMINI_BASE_URL=https://generativelanguage.googleapis.com/v1beta' \
  'GEMINI_MODEL=gemini-3.5-flash-lite' >> "${TEMP_FILE}"

install -o root -g ember -m 0640 "${TEMP_FILE}" "${CONFIG_FILE}"
GEMINI_KEY=""
systemctl restart ember-gateway
sleep 2

TOKEN="$(sed -n 's/^EMBER_TOKEN=//p' "${CONFIG_FILE}")"
curl -fsS -H "X-Ember-Token: ${TOKEN}" http://127.0.0.1:8088/health
echo
echo "Gemini is configured. Ollama remains available as the automatic fallback."
