#include "stdafx.h"
#include "instrumentor.h"


void recursiveGetChildInfo(std::vector<const char*>& children, std::map<const char*, std::vector<const char*>>& childMap, std::vector<InstrumentorDebugOutput>& threadStrings, std::map<const char*, std::pair<InstrumentorDebugOutput, bool>>& functionStrings) {
    // TODO: Detect circular reference? Tail recursion?
    for (auto&& childId : children) {
        threadStrings.emplace_back(std::move(functionStrings[childId].first));
        auto&& it = childMap.find(childId);
        if (it != childMap.end()) {
            recursiveGetChildInfo(it->second, childMap, threadStrings, functionStrings);
        }
    }
}

void Instrumentor::getDebugOutputData(InstrumentorDebugOutputData& outData)
{
    // Copy to reduce critical section time to as small as possible
    DebugInstrumentationDataMap timeCopy;
    {
        std::shared_lock lock(mMutex);
        timeCopy = mMostRecentTimes;
    }
    // Sort via threads
    for (auto& imap : timeCopy) {
        std::vector<InstrumentorDebugOutput>& threadStrings = outData.data[imap.first];
        std::map<const char* /*function*/, std::vector<const char*> /*children*/> childMap;
        std::map<const char* /*function*/, std::pair<InstrumentorDebugOutput, bool/*isRoot*/>> functionStrings;
        // Sort by parent
        for (auto&& it : imap.second) {
            const InstrumentTimeInfo& timeInfo = it.second;
            const char* functionName = it.first;

            // Store string + isroot
            const bool isRoot = (timeInfo.parent == nullptr);
            functionStrings[functionName] = std::make_pair(InstrumentorDebugOutput{
                functionName,  timeInfo.runningAverage * MICROSEC_TO_MILLISEC, timeInfo.max * MICROSEC_TO_MILLISEC, timeInfo.depth }, isRoot
            );
            // Store parent/child mapping
            if (isRoot) {
                // Make sure we exist in the child map
                childMap[functionName];
            }
            else {
                // Mark us as child of parent
                childMap[timeInfo.parent].emplace_back(functionName);
            }
        }
        // Copy strings in sorted order
        for (auto&& it : childMap) {
            std::pair<InstrumentorDebugOutput, bool/*isRoot*/>& functionData = functionStrings[it.first];
            if (functionData.second) {
                // If we are root, store our strings and recurse chilren
                threadStrings.emplace_back(std::move(functionData.first));
                recursiveGetChildInfo(it.second, childMap, threadStrings, functionStrings);
            }
        }
    }
}

InstrumentorDebugStrings Instrumentor::getSingleResult(std::thread::id threadId, const char* name) const {
    InstrumentTimeInfo info;

    std::shared_lock lock(mMutex);
    const auto&& it = mMostRecentTimes.find(threadId);
    if (it != mMostRecentTimes.end()) {
        const auto& it2 = it->second.find(name);
        if (it2 != it->second.end()) {
            info = it2->second;
        }
        else {
            return InstrumentorDebugStrings();
        }
    }
    else {
        return InstrumentorDebugStrings();
    }
    return InstrumentorDebugStrings{
        name,
        "  avg: " + std::to_string(info.runningAverage * MICROSEC_TO_MILLISEC) + "ms",
        "  max: " + std::to_string(info.max * MICROSEC_TO_MILLISEC) + "ms"};
}

void Instrumentor::buildDebugStrings(InstrumentorDebugOutput inputData, InstrumentorDebugStrings& outStrings) {
    std::ostringstream outSS[3];

    for (int i = 0; i < 3; ++i)
        outSS[i].precision(2);

    const unsigned depthFill = inputData.depth * 2;
    outSS[0] << std::setfill(' ') << std::setw(depthFill) << "" << inputData.functionName;
    outSS[1] << std::setfill(' ') << std::setw(depthFill) << "" << "  avg: " << std::setw(5) << std::fixed << inputData.runningAvgerageMs << "ms";
    outSS[2] << std::setfill(' ') << std::setw(depthFill) << "" << "  max: " << std::setw(5) << std::fixed << inputData.maxMs << "ms";
    outStrings.name = std::move(outSS[0].str());
    outStrings.avg = std::move(outSS[1].str());
    outStrings.max = std::move(outSS[2].str());
}

void Instrumentor::resetTimes()
{
    std::lock_guard lock(mMutex);
    mMostRecentTimes.clear();
}

void Instrumentor::writeProfile(const ProfileResult& result)
{
    const long long dur = result.End - result.Start;

    // Rolling average
    // TODO: Only if profiler is open
    const std::thread::id threadId = std::this_thread::get_id();

    // TODO: Is it possible to have a mutex per thread instead?
    {
        std::lock_guard lock(mMutex);
        auto&& it = mMostRecentTimes.find(threadId);
        if (it == mMostRecentTimes.end()) {
            mMostRecentTimes[threadId].emplace(std::make_pair(result.Name, InstrumentTimeInfo{ result.Parent, dur, dur, result.NestedDepth }));
        }
        else {
            auto&& it2 = it->second.find(result.Name);
            if (it2 != it->second.end()) {
                it2->second.runningAverage = (long long)((it2->second.runningAverage + dur) * 0.5f);
                it2->second.max = std::max(it2->second.max, dur);
            }
            else {
                mMostRecentTimes[threadId].emplace(std::make_pair(result.Name, InstrumentTimeInfo{ result.Parent, dur, dur, result.NestedDepth }));
            }
        }
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
