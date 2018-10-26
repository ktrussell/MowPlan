# MowPlan
Generates a polygonal pattern of waypoints from outside in for autonomous mowing from a Mission Planner waypoint file containing a single polygon.

Command line example: ./mowplan.exe inwaypointfilename outwaypointfilename inch-spacing CW

Last parameter must be "CW" or "CCW" to indicate desired direction of travel.

Example command lines:

 	./mowplan.exe rectangle.waypoints out.waypoints 54 CCW --> will process rectangle.waypoints and generate out.waypoints with
    passes in a counterclockwise fashion that are 54 inches apart.

 	./mowplan.exe rectangle.waypoints out.waypoints 54 CW --> will process rectangle.waypoints and generate out.waypoints with
    passes in a clockwise fashion that are 54 inches apart.
    
See demonstration video at https://youtu.be/WplfVcRodPo.

Note: The program works well for fairly normal polygons. It may misbehave with oddly shaped polygons or very large polygons.

JUST RUNNING IT
---------------
The .exe should run as-is under Windows if the libgcc_s_dw2-1.dll and libwinpthread-1.dll are placed in the same directory or in the path.

To compile:
1. Copy mowplan.cpp, clipper.cpp and clipper.hpp into the same directory.
2. To compile with GCC, execute the cmow.bat file or use it as an example for another C++ compiler.

