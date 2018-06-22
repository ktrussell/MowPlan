/*******************************************************************************
*                                                                              *
* Author    :  Kenny Trussell                                                  *
* Version   :  1.0.1                                                           *
* Date      :  22 June 2018                                                    *
* Youtube Ch:  https://www.youtube.com/channel/UCkIJQFyZdzLKh7AR-m3t2Bw        *
* Copyright :  Kenny Trussell 2018                                             *
*                                                                              *
* License:                                                                     *
* Use, modification & distribution is subject to MIT License                   *
* https://opensource.org/licenses/MIT                                          *
*                                                                              *
* Attributions:                                                                *
* This software uses the Clipper library by Angus Johnson                      *
* (http://www.angusj.com/delphi/clipper.php).                                  *
*                                                                              *
* Clipper - an open source freeware library for clipping and offsetting lines  *
* and polygons.                                                                *
*                                                                              *
*******************************************************************************/

#define NDEBUG	//Comment out this line to enable additional print statements for debugging purposes

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include "clipper.hpp"

#define MTRPERINCH (0.0254)

const int         MAXLINES     = 100;			//Maximum number of lines in an input waypoint file
const int         PARMSPERLINE = 12;			//Number of parameters in each line of waypoint file (except 1st line)
const long double SCALEFACTOR  = -100000000.L;	//Clipper only works on integers. We will scale lat and long up to integers.
												//Scaling all values to negative values causes resulting paths to be clockwise.
												// We change some to positive with the a "-1" factor below if the commmand line
												// specifies CCW.

using namespace ClipperLib;

//NOTE 1: In clipper.hpp, the following definitions are made. We depend on the IntPoint values being long long to be able to hold
// the precesion we need for latitude and longitude when we convert to integer values (long long).
//#ifdef use_int32
//  typedef int cInt;
//  static cInt const loRange = 0x7FFF;
//  static cInt const hiRange = 0x7FFF;
//#else
//  typedef signed long long cInt;
//  static cInt const loRange = 0x3FFFFFFF;
//  static cInt const hiRange = 0x3FFFFFFFFFFFFFFFLL;
//  typedef signed long long long64;     //used by Int128 class
//  typedef unsigned long long ulong64;
//struct IntPoint {
//  cInt X;
//  cInt Y;

void addPoint (int64_t xval, int64_t yval, Path *path) //See Note 1 above. We are depending on X and Y in IntPoint to be (long long)
{
    IntPoint ipt;
    ipt.X = xval;
    ipt.Y = yval;
    path->push_back(ipt);
}

//Command line example: mowplan.exe inwaypointfilename outwaypointfilename inch-spacing CW
// Last parameter should be "CW" or "CCW" to indicate desired diretion of travel.
// Example:
// 	mowplan.exe rectangle.waypoints out.waypoints 54 CCW --> will process rectangle.waypoints and generate out.waypoints with
//    passes in a counterclockwise fashion that are 54 inches apart.
//
// 	mowplan.exe rectangle.waypoints out.waypoints 54 CW -->	will process rectangle.waypoints and generate out.waypoints with
//    passes in a clockwise fashion that are 54 inches apart.

