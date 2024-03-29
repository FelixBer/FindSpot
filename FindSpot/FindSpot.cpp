#include "pin.H"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <utility>
#include <map>
#include <set>
#include <algorithm>

#include "helper.h"
#include "packetmanager.h"


//connection and logging data

//final dump of results
std::ofstream outFile;

//detailed debug log (performance hit), only if given by -d Knob
std::ofstream dbgLog;

//Command line switches for this tool.
KNOB<std::string> KnobOut(KNOB_MODE_WRITEONCE, "pintool", "o", "findspot.log", "write output to this file");
KNOB<std::string> KnobDbg(KNOB_MODE_WRITEONCE, "pintool", "d", "", "write detailed debugging log to this file [default off]");
KNOB<int> KnobPort(KNOB_MODE_WRITEONCE, "pintool", "p", to_string(FS_PORT), "port to listen on for controller");

//port to listen on for controller connection
int port = FS_PORT;

//communication with controller
FindSpotPacketManager manager;



bool execute_string_cmd(const std::string& cmd, std::string* result);

/*
* Thread the continously listens for commands from controller.
* This is a "internal" PIN thread spawned by PIN_SpawnInternalThread() upon controller connection.
*/
void control_thread(void* arg)
{
    //not ideal, since we race the main program start, but good enough for now
    PIN_StopApplicationThreads(PIN_ThreadId());

    int recv_failures = 0;
    while(1)
    {
        std::string cmd = manager.recv_cmd_block();
        if(cmd.empty())
        {
            if(recv_failures++ < 10)
                continue;
            else
                break;
        }

        recv_failures = 0;

        //handle some commands without freezing
        if(cmd == "freeze")
        {
            if(PIN_StopApplicationThreads(PIN_ThreadId()))
                manager.send_cmd("application frozen");
            else
                manager.send_cmd("freezing application failed");
            continue;
        }
        else if(cmd == "unfreeze")
        {
            PIN_ResumeApplicationThreads(PIN_ThreadId());
            manager.send_cmd("target resumed");
            continue;
        }
        else if(cmd == "kill")
        {
            manager.send_cmd("not implemented");
            break;
        }


        if(!PIN_StopApplicationThreads(PIN_ThreadId()))
        {
            auto error = "PIN_StopApplicationThreads() failed, dropping command\n";
            dbgLog << error;
            std::cout << error;
            manager.send_cmd(error);
            continue;
        }

        std::string result;
        bool executed = execute_string_cmd(cmd, &result);
        if(!executed)
        {
            std::cout << "command " << " returned error: " << result << std::endl;
            dbgLog << "command " << " returned error: " << result << std::endl;
            result = "unknown command";
        }
        dbgLog << "command " << " returned: " << result << std::endl;
        manager.send_cmd(result);

        PIN_ResumeApplicationThreads(PIN_ThreadId());
    }
}


int block_until_connect()
{
    manager.clientfd = manager.block_accept(port);
    int sent = manager.send_cmd("hello from findspot 1.0\ntarget frozen, type unfreeze to continue execution");

    PIN_THREAD_UID listener_uid = 0;
    THREADID thread_id = PIN_SpawnInternalThread(control_thread, NULL, 0, &listener_uid);
    if ( thread_id == INVALID_THREADID )
    {
        dbgLog << "PIN_SpawnInternalThread(listener) failed\n";
        exit(-1);
    }

    return sent;
}


//core functionality data

struct RtnInfo
{
    std::string name;
    std::string image;
    ADDRINT address = 0;
    UINT64 rtnCount = 0;
    size_t order = 0;
};

//global counter/order of hooked routines
size_t globalorder = 1;

//sort results chronological (true) or by hitcount (false)
bool sortbychrono = false;

//detach from the target as soon as possible
bool request_detach = false;

//map of all hooked routines
std::map<ADDRINT, RtnInfo> routines;

//module blacklist+whitelist
std::set<std::string> mod_white;
std::set<std::string> mod_black;

//returns false if the module should be ignored due to blacklist/whitelist
bool should_consider_module(const std::string &str)
{
    if(mod_white.size())
        return mod_white.count(str);
    if(mod_black.size())
        return !mod_black.count(str);
    return true; // both lists empty
}

//controls whether we add or remove to/from our dataset
enum class mode
{
    OFF,        //freeze current data
    COLLECT,    //add executing functions to data
    TRIM,       //remove executing functions from data
};
mode m = mode::OFF;
std::string modetostring(mode mm)
{
    if(mm == mode::OFF)
        return "OFF";
    if(mm == mode::COLLECT)
        return "COLLECT";
    if(mm == mode::TRIM)
        return "TRIM";
    return "???";
}


/*
Now some related functions.
*/

