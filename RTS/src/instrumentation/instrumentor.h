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

// Set to 1 to create a file for use by chrome://tracing/
#define DUMP_FILE 0

constexpr f32 MICROSEC_TO_MILLISEC = 0.001f;

struct ProfileResult
{
    const char* Name;
    long long Start, End;
    uint32_t ThreadID;
    unsigned NestedDepth;
    const char* Parent;
};

struct InstrumentationSession
{
    std::string Name;
};

struct InstrumentTimeInfo {
    const char* parent;
    long long runningAverage;
    long long max;
    unsigned depth;
};

struct InstrumentorDebugStrings {
    nString name;
    nString avg;
    nString max;
};

struct InstrumentorDebugOutputData {
    std::map<std::thread::id, std::vector<InstrumentorDebugStrings>> data;
};

typedef FlatMap<std::thread::id, std::map<const char*, InstrumentTimeInfo>> DebugInstrumentationDataMap;

class Instrumentor
{
private:
    InstrumentationSession* m_CurrentSession;
#if DUMP_FILE == 1
    std::ofstream m_OutputStream;
#endif
    int m_ProfileCount;
    std::mutex mMutex;
    // TODO: Hashed string
    DebugInstrumentationDataMap mMostRecentTimes;

public:
    // Nested depth of this threads debug execution
    inline static thread_local std::vector<const char*> sTimerStack;

    Instrumentor()
        : m_CurrentSession(nullptr), m_ProfileCount(0)
    {
    }

    void getDebugOutputData(InstrumentorDebugOutputData& outData);

    void resetTimes();

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

    void writeProfile(const ProfileResult& result);

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
        Instrumentor::sTimerStack.emplace_back(name);
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

        uint32_t threadID = std::hash<std::thread::id>()(std::this_thread::get_id());
        Instrumentor::sTimerStack.pop_back();
        size_t stackSize = Instrumentor::sTimerStack.size();
        Instrumentor::get().writeProfile({ m_Name, start, end, threadID, (unsigned)stackSize, stackSize ? Instrumentor::sTimerStack.back() : nullptr});
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
#define PROFILE_SCOPE(name) InstrumentationTimer CONCAT(timer, __LINE__)(name)
//#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCSIG__)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
#else
#define PROFILE_BEGIN_SESSION(name, filepath)
#define PROFILE_END_SESSION(name)
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#endif