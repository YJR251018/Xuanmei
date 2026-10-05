from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence


Point = tuple[float, float]


@dataclass(slots=True)
class Entity:
    kind: str
    points: list[Point] = field(default_factory=list)
    center: Point | None = None
    radius: float = 0.0
    start_angle: float = 0.0
    end_angle: float = 0.0
    closed: bool = False
    layer: str = "0"


@dataclass(slots=True)
class AnalysisResult:
    source: Path
    entities: Sequence[Entity]
    total_length: float
    counts: dict[str, int]
    bounds: tuple[float, float, float, float] | None
    unit: str = "drawing units"

