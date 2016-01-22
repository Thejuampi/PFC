# PFC
Repositorio para los informes de Proyecto Final de Carrera

Versión de OpenFOAM(R) utilizada: 2.4.0
Sistema Operativo: Ubuntu 14.04 LTS 64 bits

Para la instalación de OpenFOAM(R) se utilizó la herramienta apt-get segun las instrucciones detalladas en la página de OpenFOAM(R)

Instructivo:

Modificar
/opt/openfoam240/wmake/rules/linux64Gcc/c++
Agregar -std=c++11 a la bandera CC.
Quedaría de esta forma:
(...)
CC = g++ -m64 -std=c++11
(...)


