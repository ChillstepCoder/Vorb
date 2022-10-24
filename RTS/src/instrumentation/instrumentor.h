//
// Basic instrumentation profiler by Cherno

// Load the output into chrome://tracing/

// Usage: include this header file somewhere in your code (eg. precompiled header), and then use like:
//
// Instrumentor::Get().BeginSession("Session Name");        // Begin session 
// {
//     InstrumentationTimer timer("Profiled Scope Name");   // Place code like this in scopes you'd like to include in profiling
//     // Code
// }
// Instrumentor::Get().EndSession();                        // End Session
//
// You will probably want to macro-fy this, to switch on/off easily and use things like __FUNCSIG__ for the profile name.
//
#pragma once

#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <mutex>
#include <map>
#include <iomanip>

#include <thread>

// Set to 1 to create a file for use by chrome://tracing/
#define DUMP_FILE 0

constexpr f32 MICROSEC_TO_MILLISEC = 0.001f;

struct ProfileResult
{
    std::string Name;
    long long Start, End;
    uint32_t ThreadID;
};

struct InstrumentationSession
{
    std::string Name;
};

struct InstrumentTimeInfo {
    long long runningAverage;
    long long max;
};

class Instrumentor
{
private:
    InstrumentationSession* m_CurrentSession;
#if DUMP_FILE == 1
    std::ofstream m_OutputStream;
#endif
    int m_ProfileCount;
    std::mutex mMutex;
    std::map<std::string, InstrumentTimeInfo> mMostRecentTimes;
public:
    Instrumentor()
        : m_CurrentSession(nullptr), m_ProfileCount(0)
    {
    }

    std::string getMostRecentTimeString() {
        std::ostringstream out;
        out.precision(1);

        {
            std::lock_guard lock(mMutex);
            for (auto& it : mMostRecentTimes) {
                out << it.first << "\n avg: "
                    << std::setw(5) << std::fixed << it.second.runningAverage * MICROSEC_TO_MILLISEC << "ms\n" << " max: "
                    << std::setw(5) << std::fixed << it.second.max * MICROSEC_TO_MILLISEC << "ms\n";
            }
        }
        return out.str();
    }

    void beginSession(const std::string& name, const std::string& filepath = "results.json")
    {
#if DUMP_FILE == 1
        m_OutputStream.open(filepath);
        writeHeader();
#endif
        m_CurrentSession = new InstrumentationSession{ name };
    }

    void endSession()
    {
#if DUMP_FILE == 1
        writeFooter();
        m_OutputStream.close();
        m_ProfileCount = 0;
#endif
        delete m_CurrentSession;
        m_CurrentSession = nullptr;
    }

    void writeProfile(const ProfileResult& result)
    {
        const long long dur = result.End - result.Start;

        // Rolling average
        std::lock_guard lock(mMutex);
        auto&& it = mMostRecentTimes.find(result.Name);
        if (it == mMostRecentTimes.end()) {
            mMostRecentTimes.insert(std::make_pair(result.Name, InstrumentTimeInfo{ dur, dur }));
        }
        else {
            it->second.runningAverage = (long long)((it->second.runningAverage + dur) * 0.5f);
            it->second.max = std::max(it->second.max, dur);
        }
        
        // TODO: f you build up the json by sending sending everything to a local stringstream first,
        // and then send the stringstream to the output file stream in one go at the end of WriteProfile(),
        // then there is no need for a mutex.  
        // A single call to operator << will not get interrupted by another thread.
#if DUMP_FILE == 1
        if (m_ProfileCount++ > 0)
            m_OutputStream << ",";

        std::string name = result.Name;
        std::replace(name.begin(), name.end(), '"', '\'');

        m_OutputStream << "{";
        m_OutputStream << "\"cat\":\"function\",";
        m_OutputStream << "\"dur\":" << dur << ',';
        m_OutputStream << "\"name\":\"" << name << "\",";
        m_OutputStream << "\"ph\":\"X\",";
        m_OutputStream << "\"pid\":0,";
        m_OutputStream << "\"tid\":" << result.ThreadID << ",";
        m_OutputStream << "\"ts\":" << result.Start;
        m_OutputStream << "}";

        m_OutputStream.flush();
#endif
    }

#if DUMP_FILE == 1
    void writeHeader()
    {
        m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
        m_OutputStream.flush();
    }

    void writeFooter()
    {
        m_OutputStream << "]}";
        m_OutputStream.flush();
    }
#endif

    static Instrumentor& get()
    {
        static Instrumentor instance;
        return instance;
    }
};

class InstrumentationTimer
{
public:
    InstrumentationTimer(const char* name)
        : m_Name(name), m_Stopped(false)
    {
        m_StartTimepoint = std::chrono::high_resolution_clock::now();
    }

    ~InstrumentationTimer()
    {
        if (!m_Stopped)
            Stop();
    }

    void Stop()
    {
        auto endTimepoint = std::chrono::high_resolution_clock::now();

        long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
        long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

        uint32_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
        Instrumentor::get().writeProfile({ m_Name, start, end, threadID });

        m_Stopped = true;
    }
private:
    const char* m_Name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
    bool m_Stopped;
};

// TODO: IfDist
#if 1
#define PROFILE_BEGIN_SESSION(name, filepath) Instrumentor::get().beginSession(name)
#define PROFILE_END_SESSION(name) Instrumentor::get().endSession()

#define CONCAT(x, y) x ## y
#define C(x, y) CONCAT(x, y)
#define PROFILE_SCOPE(name) InstrumentationTimer C(timer, __LINE__)(name)
//#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#else
#define PROFILE_BEGIN_SESSION(name, filepath)
#define PROFILE_END_SESSION(name)
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif