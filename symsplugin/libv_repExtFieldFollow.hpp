
#pragma once

#ifdef _WIN32
    #define VREP_DLLEXPORT extern "C" __declspec(dllexport)
#else
    #define VREP_DLLEXPORT extern "C"
#endif


#include <iostream>
#include <fstream>
#include <cmath>
#include <cln/cln.h>
#include <ginac/ginac.h>
#include "v_repLib.h"
#include "tinyIntegrator.hpp"
#include "luaFunctionData.h"
#include "scriptFunctionData.h"
#include "stack/stackArray.h"
#include "stack/stackBool.h"
#include "stack/stackNull.h"
#include "stack/stackNumber.h"
#include "stack/stackString.h"


#ifdef _WIN32
    #ifdef QT_COMPIL
        #include <direct.h>
    #else
        #include <shlwapi.h>
        #pragma comment(lib, "Shlwapi.lib")
    #endif
#else
    #include <unistd.h>
#endif

#ifdef __APPLE__
#define _stricmp strcmp
#endif

#define PLUGIN_VERSION 4 // 2 since version 3.2.1, 3 since V3.3.1, 4 since V3.4.0

using GiNaC::matrix;


// State vector; saved in vrep conventions (angles and frames)
//		p,q,r is the angular velocity in global vrep frame
struct State {
	double x, y, z, vx, vy, vz, a, b, g, p, q, r;
};

// input vector: thrust + torques in v_vrep axis convention
struct Inputs {
	double fz, tx, ty, tz;
};


// custom commands
int initField(std::string fieldFilePath, std::string shapeName, bool vrepCaller);
		// read vector field file equations
void updateState(Inputs &inputs, double x, double y, double z, double yaw);
		// Eval symbolic equations
void simpleFeedback(Inputs &inputs, State &estState, const matrix &xyz,
		const matrix &abg, const matrix &v, const matrix &omega,
		const matrix &gains);


// The 3 required entry points of the V-REP plugin:
VREP_DLLEXPORT unsigned char v_repStart(void* reservedPointer,int reservedInt);
VREP_DLLEXPORT void v_repEnd();
VREP_DLLEXPORT void* v_repMessage(int message,int* auxiliaryData,void* customData,int* replyData);
