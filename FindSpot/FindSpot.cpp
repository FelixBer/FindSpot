#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <map>
#include <set>
#include <algorithm>

#include <cctype>
#include <ctime>
#include <cassert>
#define assertm(exp, msg) assert(((void)msg, exp))

#include "pin.H"

std::ofstream outFile, dbgLog;

// Command line switches for this tool.
KNOB<std::string> KnobOut(KNOB_MODE_WRITEONCE, "pintool", "o", "", "write output to this file [default findspot.log]");
KNOB<std::string> KnobDbg(KNOB_MODE_WRITEONCE, "pintool", "d", "", "write detailed debugging log for this tool [default off]");

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

//map of all hooked routines
std::map<ADDRINT, RtnInfo> routines;

//module blacklist+whitelist
std::set<std::string> mod_white;
std::set<std::string> mod_black;

//returns false if the module should be ignored due to blacklist/whitelist
bool dologmodule(const std::string &str)
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
some helper functions
note: PIN more or less supports c++11
*/

/*
//
// no fold expressions in pin yet...
//

template<typename S, typename T>
void print_aligned(S& s, int width, T&& t)
{
    s << std::setw(width) << t;
}

template<typename S, size_t N, typename... Args>
void print_aligned(S& s, const int (&a)[N], Args&&... args)
{
    static_assert(N >= sizeof...(args), "array of widths must match number of parameters");
    int i = 0;
    (print_aligned(s, a[i++], std::forward<Args>(args)), ...);
    s << std::endl;
}

template<size_t N, typename... Args>
void print_aligned(const int (&a)[N], Args&&... args)
{
    print_aligned(std::cout, a, std::forward<Args>(args)...);
}
*/

template <typename S, size_t N, typename T>
void print_aligned_(S &s, const int (&a)[N], const size_t i, T &&t)
{
    assertm(N >= i, "static array out of bound");
    s << std::setw(a[i]) << t << " ";
}

template <typename S, size_t N, typename T, typename... Args>
void print_aligned_(S &s, const int (&a)[N], const size_t i, T &&t, Args &&...args)
{
    static_assert(N >= sizeof...(args), "array of widths must match number of parameters");
    print_aligned_(s, a, i, t);
    print_aligned_(s, a, i + 1, args...);
}

template <typename Stream, size_t N, typename... Args>
void print_aligned(Stream &s, const int (&a)[N], Args &&...args)
{
    print_aligned_(s, a, 0, args...);
    s << std::endl;
}

template <size_t N, typename... Args>
void print_aligned(const int (&a)[N], Args &&...args)
{
    print_aligned_(std::cout, a, args...);
    std::cout << std::endl;
}

template <typename T>
std::string tohex(T t)
{
    // std::format("{:x}", 123);
    std::stringstream sstream;
    sstream << std::hex << t;
    return sstream.str();
}

const char *StripPath(const char *path)
{
    const char *file = strrchr(path, '/');
    if(file)
        return file + 1;
    else
        return path;
}

constexpr int NumDigits(int x)
{  
    x = abs(x);  
    return (x < 10 ? 1 :   
        (x < 100 ? 2 :   
        (x < 1000 ? 3 :   
        (x < 10000 ? 4 :   
        (x < 100000 ? 5 :   
        (x < 1000000 ? 6 :   
        (x < 10000000 ? 7 :  
        (x < 100000000 ? 8 :  
        (x < 1000000000 ? 9 :  
        10)))))))));  
}

std::string datetimestring()
{
    std::time_t result = std::time(nullptr);
    return std::asctime(std::localtime(&result));
}

/*
 * Trim whitespace from a line of text.  Leading and trailing whitespace is removed.
 * Any internal whitespace is replaced with a single space (' ') character.
 *
 *  inLine[in]  Input text line.
 *
 * Returns: A string with the whitespace trimmed.
 */
