// This file was created for V-REP release V3.4.0 

#include "libv_repExtSymDeriv.hpp"

#define CONCAT(x,y,z) x y z
#define strConCat(x,y,z)    CONCAT(x,y,z)

using namespace GiNaC;
using namespace std;

LIBRARY vrepLib; // the V-REP library that we will dynamically load and bind

// state vector
struct State {
	ex x, y, z, vx, vy, vz, psi, theta, phi, p, q, r;
};
// input vector: thrust + torques
struct Inputs {
	ex fz, tx, ty, tz;
};


// Plugin global variables
unsigned nVars = 0;					// the number of symbolic vars (up to 4)
symbol Sx("x"), Sy("y"), Sz("z"), Syaw("w");	// the lariables
matrix flatOut_D1;		// symbolic flat output derivatives vectors
matrix flatOut_D2;
matrix flatOut_D3;
matrix flatOut_D4;

// --------------------------------------------------------------------------------------
// simExtSymDeriv_print
// --------------------------------------------------------------------------------------
#define LUA_PRINT_COMMAND "simExtSymDeriv_print"

const int inArgs_PRINT[]={
    5,
	sim_script_arg_string,1,
    sim_script_arg_float,1,
    sim_script_arg_float,1,
    sim_script_arg_float,1,
    sim_script_arg_float,1
};
 

void LUA_PRINT_CALLBACK(SScriptCallBack* cb)
{ 
    CScriptFunctionData D;
    if (D.readDataFromStack(cb->stackID,inArgs_PRINT,inArgs_PRINT[0],LUA_PRINT_COMMAND))
    {
        std::vector<CScriptFunctionDataItem>* inData=D.getInDataPtr();
		string fileName = inData->at(0).stringData[0];
		float x = inData->at(1).floatData[1];
		float y = inData->at(2).floatData[2];
		float z = inData->at(3).floatData[3];
		float yaw = inData->at(4).floatData[4];

		// Get flat output derivatives
		//getFlatOutDerivatives(fileName, x, y, z, yaw);
    }
    D.pushOutData(CScriptFunctionDataItem(66));
    D.writeDataToStack(cb->stackID);
}
// --------------------------------------------------------------------------------------


// given the vector of the variables, the symbolic vector src in inputs
// computes the next derivative through dv = J_v * v
void genNextDerivative(const vector <symbol> vars, const matrix& src, matrix& dest) {

	unsigned nVars = vars.size();
	matrix jacob(nVars, nVars);
	for (unsigned r = 0; r < nVars; ++r) {
		for (unsigned c = 0; c < nVars; ++c) {
			jacob(r, c) = src[r].diff(vars.at(c));
		}
	}

	dest = jacob.mul(src);
}


void flatOutputs2state(State &state, const ex flatOut[], const ex flatOut1[],
		const ex flatOut2[]) {

	// Endogenous transformation: state in paper, eq.8
	// state: x, y, z, vx , vy , vz , psi, theta, phi, p, q, r

	state.x = flatOut[0];
	state.y = flatOut[1];
	state.z = flatOut[2];
	state.vx = flatOut1[0];
	state.vy = flatOut1[1];
	state.vz = flatOut1[2];

	ex ba = -cos(flatOut[3]) * flatOut2[0] - sin(flatOut[3]) * flatOut2[1];
	ex bb = -flatOut2[2] + 9.8;
	ex bc = -sin(flatOut[3]) * flatOut2[0] + cos(flatOut[3]) * flatOut2[1];

	state.phi = atan2(bc, sqrt(ba*ba + bb*bb));
	state.theta = atan2(ba, bb);
	state.psi = flatOut[3];

	// Euler rates rpy to angular velocity
	matrix T = {
		{ cos(state.phi) * cos(state.theta), -sin(state.phi), 0},
		{ cos(state.theta) * sin(state.phi), cos(state.phi), 0},
		{ -sin(state.theta), 0, 1}
	};

	ex d_phi = 1 / (1 + ba / bb);
	ex d_theta = 1 / (1 + bc / sqrt(ba*ba + bb*bb));
	ex d_psi = flatOut1[3];

	matrix angVel = T.mul({{d_phi}, {d_theta}, {d_psi}});
	state.p = angVel(0,0);
	state.q = angVel(1,0);
	state.r = angVel(2,0);

	cout << "flatOut, flatOut1:" << endl;
	cout << flatOut[0] << ", "<< flatOut[1] << ", "<< flatOut[2] << ", "<< flatOut[3]<< endl;
	cout << flatOut1[0] << ", "<< flatOut1[1] << ", "<< flatOut1[2] << ", "<< flatOut1[3]<< endl;

	cout << "state:" << endl;
	cout << state.x << ", "<< state.y << ", "<< state.z << endl;
	cout << state.vx << ", "<< state.vy << ", "<< state.vz << endl;
	cout << state.phi << ", "<< state.theta << ", "<< state.psi << endl;
	cout << state.p << ", "<< state.q << ", "<< state.r << endl;

}


