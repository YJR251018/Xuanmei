from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


class ConversionError(RuntimeError):
    pass


class CdrToDxfConverter:
    """Conversion facade. A native libcdr backend can implement this interface later."""

    COMMON_INKSCAPE_PATHS = (
        Path(r"C:\Program Files\Inkscape\bin\inkscape.exe"),
        Path(r"C:\Program Files\Inkscape\inkscape.exe"),
    )

    def _find_inkscape(self) -> Path | None:
        executable = shutil.which("inkscape")
        if executable:
            return Path(executable)
        return next((path for path in self.COMMON_INKSCAPE_PATHS if path.exists()), None)

    def backend_status(self) -> str:
        path = self._find_inkscape()
        return f"Inkscape: {path}" if path else "Inkscape: 未检测到"

    def convert(self, source: Path, target: Path) -> Path:
        if source.suffix.lower() != ".cdr":
            raise ConversionError("输入文件必须是 .cdr")
        if not source.is_file():
            raise ConversionError(f"找不到输入文件：{source}")

        executable = self._find_inkscape()
        if not executable:
            raise ConversionError(
                "未检测到 Inkscape。请安装 Inkscape，或在后续版本接入原生 libcdr 后端。"
            )

        target.parent.mkdir(parents=True, exist_ok=True)
        command = [
            str(executable),
            str(source),
            "--export-type=dxf",
            f"--export-filename={target}",
        ]
        completed = subprocess.run(
            command, capture_output=True, text=True, encoding="utf-8", errors="replace"
        )
        if completed.returncode != 0 or not target.exists():
            detail = (completed.stderr or completed.stdout or "未知错误").strip()
            raise ConversionError(f"转换失败：{detail}")
        return target

