#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "Run this installer with sudo." >&2
  exit 1
fi

ARCH="$(uname -m)"
if [[ "${ARCH}" != "aarch64" ]]; then
  echo "Ember requires 64-bit Raspberry Pi OS (detected ${ARCH})." >&2
  exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
INSTALL_DIR="/opt/ember"
DATA_DIR="/var/lib/ember"
CONFIG_DIR="/etc/ember"

echo "Installing system packages..."
apt-get update
apt-get install -y ca-certificates curl git build-essential cmake libopenblas-dev \
  python3 python3-venv python3-pip openssl ffmpeg

if ! id ember >/dev/null 2>&1; then
  useradd --system --home-dir "${INSTALL_DIR}" --create-home --shell /usr/sbin/nologin ember
fi

install -d -o ember -g ember "${INSTALL_DIR}/gateway" "${INSTALL_DIR}/models/whisper" \
  "${INSTALL_DIR}/models/piper" "${DATA_DIR}/audio" "${CONFIG_DIR}"
cp -a "${SOURCE_DIR}/ember_gateway" "${SOURCE_DIR}/requirements.txt" \
  "${SOURCE_DIR}/personality.txt" "${INSTALL_DIR}/gateway/"
chown -R ember:ember "${INSTALL_DIR}" "${DATA_DIR}"

echo "Installing Ember Python environment and Piper..."
python3 -m venv "${INSTALL_DIR}/venv"
"${INSTALL_DIR}/venv/bin/pip" install --upgrade pip
"${INSTALL_DIR}/venv/bin/pip" install -r "${INSTALL_DIR}/gateway/requirements.txt" 'piper-tts[http]'
runuser -u ember -- "${INSTALL_DIR}/venv/bin/python" -m piper.download_voices \
  --data-dir "${INSTALL_DIR}/models/piper" en_GB-alba-medium

echo "Building whisper.cpp v1.9.1 for the Pi 5..."
if [[ ! -d "${INSTALL_DIR}/whisper.cpp/.git" ]]; then
  runuser -u ember -- git clone --branch v1.9.1 --depth 1 \
    https://github.com/ggml-org/whisper.cpp.git "${INSTALL_DIR}/whisper.cpp"
fi
runuser -u ember -- cmake -S "${INSTALL_DIR}/whisper.cpp" \
  -B "${INSTALL_DIR}/whisper.cpp/build" -DCMAKE_BUILD_TYPE=Release \
  -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS
runuser -u ember -- cmake --build "${INSTALL_DIR}/whisper.cpp/build" -j4 --target whisper-server
runuser -u ember -- "${INSTALL_DIR}/whisper.cpp/models/download-ggml-model.sh" \
  base.en "${INSTALL_DIR}/models/whisper"

echo "Installing Ollama and the local language model..."
if ! command -v ollama >/dev/null 2>&1; then
  curl -fsSL https://ollama.com/install.sh | sh
fi
systemctl enable --now ollama
runuser -u ollama -- env HOME=/usr/share/ollama ollama pull llama3.2:3b

if [[ ! -f "${CONFIG_DIR}/ember.env" ]]; then
  TOKEN="$(openssl rand -hex 32)"
  sed "s/replace-with-a-long-random-value/${TOKEN}/" "${SOURCE_DIR}/.env.example" \
    > "${CONFIG_DIR}/ember.env"
  chmod 640 "${CONFIG_DIR}/ember.env"
  chown root:ember "${CONFIG_DIR}/ember.env"
fi
if ! grep -q '^EMBER_TIMEZONE=' "${CONFIG_DIR}/ember.env"; then
  echo 'EMBER_TIMEZONE=Africa/Johannesburg' >> "${CONFIG_DIR}/ember.env"
fi

install -m 0644 "${SOURCE_DIR}/systemd/whisper-ember.service" /etc/systemd/system/
install -m 0644 "${SOURCE_DIR}/systemd/piper-ember.service" /etc/systemd/system/
install -m 0644 "${SOURCE_DIR}/systemd/ember-gateway.service" /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now whisper-ember piper-ember ember-gateway

PI_ADDRESS="$(hostname -I | awk '{print $1}')"
echo
echo "Ember gateway installed at http://${PI_ADDRESS}:8088"
echo "Shared token (copy this once into the Fire configuration):"
sed -n 's/^EMBER_TOKEN=//p' "${CONFIG_DIR}/ember.env"
echo
echo "Check it with: sudo systemctl status ember-gateway whisper-ember piper-ember ollama"
