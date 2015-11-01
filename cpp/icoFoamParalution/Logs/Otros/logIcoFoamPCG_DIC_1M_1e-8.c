/*---------------------------------------------------------------------------*\
| =========                 |                                                 |
| \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |
|  \\    /   O peration     | Version:  2.3.1                                 |
|   \\  /    A nd           | Web:      www.OpenFOAM.org                      |
|    \\/     M anipulation  |                                                 |
\*---------------------------------------------------------------------------*/
Build  : 2.3.1-bcfaaa7b8660
Exec   : icoFoam -parallel
Date   : Oct 07 2015
Time   : 16:28:14
Host   : "juan-ubuntu"
PID    : 8557
Case   : /home/juan/dev/paralution-1.0.0/src/examples/OpenFOAM/icoFoamParalution
nProcs : 4
Slaves : 
3
(
"juan-ubuntu.8558"
"juan-ubuntu.8559"
"juan-ubuntu.8560"
)

Pstream initialized with:
    floatTransfer      : 0
    nProcsSimpleSum    : 0
    commsType          : nonBlocking
    polling iterations : 0
sigFpe : Enabling floating point exception trapping (FOAM_SIGFPE).
fileModificationChecking : Monitoring run-time modified files using timeStampMaster
allowSystemOperations : Allowing user-supplied system call operations

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
Create time

Create mesh for time = 0

Reading transportProperties

Reading field p

Reading field U

Reading/calculating face flux field phi


Starting time loop

Time = 0.0001

Courant Number mean: 0 max: 0
smoothSolver:  Solving for Ux, Initial residual = 1, Final residual = 9.96529e-06, No Iterations 695
smoothSolver:  Solving for Uy, Initial residual = 0, Final residual = 0, No Iterations 0
DICPCG:  Solving for p, Initial residual = 1, Final residual = 9.5958e-09, No Iterations 2220
time step continuity errors : sum local = 1.93297e-13, global = 7.46095e-21, cumulative = 7.46095e-21
DICPCG:  Solving for p, Initial residual = 0.78738, Final residual = 9.78755e-09, No Iterations 2215
time step continuity errors : sum local = 2.47942e-13, global = 3.03905e-21, cumulative = 1.05e-20
ExecutionTime = 79.06 s  ClockTime = 79 s

Time = 0.0002

Courant Number mean: 0.0227346 max: 0.939568
smoothSolver:  Solving for Ux, Initial residual = 0.0637823, Final residual = 9.96524e-06, No Iterations 780
smoothSolver:  Solving for Uy, Initial residual = 0.547511, Final residual = 9.95828e-06, No Iterations 1011
DICPCG:  Solving for p, Initial residual = 0.970048, Final residual = 9.68334e-09, No Iterations 2214
time step continuity errors : sum local = 9.12934e-13, global = 8.33381e-21, cumulative = 1.88338e-20
DICPCG:  Solving for p, Initial residual = 0.921705, Final residual = 9.8364e-09, No Iterations 2212
time step continuity errors : sum local = 9.58951e-13, global = -7.05914e-22, cumulative = 1.81279e-20
ExecutionTime = 170.87 s  ClockTime = 170 s

Time = 0.0003

Courant Number mean: 0.035198 max: 1.28014
smoothSolver:  Solving for Ux, Initial residual = 0.0624339, Final residual = 9.9128e-06, No Iterations 726
smoothSolver:  Solving for Uy, Initial residual = 0.569028, Final residual = 9.91994e-06, No Iterations 907
DICPCG:  Solving for p, Initial residual = 0.990169, Final residual = 9.8913e-09, No Iterations 2179
time step continuity errors : sum local = 2.59243e-12, global = -7.49129e-21, cumulative = 1.06366e-20
DICPCG:  Solving for p, Initial residual = 0.963946, Final residual = 9.97199e-09, No Iterations 2177
time step continuity errors : sum local = 2.63822e-12, global = -3.78094e-21, cumulative = 6.85567e-21
ExecutionTime = 258.82 s  ClockTime = 258 s

Time = 0.0004

Courant Number mean: 0.0417749 max: 1.19394
smoothSolver:  Solving for Ux, Initial residual = 0.103913, Final residual = 9.94246e-06, No Iterations 744
smoothSolver:  Solving for Uy, Initial residual = 0.624536, Final residual = 9.91604e-06, No Iterations 911
DICPCG:  Solving for p, Initial residual = 0.99468, Final residual = 9.83768e-09, No Iterations 2168
time step continuity errors : sum local = 6.43628e-12, global = -1.53917e-21, cumulative = 5.3165e-21
DICPCG:  Solving for p, Initial residual = 0.963881, Final residual = 9.73953e-09, No Iterations 2167
time step continuity errors : sum local = 6.45824e-12, global = 1.47423e-21, cumulative = 6.79073e-21
ExecutionTime = 348.24 s  ClockTime = 347 s

Time = 0.0005

Courant Number mean: 0.0536838 max: 3.52858
smoothSolver:  Solving for Ux, Initial residual = 0.184693, Final residual = 9.96442e-06, No Iterations 761
smoothSolver:  Solving for Uy, Initial residual = 0.746188, Final residual = 9.93823e-06, No Iterations 882
DICPCG:  Solving for p, Initial residual = 0.996311, Final residual = 9.97431e-09, No Iterations 2165
time step continuity errors : sum local = 1.63095e-11, global = -4.81199e-21, cumulative = 1.97874e-21
DICPCG:  Solving for p, Initial residual = 0.967814, Final residual = 9.67763e-09, No Iterations 2165
time step continuity errors : sum local = 1.59966e-11, global = 1.65569e-20, cumulative = 1.85356e-20
ExecutionTime = 436.55 s  ClockTime = 436 s

End

Finalising parallel run
