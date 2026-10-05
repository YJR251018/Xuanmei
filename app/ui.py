from __future__ import annotations

import math
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from core.converter import CdrToDxfConverter, ConversionError
from core.dxf_analyzer import DxfError, analyze_dxf
from core.models import AnalysisResult, Entity


class GeometryWorkbench:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("CDR 几何转换与检测上位机")
        self.root.geometry("1080x720")
        self.root.minsize(820, 560)
        self.converter = CdrToDxfConverter()
        self.result: AnalysisResult | None = None
        self._build()

    def _build(self) -> None:
        style = ttk.Style(self.root)
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 18, "bold"))
        style.configure("Metric.TLabel", font=("Microsoft YaHei UI", 15, "bold"))

        header = ttk.Frame(self.root, padding=(18, 14))
        header.pack(fill="x")
        ttk.Label(header, text="CDR 几何转换与检测", style="Title.TLabel").pack(side="left")
        ttk.Label(header, text=self.converter.backend_status()).pack(side="right")

        toolbar = ttk.Frame(self.root, padding=(18, 0, 18, 12))
        toolbar.pack(fill="x")
        ttk.Button(toolbar, text="导入 CDR 并转换", command=self._choose_cdr).pack(side="left")
        ttk.Button(toolbar, text="打开 DXF", command=self._choose_dxf).pack(side="left", padx=8)
        ttk.Button(toolbar, text="适应窗口", command=self._draw).pack(side="left")

        body = ttk.Panedwindow(self.root, orient="horizontal")
        body.pack(fill="both", expand=True, padx=18)
        preview = ttk.Frame(body)
        side = ttk.Frame(body, width=280)
        body.add(preview, weight=4)
        body.add(side, weight=1)

        self.canvas = tk.Canvas(preview, bg="#101820", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda _event: self._draw())

        ttk.Label(side, text="几何分析", style="Title.TLabel").pack(anchor="w", padx=16, pady=(6, 18))
        self.file_var = tk.StringVar(value="尚未打开文件")
        self.length_var = tk.StringVar(value="总长度：--")
        self.count_var = tk.StringVar(value="实体：--")
        self.bounds_var = tk.StringVar(value="范围：--")
        ttk.Label(side, textvariable=self.file_var, wraplength=240).pack(anchor="w", padx=16, pady=5)
        ttk.Label(side, textvariable=self.length_var, style="Metric.TLabel").pack(anchor="w", padx=16, pady=12)
        ttk.Label(side, textvariable=self.count_var, wraplength=240).pack(anchor="w", padx=16, pady=5)
        ttk.Label(side, textvariable=self.bounds_var, wraplength=240).pack(anchor="w", padx=16, pady=5)

        log_frame = ttk.LabelFrame(self.root, text="运行日志", padding=8)
        log_frame.pack(fill="x", padx=18, pady=(12, 18))
        self.log = tk.Text(log_frame, height=6, state="disabled", font=("Consolas", 9))
        self.log.pack(fill="x")
        self._write_log("系统就绪。可打开 DXF，或导入 CDR 进行转换。")

    def _write_log(self, message: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", message + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _choose_cdr(self) -> None:
        source = filedialog.askopenfilename(filetypes=[("CorelDRAW", "*.cdr")])
        if not source:
            return
        target = filedialog.asksaveasfilename(defaultextension=".dxf", filetypes=[("DXF", "*.dxf")],
                                              initialfile=Path(source).stem + ".dxf")
        if not target:
            return
        self._write_log(f"开始转换：{source}")
        threading.Thread(target=self._convert_worker, args=(Path(source), Path(target)), daemon=True).start()

    def _convert_worker(self, source: Path, target: Path) -> None:
        try:
            output = self.converter.convert(source, target)
            self.root.after(0, lambda: self._after_conversion(output))
        except ConversionError as exc:
            self.root.after(0, lambda: self._show_error(str(exc)))

    def _after_conversion(self, output: Path) -> None:
        self._write_log(f"转换完成：{output}")
        self._load(output)

    def _choose_dxf(self) -> None:
        filename = filedialog.askopenfilename(filetypes=[("DXF", "*.dxf")])
        if filename:
            self._load(Path(filename))

    def _load(self, path: Path) -> None:
        try:
            self.result = analyze_dxf(path)
        except (OSError, DxfError) as exc:
            self._show_error(str(exc))
            return
        result = self.result
        self.file_var.set(result.source.name)
        self.length_var.set(f"总长度：{result.total_length:.3f}")
        count_text = "，".join(f"{name} × {count}" for name, count in sorted(result.counts.items()))
        self.count_var.set(f"实体：{count_text or '未发现支持的实体'}")
        if result.bounds:
            x0, y0, x1, y1 = result.bounds
            self.bounds_var.set(f"范围：{x1-x0:.3f} × {y1-y0:.3f}")
        self._write_log(f"分析完成：{len(result.entities)} 个实体，总长度 {result.total_length:.3f}")
        self._draw()

    def _show_error(self, message: str) -> None:
        self._write_log("错误：" + message)
        messagebox.showerror("操作失败", message)

    def _draw(self) -> None:
        self.canvas.delete("all")
        if not self.result or not self.result.bounds:
            self.canvas.create_text(self.canvas.winfo_width()/2, self.canvas.winfo_height()/2,
                                    text="请导入 CDR 或打开 DXF", fill="#8ea0ad",
                                    font=("Microsoft YaHei UI", 15))
            return
        x0, y0, x1, y1 = self.result.bounds
        width, height = max(self.canvas.winfo_width(), 2), max(self.canvas.winfo_height(), 2)
        margin = 35
        scale = min((width-2*margin)/max(x1-x0, 1e-9), (height-2*margin)/max(y1-y0, 1e-9))

        def point(x: float, y: float) -> tuple[float, float]:
            return margin+(x-x0)*scale, height-margin-(y-y0)*scale

        color = "#4dd0e1"
        for entity in self.result.entities:
            if entity.kind in {"LINE", "LWPOLYLINE"}:
                pts = entity.points + ([entity.points[0]] if entity.closed and entity.points else [])
                if len(pts) >= 2:
                    self.canvas.create_line(*(coord for p in pts for coord in point(*p)), fill=color, width=2)
            elif entity.kind == "CIRCLE" and entity.center:
                cx, cy = entity.center; r = entity.radius
                ax, ay = point(cx-r, cy+r); bx, by = point(cx+r, cy-r)
                self.canvas.create_oval(ax, ay, bx, by, outline=color, width=2)
            elif entity.kind == "ARC" and entity.center:
                cx, cy = entity.center; r = entity.radius
                ax, ay = point(cx-r, cy+r); bx, by = point(cx+r, cy-r)
                extent = (entity.end_angle-entity.start_angle) % 360
                self.canvas.create_arc(ax, ay, bx, by, start=entity.start_angle,
                                       extent=extent, style="arc", outline=color, width=2)

    def run(self) -> None:
        self.root.mainloop()
