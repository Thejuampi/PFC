#!/usr/bin/env python3
"""Render showable views of windTunnel3D (latest foamToVTK export)."""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pyvista as pv

# Headless-friendly on Windows CI / agents
pv.OFF_SCREEN = True
pv.global_theme.font.color = "black"
pv.global_theme.background = "white"

CASE = Path(__file__).resolve().parents[1]
VTK_DIR = CASE / "VTK" / "windTunnel3D_222"
OUT = CASE / "images"
OUT.mkdir(exist_ok=True)


def load():
    internal = pv.read(VTK_DIR / "internal.vtu")
    vehicle = pv.read(VTK_DIR / "boundary" / "vehicle.vtp")
    # Point data preferred for smooth plots; cell data falls back
    if "U" not in internal.point_data and "U" in internal.cell_data:
        internal = internal.cell_data_to_point_data()
    if "p" not in vehicle.point_data and "p" in vehicle.cell_data:
        vehicle = vehicle.cell_data_to_point_data()
    if "U" in internal.point_data:
        u = internal.point_data["U"]
        internal.point_data["Umag"] = np.linalg.norm(u, axis=1)
    return internal, vehicle


def save(plotter: pv.Plotter, name: str):
    path = OUT / name
    plotter.screenshot(str(path), return_img=False)
    plotter.close()
    print(f"Wrote {path}")


def view_slice_umag(internal):
    # Mid-z slice through tunnel (domain z in [0,4], vehicle ~[1.2,2.8])
    sl = internal.slice(normal="z", origin=(6, 1.5, 2.0))
    pl = pv.Plotter(off_screen=True, window_size=(1600, 900))
    pl.set_background("white")
    pl.add_mesh(
        sl,
        scalars="Umag",
        cmap="turbo",
        show_scalar_bar=True,
        scalar_bar_args={"title": "|U| (m/s)", "color": "black"},
    )
    pl.add_text("windTunnel3D t=222 — mid-plane |U| (z=2 m)", font_size=12, color="black")
    pl.view_xy()
    pl.camera.zoom(1.15)
    save(pl, "slice_Umag_z2.png")


def view_vehicle_pressure(vehicle):
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1000))
    pl.set_background("white")
    pl.add_mesh(
        vehicle,
        scalars="p",
        cmap="coolwarm",
        show_scalar_bar=True,
        scalar_bar_args={"title": "p (m²/s²)", "color": "black"},
        smooth_shading=True,
    )
    pl.add_text("windTunnel3D t=222 — vehicle surface pressure", font_size=12, color="black")
    pl.camera_position = "iso"
    pl.camera.zoom(1.3)
    save(pl, "vehicle_pressure.png")


def view_vehicle_and_wake(internal, vehicle):
    # Iso surface of low speed wake + vehicle
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1000))
    pl.set_background("white")
    pl.add_mesh(vehicle, color="dimgray", opacity=1.0, smooth_shading=True)
    # Clip to show wake behind vehicle (x>5)
    clip = internal.clip(normal="-x", origin=(5.0, 1.5, 2.0), invert=False)
    # Contour low speed pockets if Umag exists
    if "Umag" in clip.point_data:
        try:
            iso = clip.contour(isosurfaces=[3.0], scalars="Umag")
            if iso.n_points > 0:
                pl.add_mesh(iso, color="deepskyblue", opacity=0.35, label="|U|=3 m/s")
        except Exception as e:
            print("iso skip:", e)
    # Slice behind vehicle
    wake = internal.slice(normal="z", origin=(6, 1.5, 2.0))
    pl.add_mesh(
        wake,
        scalars="Umag",
        cmap="turbo",
        opacity=0.85,
        show_scalar_bar=True,
        scalar_bar_args={"title": "|U| (m/s)", "color": "black"},
    )
    pl.add_text("windTunnel3D t=222 — vehicle + mid-plane wake", font_size=12, color="black")
    pl.camera_position = [
        (14, 8, 10),
        (5, 0.8, 2),
        (0, 1, 0),
    ]
    save(pl, "vehicle_wake_slice.png")


def view_pressure_slice(internal):
    sl = internal.slice(normal="z", origin=(6, 1.5, 2.0))
    pl = pv.Plotter(off_screen=True, window_size=(1600, 900))
    pl.set_background("white")
    pl.add_mesh(
        sl,
        scalars="p",
        cmap="coolwarm",
        show_scalar_bar=True,
        scalar_bar_args={"title": "p (m²/s²)", "color": "black"},
    )
    pl.add_text("windTunnel3D t=222 — mid-plane pressure", font_size=12, color="black")
    pl.view_xy()
    pl.camera.zoom(1.15)
    save(pl, "slice_p_z2.png")


def main() -> int:
    if not (VTK_DIR / "internal.vtu").is_file():
        print(f"Missing {VTK_DIR}/internal.vtu — run foamToVTK -latestTime first", file=sys.stderr)
        return 1
    internal, vehicle = load()
    print(internal)
    print("point arrays:", list(internal.point_data.keys()))
    view_slice_umag(internal)
    view_pressure_slice(internal)
    view_vehicle_pressure(vehicle)
    view_vehicle_and_wake(internal, vehicle)
    print(f"All images in {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