void initField(string fieldFilePath) {

	string line;
	ifstream vectFile;
	vector<string> vectFieldStr;

	// File open
	try {
		vectFile.open(fieldFilePath, ifstream::in);
	} catch (ifstream::failure e) {
		cout << e.what();
		return;
	}

	// Prepare the GiNaC parser
	symtab table;
	vector <symbol> vars = {Sx, Sy, Sz, Syaw};
	table["x"] = Sx;
	table["y"] = Sy;
	table["z"] = Sz;
	table["w"] = Syaw;
	parser reader(table);

	// Get the first lines in the file as a vector
	nVars = 0;
	while (getline(vectFile, line)) {
		vectFieldStr.push_back(line);
		++nVars;
	}
	vectFile.close();
	vars.erase(vars.begin()+nVars, vars.end());

	// Fill a symbolic matrix
	matrix vectFieldSym(nVars, 1);
	for (unsigned i = 0; i < nVars; ++i) {
		ex e = reader(vectFieldStr[i]);
		vectFieldSym.set(i, 0, e);
	}

	// Save the first flat output derivative d(sigma)/dt=V(x)
	flatOut_D1 = vectFieldSym;

	// Compute next derivatives
	genNextDerivative(vars, flatOut_D1, flatOut_D2);
	genNextDerivative(vars, flatOut_D2, flatOut_D3);
	genNextDerivative(vars, flatOut_D3, flatOut_D4);
	
}

// TODO: check vrep registered function

// The registered vrep function for evaluating the inputs
void updateState(float x, float y, float z, float yaw) {

	// Evaluate the D4 vectors numerically
	exmap symMap;
	symMap[Sx] = x;
	symMap[Sy] = y;
	symMap[Sz] = z;
	symMap[Syaw] = yaw;

	// 	The flat output derivatives
	ex flatOut[4];
	ex flatOut1[4];
	ex flatOut2[4];
	ex flatOut3[4];
	ex flatOut4[4];

	// fill the globals flatOutputs derivatives
	for (unsigned i = 0; i < nVars; ++i) {
		flatOut1[i] = flatOut_D1(i,0).subs(symMap);
		flatOut2[i] = flatOut_D2(i,0).subs(symMap);
		flatOut3[i] = flatOut_D3(i,0).subs(symMap);
		flatOut4[i] = flatOut_D4(i,0).subs(symMap);
		//flatOut1[i] = ex_to<numeric>(flatOut_D1(i,0).subs(symMap)).to_double();
		//flatOut2[i] = ex_to<numeric>(flatOut_D2(i,0).subs(symMap)).to_double();
		//flatOut3[i] = ex_to<numeric>(flatOut_D3(i,0).subs(symMap)).to_double();
		//flatOut4[i] = ex_to<numeric>(flatOut_D4(i,0).subs(symMap)).to_double();
	}
	for (unsigned i = nVars; i < 4; ++i) {
		flatOut1[i] = 0;
		flatOut2[i] = 0;
		flatOut3[i] = 0;
		flatOut4[i] = 0;
	}
	flatOut[0] = x;
	flatOut[1] = y;
	flatOut[2] = z;
	flatOut[3] = yaw;

	// Get the state of the quadrotor
	State state;
	cout << flatOut1[3];
	flatOutputs2state(state, flatOut, flatOut1, flatOut2);
	

	// TODO: dividi il corpo in init (leggi file) e do it con sostituzioni

}



