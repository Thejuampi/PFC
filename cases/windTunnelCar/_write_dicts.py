from pathlib import Path
dst = Path(r"G:/dev/repos/PFC/cases/windTunnelCar/system")
banner = """/*--------------------------------*- C++ -*----------------------------------*\\
| =========                 |                                                 |
| \\\\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |
|  \\\\    /   O peration     | Version:  v2512                                 |
|   \\\\  /    A nd           | Website:  www.openfoam.com                      |
|    \\\\/     M anipulation  |                                                 |
\\*---------------------------------------------------------------------------*/
FoamFile
{
    version     2.0;
    format      ascii;
    class       dictionary;
    object      %s;
}
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
"""
block = banner % "blockMeshDict" + """
scale   1;

vertices
(
    ( 0  0  0)
    (18  0  0)
    (18  4  0)
    ( 0  4  0)
    ( 0  0  4)
    (18  0  4)
    (18  4  4)
    ( 0  4  4)
);

blocks
(
    hex (0 1 2 3 4 5 6 7) (72 20 20) simpleGrading (1 1 1)
);

edges
(
);

boundary
(
    inlet
    {
        type patch;
        faces
        (
            (0 4 7 3)
        );
    }
    outlet
    {
        type patch;
        faces
        (
            (1 2 6 5)
        );
    }
    ground
    {
        type wall;
        faces
        (
            (0 1 5 4)
        );
    }
    top
    {
        type patch;
        faces
        (
            (3 7 6 2)
        );
    }
    side1
    {
        type patch;
        faces
        (
            (0 3 2 1)
        );
    }
    side2
    {
        type patch;
        faces
        (
            (4 5 6 7)
        );
    }
);

mergePatchPairs
(
);

// ************************************************************************* //
"""
snappy = banner % "snappyHexMeshDict" + """
castellatedMesh true;
snap            true;
addLayers       false;

geometry
{
    car.stl
    {
        type triSurfaceMesh;
        name vehicle;
    }
}

castellatedMeshControls
{
    maxLocalCells 3000000;
    maxGlobalCells 5000000;
    minRefinementCells 10;
    maxLoadUnbalance 0.10;
    nCellsBetweenLevels 2;

    features
    (
    );

    refinementSurfaces
    {
        vehicle
        {
            level (3 3);
            patchInfo
            {
                type wall;
            }
        }
    }

    resolveFeatureAngle 30;

    refinementRegions
    {
        vehicle
        {
            mode distance;
            levels ((0.5 3) (1.5 2) (3.0 1));
        }
    }

    locationInMesh (1.5 2.0 2.0);

    allowFreeStandingZoneFaces true;
}

snapControls
{
    nSmoothPatch 3;
    tolerance 2.0;
    nSolveIter 50;
    nRelaxIter 5;
    nFeatureSnapIter 10;
}

addLayersControls
{
    relativeSizes true;
    layers
    {
    }
    expansionRatio 1.0;
    finalLayerThickness 0.3;
    minThickness 0.1;
    nGrow 0;
    featureAngle 60;
    nRelaxIter 3;
    nSmoothSurfaceNormals 1;
    nSmoothNormals 3;
    nSmoothThickness 10;
    maxFaceThicknessRatio 0.5;
    maxThicknessToMedialRatio 0.3;
    minMedianAxisAngle 90;
    nBufferCellsNoExtrude 0;
    nLayerIter 50;
}

meshQualityControls
{
    #include "meshQualityDict"
}

writeFlags
(
);

mergeTolerance 1e-6;

// ************************************************************************* //
"""
(dst / "blockMeshDict").write_bytes(block.encode("ascii"))
(dst / "snappyHexMeshDict").write_bytes(snappy.encode("ascii"))
ctrl_path = dst / "controlDict"
ctrl = ctrl_path.read_text(encoding="utf-8")
ctrl = ctrl.replace("endTime         500;", "endTime         400;")
ctrl = ctrl.replace("CofR            (4 0.5 2);", "CofR            (5.5 0.4 2.0);")
ctrl = ctrl.replace("lRef            2;      // vehicle length (x)", "lRef            3.1;")
ctrl = ctrl.replace("Aref            1.6;    // frontal area ~ 1.0 * 1.6", "Aref            1.2;")
ctrl_path.write_bytes(ctrl.replace("\r\n", "\n").encode("ascii", "ignore"))
print("done")