template<typename Stream>
void PrintData(Stream& ss, size_t n = INT32_MAX)
{
    //std::copy_if(routines.begin(), routines.end(), std::back_inserter(vec), [](const auto& x) { return x.second.rtnCount  != 0; });
    std::vector<RtnInfo> vec;
    for(const auto &x : routines)
        if(x.second.rtnCount)
            vec.push_back(x.second);

    std::sort(vec.begin(), vec.end(), [](const auto &a, const auto &b) { return a.order < b.order; });
    std::for_each(vec.begin(), vec.end(), [i=size_t(0)](auto& x) mutable { x.order = i++; });
    if(!sortbychrono)
        std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) { return a.rtnCount > b.rtnCount; });

    const int ww[]{NumDigits((int)vec.size()), 18, 10, 20, 0};
    print_aligned(ss, ww, "#", "Address", "Hits", "Module", "Symbol");

    size_t lim = 0;
    for(const auto& x : vec)
    {
        print_aligned(ss, ww, x.order, tohex(x.address), x.rtnCount, x.image, x.name);
        if(lim++ > n)
        {
            ss << "<...>\n";
            break;
        }
    }
    ss << "Total Count: " << std::dec << vec.size() << std::endl;
}

std::string PrintData(size_t n = INT32_MAX)
{
    std::stringstream ss;
    PrintData(ss, n);
    return ss.str();
}

void ClearData()
{
    routines.clear();
}

template<typename Stream>
void write_to_file(Stream& ss)
{
    PrintData(ss);
    ss << "------------------\n";
    ss << std::flush;
}

void Fini(INT32 code, void *v)
{
    write_to_file(outFile);
    outFile.close();
}

void ImgLoad(IMG img, void *v)
{
    if(IMG_Valid(img) && IMG_IsMainExecutable(img))
    {
        LOG("Loaded main Image: " + IMG_Name(APP_ImgHead()) + "\n");
        outFile << ("Loaded main Image: " + IMG_Name(APP_ImgHead()) + "\n");
    }
}

// This function is called before every hooked routine is executed
void docount(RtnInfo *rt)
{
    if(request_detach)
    {
        request_detach = false;
        PIN_Detach();
        return;
    }
    if(m == mode::OFF)
    {
        dbgLog << "ignored: " << tohex(rt->address) << " " << rt->image << " " << rt->name << std::endl;
        return;
    }
    if(m == mode::TRIM)
    {
        if(should_consider_module(rt->image))
        {
            rt->rtnCount = 0;
            dbgLog << "trimmed: " << tohex(rt->address) << " " << rt->image << " " << rt->name << std::endl;
        }
        return;
    }
    if(m == mode::COLLECT)
    {
        if(should_consider_module(rt->image))
        {
            rt->rtnCount++;
            dbgLog << "collect: " << tohex(rt->address) << " " << rt->image << " " << rt->name << std::endl;
        }
        return;
    }
}

// Pin calls this function every time a new rtn is executed
void Routine(RTN rtn, void *v)
{
    std::string filename = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
    ADDRINT adr = RTN_Address(rtn);

    if(should_consider_module(filename)) //todo: this means we cant dynamically change whitelist/blacklist even though we calaim to support it!
    {
        RtnInfo &rc = routines[adr];

        rc.name = RTN_Name(rtn);
        rc.image = filename;
        rc.address = adr;
        rc.rtnCount = 0;
        rc.order = globalorder++;

        dbgLog << "hook routine: " << tohex(rc.address) << " " << rc.image << " " << rc.name << std::endl;
        RTN_Open(rtn);

        // Insert a call at the entry point of a routine to increment the call count
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &rc, IARG_END);

        // For each instruction of the routine
        // for(INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
        //{
        //    // Insert a call to docount to increment the instruction counter for this rtn
        //    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_icount), IARG_END);
        //}

        RTN_Close(rtn);
    }
    else
    {
        dbgLog << "ignored module: " << tohex(adr) << " " << filename << std::endl;
    }
}



