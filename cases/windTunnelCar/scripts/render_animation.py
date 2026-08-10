#!/usr/bin/env python3
"""
Fixed camera + air flowing from the front.
Animates "comet" segments sliding along CFD streamlines (reliable offscreen).
30 s @ 30 fps.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np
import pyvista as pv
from PIL import Image

pv.OFF_SCREEN = True

CASE = Path(__file__).resolve().parents[1]
vtk_roots = [p for p in (CASE / "VTK").glob("windTunnelCar_*") if p.is_dir()]
if not vtk_roots:
    print("No VTK export — run foamToVTK -latestTime", file=sys.stderr)
    sys.exit(1)
VTK_DIR = sorted(vtk_roots)[-1]
OUT_DIR = CASE / "images" / "frames_flow"
OUT_DIR.mkdir(parents=True, exist_ok=True)
VIDEO = CASE / "images" / "windTunnelCar_airflow_30s_30fps.mp4"

N_FRAMES = 900
FPS = 30
W, H = 1280, 720
# How many times particles travel front→rear during the video
# Higher = faster perceived airflow (was 3.0 — felt slow)
N_CYCLES = 8.0
# Length of each comet as fraction of streamline (0..1)
COMET_LEN = 0.10
# Number of staggered comets per streamline
COMETS_PER_LINE = 4
N_RESAMPLE = 240


def load():
    internal = pv.read(VTK_DIR / "internal.vtu")
    vehicle = pv.read(VTK_DIR / "boundary" / "vehicle.vtp")
    ground = pv.read(VTK_DIR / "boundary" / "ground.vtp")
    if "U" in internal.cell_data and "U" not in internal.point_data:
        internal = internal.cell_data_to_point_data()
    if "p" in vehicle.cell_data and "p" not in vehicle.point_data:
        vehicle = vehicle.cell_data_to_point_data()
    internal.point_data["Umag"] = np.linalg.norm(internal.point_data["U"], axis=1)
    return internal, vehicle, ground


def extract_paths(streams: pv.PolyData) -> list[np.ndarray]:
    paths: list[np.ndarray] = []
    if streams is None or streams.n_points == 0:
        return paths
    for i in range(streams.n_cells):
        pts = np.asarray(streams.get_cell(i).points, dtype=float)
        if len(pts) >= 6:
            paths.append(pts)
    return paths


def resample_path(path: np.ndarray, n: int) -> np.ndarray:
    d = np.linalg.norm(np.diff(path, axis=0), axis=1)
    s = np.concatenate([[0.0], np.cumsum(d)])
    if s[-1] < 1e-9:
        return np.repeat(path[:1], n, axis=0)
    s /= s[-1]
    u = np.linspace(0.0, 1.0, n)
    out = np.empty((n, 3))
    for k in range(3):
        out[:, k] = np.interp(u, s, path[:, k])
    return out


def comet_polydata(path_stack: np.ndarray, head: float, length: float) -> pv.PolyData:
    """
    path_stack: (n_lines, n_pts, 3)
    head: fractional head position [0,1) for first comet family
    Build line cells for each streamline × COMETS_PER_LINE.
    """
    n_lines, n_pts, _ = path_stack.shape
    pts_list = []
    lines = []
    pid = 0
    for li in range(n_lines):
        for c in range(COMETS_PER_LINE):
            # stagger comets evenly along the path
            h = (head + c / COMETS_PER_LINE) % 1.0
            t0 = (h - length) % 1.0
            # unwrap window if it wraps around path end
            i_head = int(h * (n_pts - 1))
            i_tail = int(t0 * (n_pts - 1))
            if i_head >= i_tail:
                seg = path_stack[li, i_tail : i_head + 1]
            else:
                # wrap: end + start
                seg = np.vstack([path_stack[li, i_tail:], path_stack[li, : i_head + 1]])
            if len(seg) < 2:
                continue
            n = len(seg)
            pts_list.append(seg)
            # VTK line cell: [n, id0, id1, ...]
            ids = np.arange(pid, pid + n)
            lines.append(np.concatenate([[n], ids]))
            pid += n
    if not pts_list:
        return pv.PolyData()
    points = np.vstack(pts_list)
    # Build lines array for PolyData
    lines_arr = np.hstack(lines).astype(np.int64)
    poly = pv.PolyData()
    poly.points = points
    poly.lines = lines_arr
    return poly


def main() -> int:
    print(f"VTK: {VTK_DIR}")
    internal, vehicle, ground = load()

    sl = internal.slice(normal="z", origin=(6.0, 0.6, 2.0))
    seeds = pv.Plane(
        center=(2.5, 0.55, 2.0),
        direction=(1, 0, 0),
        i_size=2.4,
        j_size=1.5,
        i_resolution=14,
        j_resolution=10,
    )
    print("Streamlines...")
    streams = internal.streamlines_from_source(
        seeds,
        vectors="U",
        max_length=14.0,
        integration_direction="forward",
        initial_step_length=0.04,
    )
    raw = extract_paths(streams)
    paths = [resample_path(p, N_RESAMPLE) for p in raw if np.linalg.norm(p[-1] - p[0]) > 2.0]
    print(f"  paths kept: {len(paths)}")
    if not paths:
        return 1
    path_stack = np.stack(paths, axis=0)

    umin, umax = 0.0, float(np.percentile(internal.point_data["Umag"], 99))
    p = vehicle.point_data["p"]
    pmin, pmax = float(np.percentile(p, 2)), float(np.percentile(p, 98))

    # Fixed camera — side 3/4, air comes from the left (inlet)
    cam_pos = (11.0, 3.0, 8.5)
    cam_foc = (5.9, 0.55, 2.0)
    cam_up = (0.0, 1.0, 0.0)

    pl = pv.Plotter(off_screen=True, window_size=(W, H))
    pl.set_background("#0b1020")
    pl.add_mesh(ground, color="#1a1a22", opacity=0.6)
    pl.add_mesh(
        vehicle,
        scalars="p",
        cmap="coolwarm",
        clim=[pmin, pmax],
        smooth_shading=True,
        show_scalar_bar=False,
        specular=0.4,
    )
    pl.add_mesh(
        sl,
        scalars="Umag",
        cmap="turbo",
        clim=[umin, umax],
        opacity=0.5,
        show_scalar_bar=True,
        scalar_bar_args={
            "title": "|U| m/s",
            "color": "white",
            "title_font_size": 14,
            "label_font_size": 12,
            "n_labels": 5,
            "width": 0.45,
            "height": 0.08,
            "position_x": 0.28,
            "position_y": 0.02,
        },
    )
    # faint full streamlines
    if streams.n_points > 0:
        pl.add_mesh(streams, color="#3d5570", line_width=1.0, opacity=0.18)

    pl.add_text(
        "PFC windTunnelCar | CarConcept | fixed cam | air from inlet",
        font_size=11,
        color="white",
        position="upper_left",
    )

    pl.show(auto_close=False, interactive=False)
    cam = pl.renderer.GetActiveCamera()
    cam.SetPosition(*cam_pos)
    cam.SetFocalPoint(*cam_foc)
    cam.SetViewUp(*cam_up)
    cam.SetClippingRange(0.5, 120.0)
    pl.renderer.ResetCameraClippingRange()

    flow_actor = None
    print(f"Rendering {N_FRAMES} frames (comets on {len(paths)} streamlines)...")
    for i in range(N_FRAMES):
        head = (i / max(N_FRAMES - 1, 1)) * N_CYCLES
        head = head % 1.0
        comets = comet_polydata(path_stack, head, COMET_LEN)

        if flow_actor is not None:
            pl.remove_actor(flow_actor, reset_camera=False)
        if comets.n_points > 0:
            flow_actor = pl.add_mesh(
                comets,
                color="#7fd4ff",
                line_width=3.5,
                opacity=0.95,
                name="comets",
                reset_camera=False,
            )
        else:
            flow_actor = None

        pl.renderer.GetRenderWindow().Render()
        pl.render()
        img = pl.screenshot(return_img=True, window_size=(W, H))
        Image.fromarray(img).save(OUT_DIR / f"frame_{i:04d}.png")
        if i % 30 == 0:
            print(f"  frame {i}/{N_FRAMES} head={head:.3f} img_mean={float(img.mean()):.2f}")

    pl.close()

    cmd = [
        "ffmpeg", "-y",
        "-framerate", str(FPS),
        "-i", str(OUT_DIR / "frame_%04d.png"),
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        "-crf", "18",
        "-movflags", "+faststart",
        str(VIDEO),
    ]
    print(" ".join(cmd))
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-1500:], file=sys.stderr)
        return r.returncode
    print(f"Wrote {VIDEO}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
