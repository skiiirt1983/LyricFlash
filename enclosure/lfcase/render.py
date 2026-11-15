"""Off-screen PNG renders with VTK, plus matplotlib label overlays.

On a headless Linux box run scripts under `xvfb-run -a`; on Windows/macOS
VTK renders off-screen without anything extra.
"""
from __future__ import annotations

import numpy as np
import vtk
from vtk.util.numpy_support import vtk_to_numpy

import cadquery as cq

VIEWS = {
    # name: (camera direction from target, view-up)
    "front": ((0, 0, 1), (0, 1, 0)),
    "back": ((0, 0, -1), (0, 1, 0)),
    "back_drawing": ((0, 0, -1), (1, 0, 0)),   # same orientation as Waveshare's drawing (USB-C at the bottom)
    "front_drawing": ((0, 0, 1), (1, 0, 0)),
    "iso": ((-0.55, -0.75, 0.6), (0, 0, 1)),
    "iso_back": ((-0.6, -0.65, -0.55), (0, 0, -1)),
    "left": ((-1, 0, 0), (0, 0, 1)),
    "right": ((1, 0, 0), (0, 0, 1)),
    "bottom_edge": ((0, -1, 0), (0, 0, 1)),
}


def _actor(shape: cq.Shape, color, opacity=1.0, tol=0.02, edges=False, clip=None):
    poly = shape.toVtkPolyData(tol, 0.2, normals=True)
    mapper = vtk.vtkPolyDataMapper()
    mapper.SetInputData(poly)
    if clip is not None:
        pl = vtk.vtkPlane()
        pl.SetOrigin(*clip[0])
        pl.SetNormal(*clip[1])
        mapper.AddClippingPlane(pl)
    mapper.ScalarVisibilityOff()
    a = vtk.vtkActor()
    a.SetMapper(mapper)
    p = a.GetProperty()
    p.SetColor(*color)
    p.SetOpacity(opacity)
    p.SetSpecular(0.25)
    p.SetSpecularPower(30)
    p.SetAmbient(0.25)
    p.SetDiffuse(0.8)
    return a


class Scene:
    def __init__(self, size=(1600, 1200), bg=(1, 1, 1)):
        self.rw = vtk.vtkRenderWindow()
        self.rw.SetOffScreenRendering(1)
        self.rw.SetSize(*size)
        self.ren = vtk.vtkRenderer()
        self.ren.SetBackground(*bg)
        self.ren.SetUseDepthPeeling(1)
        self.rw.SetAlphaBitPlanes(1)
        self.rw.SetMultiSamples(8)
        self.rw.AddRenderer(self.ren)
        self.size = size

    def add(self, shape, color=(0.7, 0.7, 0.7), opacity=1.0, tol=0.02, clip=None):
        """clip = (origin, normal): keep the side the normal points to (cheap VTK clipping)."""
        self.ren.AddActor(_actor(shape, color, opacity, tol, clip=clip))

    def close(self):
        self.rw.Finalize()
        del self.ren, self.rw

    def camera(self, view="iso", target=None, zoom=1.0, parallel=True):
        d, up = VIEWS[view] if isinstance(view, str) else view
        self.ren.ResetCamera()
        cam = self.ren.GetActiveCamera()
        b = self.ren.ComputeVisiblePropBounds()
        c = target or ((b[0] + b[1]) / 2, (b[2] + b[3]) / 2, (b[4] + b[5]) / 2)
        r = max(b[1] - b[0], b[3] - b[2], b[5] - b[4])
        d = np.asarray(d, float)
        d /= np.linalg.norm(d)
        cam.SetFocalPoint(*c)
        cam.SetPosition(*(np.asarray(c) + d * r * 4))
        cam.SetViewUp(*up)
        cam.SetParallelProjection(parallel)
        self.ren.ResetCamera()
        cam.SetFocalPoint(*c)
        cam.SetPosition(*(np.asarray(c) + d * r * 4))
        if parallel:
            cam.SetParallelScale(r * 0.62 / zoom)
        self.ren.ResetCameraClippingRange()
        light = vtk.vtkLight()
        light.SetLightTypeToCameraLight()
        light.SetPosition(0.4, 0.6, 1)
        self.ren.AddLight(light)

    def image(self):
        self.rw.Render()
        w = vtk.vtkWindowToImageFilter()
        w.SetInput(self.rw)
        w.ReadFrontBufferOff()
        w.Update()
        img = w.GetOutput()
        nx, ny, _ = img.GetDimensions()
        arr = vtk_to_numpy(img.GetPointData().GetScalars()).reshape(ny, nx, -1)
        return np.flipud(arr[:, :, :3])

    def project(self, p):
        """World point -> pixel (x right, y down) in the image."""
        c = vtk.vtkCoordinate()
        c.SetCoordinateSystemToWorld()
        c.SetValue(*p)
        x, y = c.GetComputedDoubleDisplayValue(self.ren)
        return x, self.size[1] - y


def save_labelled(scene: Scene, path, labels=(), title=None, footer=None, dpi=100):
    """labels: [(world_xyz, text, (dx, dy) label offset in px, colour)]."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    img = scene.image()
    h, w = img.shape[:2]
    fig = plt.figure(figsize=(w / dpi, h / dpi), dpi=dpi)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.imshow(img)
    ax.set_axis_off()
    cx, cy = w / 2, h / 2
    for p, text, off, col in labels:
        x, y = scene.project(p)
        if not isinstance(off, tuple):  # radial auto-placement, off = distance in px (+ optional angle tweak)
            ang = np.arctan2(y - cy, x - cx) + np.radians(getattr(off, "imag", 0) or 0)
            L = abs(off.real) if isinstance(off, complex) else off
            dx, dy = L * np.cos(ang), L * np.sin(ang)
        else:
            dx, dy = off
        ax.annotate(
            text, xy=(x, y), xytext=(x + dx, y + dy), fontsize=17, color="black",
            ha="center", va="center", fontweight="bold",
            bbox=dict(boxstyle="round,pad=0.35", fc="white", ec=col, lw=2.5),
            arrowprops=dict(arrowstyle="-|>", color=col, lw=2.5, mutation_scale=18),
        )
        ax.plot([x], [y], "o", ms=7, mfc=col, mec="black")
    if title:
        ax.text(20, 30, title, fontsize=22, fontweight="bold", va="top",
                bbox=dict(fc="white", ec="none", alpha=0.85))
    if footer:
        ax.text(20, h - 20, footer, fontsize=14, va="bottom", color="#333")
    fig.savefig(path, dpi=dpi)
    plt.close(fig)
    scene.close()
