#!/usr/bin/env python3
"""Controlled latency baseline for the Ember gateway.

Builds a fixed, identical short speech prompt (so results are comparable across
runs and providers) and posts it to /v1/voice N times, capturing the per-stage
gateway timings and the response audio size. Older firmware style: each run may
use the same anonymous device id (fixed history) or a fresh one (clean first
turn). Use --fresh-device to get a clean single-turn baseline per repetition.

Dependencies: python3 standard library only (run from /opt/ember/venv or system).
"""

from __future__ import annotations

import argparse
import io
import json
import os
import statistics
import subprocess
import sys
import time
import urllib.error
import urllib.request
import wave


def http_bytes(
    url: str,
    method: str = "GET",
    headers: dict | None = None,
    body: bytes | None = None,
) -> tuple[int, bytes | None]:
    request = urllib.request.Request(url, data=body, method=method, headers=headers or {})
    try:
        with urllib.request.urlopen(request, timeout=180) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def synthesize_control(
    piper_url: str,
    control_text: str,
    output_16k: str,
) -> None:
    """Synthesize the control phrase at 16 kHz mono via Piper + ffmpeg."""
    status, body = http_bytes(
        f"{piper_url}/synthesize",
        method="POST",
        headers={"Content-Type": "application/json"},
        body=json.dumps({"text": control_text}).encode(),
    )
    if status != 200 or not body:
        raise RuntimeError(f"piper synthesis failed (status {status})")
    raw = os.path.join(os.path.dirname(output_16k), "ember_bench_raw.wav")
    with open(raw, "wb") as handle:
        handle.write(body)
    subprocess.run(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", raw, "-ar", "16000", "-ac", "1", output_16k],
        check=True,
    )
    with wave.open(output_16k, "rb") as handle:
        if handle.getframerate() != 16000:
            raise RuntimeError("control WAV did not convert to 16 kHz")


def control_wav_zero_fill(wav_path: str) -> bytes:
    with open(wav_path, "rb") as handle:
        return handle.read()


def build_multipart(wav_bytes: bytes, boundary: str) -> tuple[bytes, str]:
    prefix = (
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; "
        f"filename=\"bench.wav\"\r\nContent-Type: audio/wav\r\n\r\n"
    ).encode()
    suffix = f"\r\n--{boundary}--\r\n".encode()
    return prefix + wav_bytes + suffix, str(len(prefix) + len(wav_bytes) + len(suffix))


def run_turn(
    host: str,
    token: str,
    device_id: str,
    wav_bytes: bytes,
) -> dict:
    boundary = f"----EmberBench{uuid_hex()}"
    body, content_length = build_multipart(wav_bytes, boundary)
    status, data = http_bytes(
        f"{host}/v1/voice",
        method="POST",
        headers={
            "X-Ember-Token": token,
            "X-Ember-Device": device_id,
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(content_length),
        },
        body=body,
    )
    if status != 200:
        snippet = data[:200] if data else ""
        raise RuntimeError(f"gateway returned {status}: {snippet!r}")
    payload = json.loads(data.decode())
    audio_url = payload.get("audio_url")
    download_ms, audio_bytes = 0, 0
    if audio_url:
        started = time.perf_counter()
        audio_status, audio_data = http_bytes(f"{host}{audio_url}", headers={"X-Ember-Token": token})
        download_ms = (time.perf_counter() - started) * 1000
        audio_bytes = len(audio_data) if audio_status == 200 else 0
    return {
        "provider": payload.get("conversation_provider"),
        "transcript": payload.get("transcript", ""),
        "reply_len": len(payload.get("reply", "") or ""),
        "audio_bytes": audio_bytes,
        "timings": payload.get("timings_ms", {}),
        "download_ms": download_ms,
    }


def uuid_hex() -> str:
    import uuid
    return uuid.uuid4().hex


def main() -> int:
    parser = argparse.ArgumentParser(description="Controlled Ember latency baseline")
    parser.add_argument("--token", required=True)
    parser.add_argument("--host", default="http://127.0.0.1:8088")
    parser.add_argument("--piper", default="http://127.0.0.1:5000")
    parser.add_argument("--wav", help="existing 16k prompt WAV to reuse")
    parser.add_argument("--control", default="peel a bell and a turtle at noon")
    parser.add_argument("--reps", type=int, default=5)
    parser.add_argument("--fresh-device", action="store_true",
                        help="use a fresh device id per rep for a clean single turn")
    parser.add_argument("--out", default="/tmp/ember_bench.wav", help="where to cache the control WAV")
    args = parser.parse_args()

    if args.wav:
        wav_bytes = open(args.wav, "rb").read()
    else:
        if not os.path.exists(args.out):
            synthesize_control(args.piper, args.control, args.out)
            print(f"[bench] control WAV at {args.out}")
        wav_bytes = open(args.out, "rb").read()
    print(f"[bench] control WAV {len(wav_bytes)} bytes; reps={args.reps}")

    rows = []
    group_name = "fresh" if args.fresh_device else ("fixed")
    id_base = uuid_hex() if args.fresh_device else "bench-fixed"
    for rep in range(1, args.reps + 1):
        device_id = id_base if not args.fresh_device else f"bench-{uuid_hex()}"
        started = time.perf_counter()
        row = run_turn(args.host, args.token, device_id, wav_bytes)
        row["fire_total"] = (time.perf_counter() - started) * 1000
        rows.append(row)
        t = row["timings"]
        print(
            f"  rep{rep:>2} {row['provider']:<12} "
            f"transcribe={t.get('transcription'):>5} conv={t.get('conversation'):>5} "
            f"synth={t.get('synthesis'):>5} gw_total={t.get('gateway_total'):>5} "
            f"aud={row['reply_len']:>3}ch/{(row['audio_bytes']//1024):>4}KiB dl={row['download_ms']:.0f}ms"
        )

    wanted = ["upload_validation", "conversation", "synthesis", "gateway_total"]
    print("\n  stage              avg    median");
    for stage in wanted:
        col = [r["timings"].get(stage, 0) for r in rows]
        print(f"  {stage:<18} {statistics.mean(col):6.0f} {statistics.median(col):6.0f}")
    dl = [r["download_ms"] for r in rows]
    print(f"  {'audio_download':<18} {statistics.mean(dl):6.0f} {statistics.median(dl):6.0f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())