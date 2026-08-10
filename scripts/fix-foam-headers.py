#!/usr/bin/env python3
"""Rewrite OpenFOAM case files with a valid FoamFile banner."""
import re
import sys
from pathlib import Path

HEADER = """/*--------------------------------*- C++ -*----------------------------------*\\
| =========                 |                                                 |
| \\\\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |
|  \\\\    /   O peration     | Version:  v2512                                 |
|   \\\\  /    A nd           | Website:  www.openfoam.com                      |
|    \\\\/     M anipulation  |                                                 |
\\*---------------------------------------------------------------------------*/
FoamFile
{{
    version     2.0;
    format      ascii;
    class       {cls};
    object      {obj};
}}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

"""

SPECS = [
    ("system/blockMeshDict", "blockMeshDict", "dictionary"),
    ("system/snappyHexMeshDict", "snappyHexMeshDict", "dictionary"),
    ("system/meshQualityDict", "meshQualityDict", "dictionary"),
    ("system/controlDict", "controlDict", "dictionary"),
    ("system/fvSchemes", "fvSchemes", "dictionary"),
    ("system/fvSolution", "fvSolution", "dictionary"),
    ("system/decomposeParDict", "decomposeParDict", "dictionary"),
    ("constant/transportProperties", "transportProperties", "dictionary"),
    ("constant/turbulenceProperties", "turbulenceProperties", "dictionary"),
    ("constant/g", "g", "uniformDimensionedVectorField"),
    ("0/U", "U", "volVectorField"),
    ("0/p", "p", "volScalarField"),
]


def strip_old(text: str) -> str:
    text = text.lstrip("\ufeff")
    text = re.sub(r"(?s)^\s*/\*.*?\*/\s*", "", text, count=1)
    text = re.sub(r"(?s)^\s*FoamFile\s*\{.*?\}\s*", "", text, count=1)
    # drop leading separator-only lines
    text = re.sub(r"(?m)^// \*+ //\s*\n", "", text, count=1)
    return text.lstrip("\n")


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "cases/windTunnel3D")
    for rel, obj, cls in SPECS:
        path = root / rel
        if not path.is_file():
            print("skip missing", path)
            continue
        body = strip_old(path.read_text(encoding="utf-8", errors="replace"))
        path.write_text(HEADER.format(cls=cls, obj=obj) + body, encoding="utf-8", newline="\n")
        print("fixed", path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
