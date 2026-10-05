from pathlib import Path
from tempfile import TemporaryDirectory
import math
import unittest

from core.dxf_analyzer import analyze_dxf


DXF = """0
SECTION
2
ENTITIES
0
LINE
8
0
10
0
20
0
11
3
21
4
0
CIRCLE
10
10
20
10
40
2
0
ENDSEC
0
EOF
"""


class AnalyzerTests(unittest.TestCase):
    def test_line_and_circle_length(self) -> None:
        with TemporaryDirectory() as folder:
            path = Path(folder) / "sample.dxf"
            path.write_text(DXF, encoding="ascii")
            result = analyze_dxf(path)
        self.assertEqual(result.counts, {"LINE": 1, "CIRCLE": 1})
        self.assertAlmostEqual(result.total_length, 5 + 4 * math.pi)


if __name__ == "__main__":
    unittest.main()
