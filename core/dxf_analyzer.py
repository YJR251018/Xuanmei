from __future__ import annotations

import math
from collections import Counter
from pathlib import Path

from .models import AnalysisResult, Entity, Point


class DxfError(RuntimeError):
    pass


def _pairs(text: str) -> list[tuple[int, str]]:
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if len(lines) % 2:
        raise DxfError("DXF 组码和值没有成对出现，文件可能损坏")
    result: list[tuple[int, str]] = []
    for index in range(0, len(lines), 2):
        try:
            code = int(lines[index].strip())
        except ValueError as exc:
            raise DxfError(f"第 {index + 1} 行不是有效 DXF 组码") from exc
        result.append((code, lines[index + 1].strip()))
    return result


def _float(fields: list[tuple[int, str]], code: int, default: float = 0.0) -> float:
    for field_code, value in fields:
        if field_code == code:
            try:
                return float(value)
            except ValueError:
                return default
    return default


def _text(fields: list[tuple[int, str]], code: int, default: str = "") -> str:
    return next((value for field_code, value in fields if field_code == code), default)


def _entity(kind: str, fields: list[tuple[int, str]]) -> Entity | None:
    layer = _text(fields, 8, "0")
    if kind == "LINE":
        return Entity(kind, [(_float(fields, 10), _float(fields, 20)),
                             (_float(fields, 11), _float(fields, 21))], layer=layer)
    if kind in {"CIRCLE", "ARC"}:
        return Entity(kind, center=(_float(fields, 10), _float(fields, 20)),
                      radius=abs(_float(fields, 40)), start_angle=_float(fields, 50),
                      end_angle=_float(fields, 51), layer=layer)
    if kind == "LWPOLYLINE":
        points: list[Point] = []
        current_x: float | None = None
        for code, value in fields:
            if code == 10:
                current_x = float(value)
            elif code == 20 and current_x is not None:
                points.append((current_x, float(value)))
                current_x = None
        flags = int(_float(fields, 70))
        return Entity(kind, points, closed=bool(flags & 1), layer=layer)
    return None


def _length(entity: Entity) -> float:
    if entity.kind == "LINE":
        return math.dist(entity.points[0], entity.points[1])
    if entity.kind == "LWPOLYLINE":
        segments = list(zip(entity.points, entity.points[1:]))
        if entity.closed and len(entity.points) > 1:
            segments.append((entity.points[-1], entity.points[0]))
        return sum(math.dist(a, b) for a, b in segments)
    if entity.kind == "CIRCLE":
        return 2 * math.pi * entity.radius
    if entity.kind == "ARC":
        sweep = (entity.end_angle - entity.start_angle) % 360
        return math.radians(sweep) * entity.radius
    return 0.0


def _bounds(entities: list[Entity]) -> tuple[float, float, float, float] | None:
    points: list[Point] = []
    for entity in entities:
        points.extend(entity.points)
        if entity.center:
            cx, cy = entity.center
            r = entity.radius
            points.extend(((cx - r, cy - r), (cx + r, cy + r)))
    if not points:
        return None
    xs, ys = zip(*points)
    return min(xs), min(ys), max(xs), max(ys)


def analyze_dxf(path: Path) -> AnalysisResult:
    raw = path.read_bytes()
    if raw.startswith(b"AutoCAD Binary DXF"):
        raise DxfError("当前原型仅支持 ASCII DXF，不支持二进制 DXF")
    text = raw.decode("utf-8", errors="replace")
    pairs = _pairs(text)
    entities: list[Entity] = []
    in_entities = False
    index = 0
    while index < len(pairs):
        code, value = pairs[index]
        if code == 0 and value == "SECTION" and index + 1 < len(pairs):
            in_entities = pairs[index + 1] == (2, "ENTITIES")
            index += 2
            continue
        if code == 0 and value == "ENDSEC":
            in_entities = False
        if in_entities and code == 0 and value in {"LINE", "LWPOLYLINE", "CIRCLE", "ARC"}:
            kind = value
            index += 1
            fields: list[tuple[int, str]] = []
            while index < len(pairs) and pairs[index][0] != 0:
                fields.append(pairs[index])
                index += 1
            parsed = _entity(kind, fields)
            if parsed:
                entities.append(parsed)
            continue
        index += 1
    counts = dict(Counter(entity.kind for entity in entities))
    return AnalysisResult(path, entities, sum(map(_length, entities)), counts, _bounds(entities))