bool execute_string_cmd(const std::string& cmd, std::string* result)
{
    if(cmd == "help")
    {
        result->append("help          -- print this help.\n");
        result->append("kill          -- kill program (not available with -appdebug).\n");
        result->append("freeze        -- freeze the target program (all threads).\n");
        result->append("unfreeze      -- unfreeze the target program.\n");
        result->append("clear         -- clear all collected data.\n");
        result->append("show          -- show stats on collected data.\n");
        result->append("dump <file>   -- dump current data to file.\n");
        result->append("mode collect  -- collect all functions called from now on.\n");
        result->append("mode trim     -- remove all functions called from now on.\n");
        result->append("mode off      -- dont touch collected data.\n");
        result->append("mode          -- show current mode.\n");
        result->append("sort chrono   -- sort output in the order the functions were encountered.\n");
        result->append("sort hitcount -- sort output by number of times the functions were encountered.\n");
        result->append("mod           -- display white/blacklist.\n");
        result->append("mod blacklist <mod> -- add module to blacklist.\n");
        result->append("mod whitelist <mod> -- add module to whitelist.\n");
        result->append("mod blacklist remove <mod> -- remove module from blacklist.\n");
        result->append("mod whitelist remove <mod> -- remove module from whitelist.\n");
        return true;
    }
    else if(cmd == "detach")
    {
        //PIN_Detach();
        request_detach = true;
        *result = "detach request registered\n";
        return true;
    }
    else if(cmd == "show")
    {
        *result = PrintData(20);
        return true;
    }
    else if(cmd == "mode")
    {
        *result = "current mode: " + modetostring(m) + "\n";
        return true;
    }
    else if(cmd.find("dump") == 0)
    {
        std::string path = TrimWhitespace(cmd.substr(std::strlen("dump")));
        std::ofstream file;
        file.open(path.c_str());
        if(!file.is_open())
            std::cout << "Could not open file " << path << std::endl;
        else
            write_to_file(file);
        return true;
    }
    else if(cmd == "clear")
    {
        ClearData(); //todo broke because we pass pointers to contained data to instrumentation...
        *result = PrintData(20);
        return true;
    }
    else if(cmd == "mode collect")
    {
        m = mode::COLLECT;
        *result = "new mode: " + modetostring(m) + "\n";
        return true;
    }
    else if(cmd == "mode trim")
    {
        m = mode::TRIM;
        *result = "new mode: " + modetostring(m) + "\n";
        return true;
    }
    else if(cmd == "mode off")
    {
        m = mode::OFF;
        *result = "new mode: " + modetostring(m) + "\n";
        return true;
    }
    else if(cmd.find("mod blacklist remove") == 0)
    {
        std::string mod = TrimWhitespace(cmd.substr(std::strlen("mod blacklist remove")));
        mod_black.erase(mod);
        return true;
    }
    else if(cmd.find("mod whitelist remove") == 0)
    {
        std::string mod = TrimWhitespace(cmd.substr(std::strlen("mod whitelist remove")));
        mod_white.erase(mod);
        return true;
    }
    else if(cmd.find("mod blacklist") == 0)
    {
        std::string mod = TrimWhitespace(cmd.substr(std::strlen("mod blacklist")));
        mod_black.insert(mod);
        return true;
    }
    else if(cmd.find("mod whitelist") == 0)
    {
        std::string mod = TrimWhitespace(cmd.substr(std::strlen("mod whitelist")));
        mod_white.insert(mod);
        return true;
    }
    else if(cmd.find("mod") == 0)
    {
        std::stringstream ss;
        ss << "whitelist: ";
        for(const auto &m : mod_white)
            ss << m << ",";
        ss << std::endl;
        ss << "blacklist: ";
        for(const auto &m : mod_black)
            ss << m << ",";
        ss << std::endl;
        if(mod_black.empty() && mod_white.empty())
            ss << "All modules are being tracked." << std::endl;
        *result = ss.str();
        return true;
    }
    else if(cmd.find("sort c") == 0)
    {
        sortbychrono = true;
        return true;
    }
    else if(cmd.find("sort h") == 0)
    {
        sortbychrono = false;
        return true;
    }

    *result = "unknown command";
    return false;
}

/*
 * This call-back implements the extended debugger commands.
 *
 *  tid[in]         Pin thread ID for debugger's "focus" thread.
 *  ctxt[in,out]    Register state for the debugger's "focus" thread.
 *  cmd[in]         Text of the extended command.
 *  result[out]     Text that the debugger prints when the command finishes.
 *
 * Returns: true if we recognize this extended command.
 */
bool DebugInterpreter(THREADID tid, CONTEXT *ctxt, const std::string &cmd, std::string *result, void *)
{
    *result = "";
    std::string line = TrimWhitespace(cmd);

    dbgLog << "DebugInterpreter: " << line << std::endl;

    bool executed = execute_string_cmd(line, result);
    if(!executed)
        dbgLog << "Previous command was unknown!" << std::endl;
    return executed;
}

int Usage()
{
    std::cerr << "This tool helps you find the specific functions executed by some event" << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if(PIN_Init(argc, argv))
        return Usage();

    port = KnobPort.Value();
    outFile.open(KnobOut.Value().c_str());

    if(!KnobDbg.Value().empty())
        dbgLog.open(KnobDbg.Value().c_str());

    const std::string timestamp = datetimestring();
    LOG("time: " + timestamp + "\n");
    LOG("port: " + to_string(port) + "\n");
    LOG("writing output to " + KnobOut.Value() + "\n");
    dbgLog << "time: " << timestamp << std::endl;
    dbgLog << "port: " << port << std::endl;
    dbgLog << "" << PIN_Version() << std::endl;
   // dbgLog << "pin: " << PIN_VmFullPath() << std::endl;
    dbgLog << "tool: " << PIN_ToolFullPath() << std::endl;
    outFile << "time: " << timestamp << std::endl;

    PIN_AddDebugInterpreter(DebugInterpreter, 0);
    RTN_AddInstrumentFunction(Routine, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImgLoad, 0);

    block_until_connect();

    PIN_StartProgram();
    return 0;
}