// This is the plugin start routine (called just once, just after the plugin was loaded):
VREP_DLLEXPORT unsigned char v_repStart(void* reservedPointer,int reservedInt)
{
    // Dynamically load and bind V-REP functions:
    // 1. Figure out this plugin's directory:
    char curDirAndFile[1024];
#ifdef _WIN32
    #ifdef QT_COMPIL
        _getcwd(curDirAndFile, sizeof(curDirAndFile));
    #else
        GetModuleFileName(NULL,curDirAndFile,1023);
        PathRemoveFileSpec(curDirAndFile);
    #endif
#else
    getcwd(curDirAndFile, sizeof(curDirAndFile));
#endif

    std::string currentDirAndPath(curDirAndFile);
    // 2. Append the V-REP library's name:
    std::string temp(currentDirAndPath);
#ifdef _WIN32
    temp+="\\v_rep.dll";
#elif defined (__linux)
    temp+="/libv_rep.so";
#elif defined (__APPLE__)
    temp+="/libv_rep.dylib";
#endif /* __linux || __APPLE__ */
    // 3. Load the V-REP library:
    vrepLib=loadVrepLibrary(temp.c_str());
    if (vrepLib==NULL)
    {
        std::cout << "Error, could not find or correctly load the V-REP library. Cannot start 'PluginSkeleton' plugin.\n";
        return(0); // Means error, V-REP will unload this plugin
    }
    if (getVrepProcAddresses(vrepLib)==0)
    {
        std::cout << "Error, could not find all required functions in the V-REP library. Cannot start 'PluginSkeleton' plugin.\n";
        unloadVrepLibrary(vrepLib);
        return(0); // Means error, V-REP will unload this plugin
    }

    // Check the version of V-REP:
    int vrepVer;
    simGetIntegerParameter(sim_intparam_program_version,&vrepVer);
    if (vrepVer<30200) // if V-REP version is smaller than 3.02.00
    {
        std::cout << "Sorry, your V-REP copy is somewhat old. Cannot start 'PluginSkeleton' plugin.\n";
        unloadVrepLibrary(vrepLib);
        return(0); // Means error, V-REP will unload this plugin
    }

    // Register the new Lua command "simExtSkeleton_getData":
    simRegisterScriptCallbackFunction(strConCat(LUA_PRINT_COMMAND,"@","SymDeriv"),strConCat("string result=",LUA_PRINT_COMMAND,"(number x)"),LUA_PRINT_CALLBACK);

    return(PLUGIN_VERSION); // initialization went fine, we return the version number of this plugin (can be queried with simGetModuleName)
}

// This is the plugin end routine (called just once, when V-REP is ending, i.e. releasing this plugin):
VREP_DLLEXPORT void v_repEnd()
{
    // Here you could handle various clean-up tasks

    unloadVrepLibrary(vrepLib); // release the library
}