std::string TrimWhitespace(const std::string &inLine)
{
    std::string outLine = inLine;

    bool skipNextSpace = true;
    for(std::string::iterator it = outLine.begin(); it != outLine.end(); ++it)
    {
        if(std::isspace(*it))
        {
            if(skipNextSpace)
            {
                it = outLine.erase(it);
                if(it == outLine.end())
                    break;
            }
            else
            {
                *it = ' ';
                skipNextSpace = true;
            }
        }
        else
        {
            skipNextSpace = false;
        }
    }
    if(!outLine.empty())
    {
        std::string::reverse_iterator it = outLine.rbegin();
        if(std::isspace(*it))
            outLine.erase(outLine.size() - 1);
    }
    return outLine;
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

    const int ww[]{NumDigits(vec.size()), 18, 10, 20, 0};
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

// This function is called before every instruction is executed
int dbgstatus = 123;
void docount(RtnInfo *rt)
{
    int s = PIN_GetDebugStatus();
    if(dbgstatus != s)
    {
        dbgLog << "dbg: " << dbgstatus << " -> " << s << std::endl;
        dbgstatus = s;
    }

    if(m == mode::OFF)
    {
        dbgLog << "ignored: " << tohex(rt->address) << " " << rt->image << " " << rt->name << std::endl;
        return;
    }
    if(m == mode::TRIM)
    {
        if(dologmodule(rt->image))
        {
            rt->rtnCount = 0;
            dbgLog << "trimmed: " << tohex(rt->address) << " " << rt->image << " " << rt->name << std::endl;
        }
        return;
    }
    if(m == mode::COLLECT)
    {
        if(dologmodule(rt->image))
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

    if(dologmodule(filename))
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

/*
 * This call-back implements the extended debugger commands.
 *
 *  tid[in]         Pin thread ID for debugger's "focus" thread.
 *  ctxt[in,out]    Register state for the debugger's "focus" thread.
 *  cmd[in]         Text of the extended command.
 *  result[out]     Text that the debugger prints when the command finishes.
 *
 * Returns: TRUE if we recognize this extended command.
 */
bool DebugInterpreter(THREADID tid, CONTEXT *ctxt, const std::string &cmd, std::string *result, void *)
{
    std::string line = TrimWhitespace(cmd);
    *result = "";

    dbgLog << "DebugInterpreter: " << line << std::endl;

    if(line == "help")
    {
        result->append("help          -- print this help.\n");
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
        return TRUE;
    }
    else if(line == "detach")
    {
        PIN_Detach();
        return TRUE;
    }
    else if(line == "show")
    {
        *result = PrintData(20);
        return TRUE;
    }
    else if(line == "mode")
    {
        *result = "current mode: " + modetostring(m) + "\n";
        return TRUE;
    }
    else if(line.find("dump") == 0)
    {
        std::string path = TrimWhitespace(line.substr(std::strlen("dump")));
        std::ofstream file;
        file.open(path.c_str());
        if(!file.is_open())
            std::cout << "Could not open file " << path << std::endl;
        else
            write_to_file(file);
        return TRUE;
    }
    else if(line == "clear")
    {
        ClearData();
        *result = PrintData(20);
        return TRUE;
    }
    else if(line == "mode collect")
    {
        m = mode::COLLECT;
        return TRUE;
    }
    else if(line == "mode trim")
    {
        m = mode::TRIM;
        return TRUE;
    }
    else if(line == "mode off")
    {
        m = mode::OFF;
        return TRUE;
    }
    else if(line.find("mod blacklist remove") == 0)
    {
        std::string mod = TrimWhitespace(line.substr(std::strlen("mod blacklist remove")));
        mod_black.erase(mod);
        return TRUE;
    }
    else if(line.find("mod whitelist remove") == 0)
    {
        std::string mod = TrimWhitespace(line.substr(std::strlen("mod whitelist remove")));
        mod_white.erase(mod);
        return TRUE;
    }
    else if(line.find("mod blacklist") == 0)
    {
        std::string mod = TrimWhitespace(line.substr(std::strlen("mod blacklist")));
        mod_black.insert(mod);
        return TRUE;
    }
    else if(line.find("mod whitelist") == 0)
    {
        std::string mod = TrimWhitespace(line.substr(std::strlen("mod whitelist")));
        mod_white.insert(mod);
        return TRUE;
    }
    else if(line.find("mod") == 0)
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
        return TRUE;
    }
    else if(line.find("sort c") == 0)
    {
        sortbychrono = true;
        return TRUE;
    }
    else if(line.find("sort h") == 0)
    {
        sortbychrono = false;
        return TRUE;
    }

    dbgLog << "Previous command was unknown!" << std::endl;
    return FALSE; // Unknown command
}

INT32 Usage()
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

    if(PIN_GetDebugStatus() == DEBUG_STATUS_DISABLED)
    {
        std::cerr << "Application level debugging must be enabled to use this tool.\n";
        std::cerr << "Start Pin with either -appdebug or -appdebug_enable.\n";
        std::cerr << std::flush;
        return 1;
    }

    if(!KnobOut.Value().empty())
        outFile.open(KnobOut.Value().c_str());
    else
        outFile.open("findspot.log");

    if(!KnobDbg.Value().empty())
        dbgLog.open(KnobDbg.Value().c_str());

    LOG("Time: " + datetimestring());
    outFile << ("Time: " + datetimestring());
    dbgLog << ("Time: " + datetimestring());
    LOG("Writing output to " + KnobOut.Value() + "\n");

    dbgLog << "dbg: DEBUG_STATUS_DISABLED = "  << DEBUG_STATUS_DISABLED << std::endl;
    dbgLog << "dbg: DEBUG_STATUS_UNCONNECTABLE = "  << DEBUG_STATUS_UNCONNECTABLE << std::endl;
    dbgLog << "dbg: DEBUG_STATUS_UNCONNECTED = "  << DEBUG_STATUS_UNCONNECTED << std::endl;
    dbgLog << "dbg: DEBUG_STATUS_CONNECTED = "  << DEBUG_STATUS_CONNECTED << std::endl;

    PIN_AddDebugInterpreter(DebugInterpreter, 0);
    RTN_AddInstrumentFunction(Routine, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImgLoad, 0);

    PIN_StartProgram();
    return 0;
}


// htb timelapse
