#!/usr/bin/env python3
"""Run every standalone Tapehead native regression suite."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEST_SCRIPTS = (
	"test_audio_hardening.py",
	"test_track_trim.py",
	"test_apc40_mk2.py",
    "test_sample_morph.py",
    "test_tapehead_actions.py",
	"test_midi_map.py",
	"test_midi_surface.py",
    "test_undo_transactions.py",
	"test_baker_allocator.py",
	"test_baker_timeline_planner.py",
	"test_baker_adaptive_xm.py",
	"test_baker_adaptive_patterns.py",
	"test_tapesister_protocol.py",
	"test_tapesister_ack.py",
	"test_tapesister_config.py",
	"test_wav_metadata.py",
	"test_tapesister_exchange_wiring.py",
	"test_sysreq_layout.py",
	"test_overlay_layout.py",
	"test_baker_adaptive_save.py",
	"test_baker_assets.py",
	"test_baker_dialog_layout.py",
	"test_sample_launcher_native.py",
	"test_sample_launcher_banks.py",
	"test_sample_matrix_browser.py",
	"test_fasttracks_native.py",
	"test_track_length_control.py",
	"test_hybrid_transport_visuals.py",
	"test_pr43_len_performance.py",
	"test_microtonal.py",
	"test_pattern_layout.py",
	"test_pattern_colors.py",
	"test_universal_palette.py",
	"test_pattern_palette_native.py",
	"test_pattern_block_extract.py",
	"test_tuning_interpolation.py",
    "test_fasttracks_transport.py",
    "test_multichannel_native.py",
    "test_audio_bus_delivery.py",
    "test_jack_native.py",
    "test_poly_matrix_native.py",
	"test_midi_dub_config.py",
	"test_splash_config.py",
	"test_video_damage.py",
)


def main() -> None:
    for script_name in TEST_SCRIPTS:
        print(f"\n== {script_name} ==", flush=True)
        subprocess.run(
            [sys.executable, str(ROOT / "scripts" / script_name)],
            check=True,
            cwd=ROOT,
        )

    print("\nAll Tapehead native regression suites passed.")


if __name__ == "__main__":
    main()