// This is the plugin messaging routine (i.e. V-REP calls this function very often, with various messages):
VREP_DLLEXPORT void* v_repMessage(int message,int* auxiliaryData,void* customData,int* replyData)
{ // This is called quite often. Just watch out for messages/events you want to handle
    // Keep following 5 lines at the beginning and unchanged:
    static bool refreshDlgFlag=true;
    int errorModeSaved;
    simGetIntegerParameter(sim_intparam_error_report_mode,&errorModeSaved);
    simSetIntegerParameter(sim_intparam_error_report_mode,sim_api_errormessage_ignore);
    void* retVal=NULL;

    // Here we can intercept many messages from V-REP (actually callbacks). Only the most important messages are listed here.
    // For a complete list of messages that you can intercept/react with, search for "sim_message_eventcallback"-type constants
    // in the V-REP user manual.

    if (message==sim_message_eventcallback_refreshdialogs)
        refreshDlgFlag=true; // V-REP dialogs were refreshed. Maybe a good idea to refresh this plugin's dialog too

    if (message==sim_message_eventcallback_menuitemselected)
    { // A custom menu bar entry was selected..
        // here you could make a plugin's main dialog visible/invisible
    }

    if (message==sim_message_eventcallback_instancepass)
    {   // This message is sent each time the scene was rendered (well, shortly after) (very often)
        // It is important to always correctly react to events in V-REP. This message is the most convenient way to do so:

        int flags=auxiliaryData[0];
        bool sceneContentChanged=((flags&(1+2+4+8+16+32+64+256))!=0); // object erased, created, model or scene loaded, und/redo called, instance switched, or object scaled since last sim_message_eventcallback_instancepass message 
        bool instanceSwitched=((flags&64)!=0);

        if (instanceSwitched)
        {
            // React to an instance switch here!!
        }

        if (sceneContentChanged)
        { // we actualize plugin objects for changes in the scene

            //...

            refreshDlgFlag=true; // always a good idea to trigger a refresh of this plugin's dialog here
        }
    }

    if (message==sim_message_eventcallback_mainscriptabouttobecalled)
    { // The main script is about to be run (only called while a simulation is running (and not paused!))
        
    }

    if (message==sim_message_eventcallback_simulationabouttostart)
    { // Simulation is about to start

    }

    if (message==sim_message_eventcallback_simulationended)
    { // Simulation just ended

    }

    if (message==sim_message_eventcallback_moduleopen)
    { // A script called simOpenModule (by default the main script). Is only called during simulation.
        //if ( (customData==NULL)||(_stricmp("PluginSkeleton",(char*)customData)==0) ) // is the command also meant for this plugin?
        //{
        //    // we arrive here only at the beginning of a simulation
        //}
    }

    if (message==sim_message_eventcallback_modulehandle)
    { // A script called simHandleModule (by default the main script). Is only called during simulation.
        //if ( (customData==NULL)||(_stricmp("PluginSkeleton",(char*)customData)==0) ) // is the command also meant for this plugin?
        //{
        //    // we arrive here only while a simulation is running
        //}
    }

    if (message==sim_message_eventcallback_moduleclose)
    { // A script called simCloseModule (by default the main script). Is only called during simulation.
        //if ( (customData==NULL)||(_stricmp("PluginSkeleton",(char*)customData)==0) ) // is the command also meant for this plugin?
        //{
        //    // we arrive here only at the end of a simulation
        //}
    }

    if (message==sim_message_eventcallback_instanceswitch)
    { // We switched to a different scene. Such a switch can only happen while simulation is not running

    }

    if (message==sim_message_eventcallback_broadcast)
    { // Here we have a plugin that is broadcasting data (the broadcaster will also receive this data!)

    }

    if (message==sim_message_eventcallback_scenesave)
    { // The scene is about to be saved. If required do some processing here (e.g. add custom scene data to be serialized with the scene)

    }

    // You can add many more messages to handle here

    if ((message==sim_message_eventcallback_guipass)&&refreshDlgFlag)
    { // handle refresh of the plugin's dialogs
        // ...
        refreshDlgFlag=false;
    }

    // Keep following unchanged:
    simSetIntegerParameter(sim_intparam_error_report_mode,errorModeSaved); // restore previous settings
    return(retVal);
}



// TODO: testing
int main() {
	initField("/home/roberto/Desktop/Erob/V-REP/symsplugin/vector-field.txt");
	updateState(-1, -2, 0, 0);
}