int main (int argc, char ** argv)
{
    Paths       subj;
	Paths       solution;
	std::string FirstLine, SecondLine, line;
	long double FirstCornerX;
	double      mowdir; //Set to 1 for CW, to -1 for CCW

	//Command line arguments:
	// argv[1]: input waypoint filename
	// argv[2]: output waypoint filename
	// argv[3]: amount to offset each new polygon in inches
	// argv[4]: "CW" or "CCW" to indicate direction of travel

	if (argc != 5)
	{
		std::cerr << "Usage:\n mowplan.exe InputWayPointFileName OutputWayPointFileName Spacing-Inches DIR\n"
					<< "  where DIR is CW or CCW" << std::endl;
		return 1;
	}

	std::string inputname   = argv[1];
	std::string outname     = argv[2];
	unsigned    InchSpacing = (unsigned int) strtoul(argv[3], NULL, 10);
	std::string DirStr      = argv[4];

	std::ifstream infile(inputname);
	if (!infile)
	{
		std::cerr << "Unable to open input file: " << inputname << std::endl;
		return 2;
	}

	std::ofstream outfile(outname);
	if (!outfile)
	{
		std::cerr << "Unable to open output file: " << outname << std::endl;
		return 3;
	}
	
	if (InchSpacing < 1)
	{
		std::cerr << "Spacing must be greater than 0." << std::endl;
		return 4;
	}
	
	if (strcmp(strupr(argv[4]), "CW") == 0)
	{
		mowdir = 1; //CW was specified on command line.
	}
	else if (strcmp(strupr(argv[4]), "CCW") == 0)
	{
		mowdir = -1; //CCW was specified on command line.
	}
	else
	{
		std::cerr << "DIR must be CW or CCW" << std::endl;
		return 5;
	}
	
	std::string WPparms[MAXLINES][PARMSPERLINE];
	
	//Read 1st and 2nd lines and keep for writing unchanged to output file.
	std::getline(infile, FirstLine);
	std::getline(infile, SecondLine);

	// Create the output file and write the 1st two lines from the input file.
	outfile << FirstLine << std::endl << SecondLine << std::endl;

	unsigned lno = 0;
	while (std::getline(infile, line))
	{
		outfile << line << std::endl;	//Write each line of the input file specifying our perimeter polygon to outfile.
		std::istringstream iss(line);
		for (int icnt = 0; icnt < PARMSPERLINE; icnt++) //Parse the line to get indiviual values.
			iss >> WPparms[lno][icnt];
		lno++;
	}

	#ifndef NDEBUG
		for (int jcnt = 0; jcnt < lno; jcnt++)
		{
			for (int icnt = 0; icnt < PARMSPERLINE; icnt++)
			{
				std::cout << WPparms[jcnt][icnt] << " ";
			}
			std::cout << std::endl;
		}
	#endif

	// To have clipper offset x and y directions by a reasonably accurate and proportional amount, we need to convert the latitude
	// and longitude to coordinates that have the same distance per degree. We will scale the longitude value by the ratio of
	// meters/deg of longitude at our current latitude to the meters/deg of latitude. The equations were taken from the section
	// titled "Length of a degree of latitude" from https://en.wikipedia.org/wiki/Latitude.
	// We don't need to be this accurate, but why not! We only calculate the scaling factor once.
	// We will use the latitude from our first waypoint in these calculations.

	double latrad        = (M_PI / 180.) * stod(WPparms[2][8]);	//latitude of 1st point in radians
	double LatMtrPerDeg  = M_PI * 6378137.0 * (1 - 0.00669437999014) / (180 * pow(sqrt(1 - 0.00669437999014 * 
						   pow(sin(latrad), 2)), 1.5));
	double LongMtrPerDeg = M_PI * 6378137.0 * cos(latrad) / (180 * sqrt(1 - 0.00669437999014 * pow(sin(latrad), 2)));

	// NOTE: It seems to me (RKT) that this scaling is inverted, but it works this way and not if inverted!
	// TODO: (figure out later)
	long double LongScaleFactor = LongMtrPerDeg/LatMtrPerDeg;
	int64_t   DegSpacing      = round((InchSpacing * MTRPERINCH / LatMtrPerDeg) * SCALEFACTOR);

	#ifndef NDEBUG
		std::cout << "LatMtrPerDeg = " << LatMtrPerDeg << std::endl;
		std::cout << "LongMtrPerDeg = " << LongMtrPerDeg << std::endl;
		std::cout << "LongScaleFactor = " << LongScaleFactor << std::endl;
		std::cout << "DegSpacing = " << DegSpacing << std::endl;
	#endif
	
	// WPparms elements 8 and 9 of the 3rd row forward are the latitude and longitude of the points.
	// Scale them by SCALEFACTOR. Also scale longitude so that it has same scale per degree as latitude. Convert to integer-type
	// variables. Add to the path for the perimeter polygon for clipper.
    Path p;
	for (int jcnt = 0; jcnt < lno; jcnt++)
	{
		//(int64_t) casting required to match parms of "addPoint". See Note 1.
		addPoint((int64_t) round(std::stod(WPparms[jcnt][8])*SCALEFACTOR), (int64_t) round(LongScaleFactor *
			std::stod(WPparms[jcnt][9]) * (mowdir * SCALEFACTOR)), &p);
		#ifndef NDEBUG
			printf( "Lat = %lf, Long = %lf\n", std::stod(WPparms[jcnt][8]), std::stod(WPparms[jcnt][9]));
			printf( "Lat = %lld, Long = %lld\n", (int64_t) round(std::stod(WPparms[jcnt][8]) * SCALEFACTOR), 
				(int64_t) round(std::stod(WPparms[jcnt][9]) * SCALEFACTOR));
		#endif
	}

	// The output from clipper will not quite be what we desire for the first corner waypoint. We desire the 1st corner of each
	// new polygon to be on the previous polygon's latitude. Clipper would put it on the latitude of the new polygon. We remember
	// the latitude of the previous 1st corner and replace the latitude calcucated by clipper with this value for the 1st corner
	// only.
	FirstCornerX = (int64_t) round(std::stod(WPparms[0][8]) * SCALEFACTOR);
	
    subj.push_back(p);
    ClipperOffset c;
    c.AddPaths(subj, jtMiter, etClosedPolygon);
    c.Execute(solution, DegSpacing);

	#ifndef NDEBUG
		printf("lno = %d\n", lno);
	#endif
	
	unsigned newlno = lno+1;	//Line number to put in outfile for lines generated
	
	while (solution.size() > 0)
	{
        Path pth3;

		#ifndef NDEBUG
			printf("solution size = %d\n", (int) solution.size());
		#endif
		
	    for (int icnt = 0; icnt < solution.size(); icnt++)
	    {
	        pth3 = solution.at(icnt);

			#ifndef NDEBUG
				printf("pth3.size = %d\n", pth3.size());
			#endif
			
	        for (int jcnt = 0; jcnt < pth3.size(); jcnt++)
	        {
	            IntPoint ipt = pth3.at(jcnt);
	            
				#ifndef NDEBUG
					printf("%d = %lld, %lld\n", jcnt, ipt.X, ipt.Y);
				#endif
				
				outfile << newlno++ << " ";
				for (int kcnt = 1; kcnt < 8; kcnt++)
				{
					outfile << WPparms[lno-1][kcnt] << " ";
				}
				outfile.precision(9); //We want lat and long to have 8 digits + decimal place.
				if (jcnt == 0)
				{
					//(long double) used to hold the "scaled back to decimal" values of the (int64_t) integer values.
					outfile << FirstCornerX / SCALEFACTOR << " " << (long double) (ipt.Y / LongScaleFactor) /
						(mowdir * SCALEFACTOR) << " ";
					FirstCornerX = (long double) ipt.X; // Remember X (latitude) of 1st corner for next polygon.
				}
				else
				{
					outfile << (long double) ipt.X / SCALEFACTOR << " " << (long double) (ipt.Y / LongScaleFactor) /
						(mowdir * SCALEFACTOR) << " ";
				}
				outfile << WPparms[lno-1][10] << " " << WPparms[lno-1][11] << std::endl;
			}
	    }

		//Clear the subject paths and fill with the points just calculated for next iteration.
		subj.clear();
	    subj.push_back(pth3);

		c.Clear();
	    c.AddPaths(subj, jtMiter, etClosedPolygon);
	    c.Execute(solution, DegSpacing);
	}

	outfile.close();
	std::cout << newlno << " waypoints in new file." << std::endl;

    return 0;
}
