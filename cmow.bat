rem This compile line will include libgcc and libstdc++ in the .exe so that users do not have to have them.
g++ -static-libgcc -static-libstdc++ clipper.cpp mowplan.cpp -o mowplan

rem Old compile line
rem g++ clipper.cpp mowplan.cpp -o mowplan


