#include "stdafx.h"
#include "instrumentor.h"

void Instrumentor::getDebugOutputData(InstrumentorDebugOutputData& outData)
{
    std::ostringstream outSS[3];

    for (int i = 0; i < 3; ++i)
        outSS[i].precision(1);

    DebugInstrumentationDataMap timeCopy;
    {
        std::lock_guard lock(mMutex);
        timeCopy = mMostRecentTimes;
    }
    // Sort via threads
    for (auto& imap : timeCopy) {
        std::vector<InstrumentorDebugStrings>& threadStrings = outData.data[imap.first];
        std::map<const char* /*function*/, std::pair<InstrumentorDebugStrings, std::vector<InstrumentorDebugStrings> /*childStrings*/>> functionStrings;
        // Sort by parent
        for (auto&& it : imap.second) {
            const InstrumentTimeInfo& timeInfo = it.second;
            const char* functionName = it.first;
            // Format
            const unsigned depthFill = timeInfo.depth * 2 + 5;
            outSS[0] << std::setfill(' ') << std::setw(depthFill) << "" << functionName;
            outSS[1] << std::setfill(' ') << std::setw(depthFill) << "avg: " << std::setw(5) << std::fixed << timeInfo.runningAverage * MICROSEC_TO_MILLISEC << "ms";
            outSS[2] << std::setfill(' ') << std::setw(depthFill) << "max: " << std::setw(5) << std::fixed << timeInfo.max * MICROSEC_TO_MILLISEC << "ms";

            // Store strings in order
            if (timeInfo.parent == nullptr) {
                functionStrings[functionName].first = InstrumentorDebugStrings{ outSS[0].str(), outSS[1].str(), outSS[2].str() };
            }
            else {
                functionStrings[functionName].second.emplace_back(InstrumentorDebugStrings{ outSS[0].str(), outSS[1].str(), outSS[2].str() });
            }
            outSS[0].str({});
            outSS[1].str({});
            outSS[2].str({});
        }
        // Copy strings in sorted order
        for (auto&& it : functionStrings) {
            threadStrings.reserve(1 + it.second.second.size());
            threadStrings.emplace_back(std::move(it.second.first));
            for (auto&& childStrings : it.second.second) {
                threadStrings.emplace_back(childStrings);
            }
        }
    }
}

void Instrumentor::resetTimes()
{
    std::lock_guard lock(mMutex);
    for (auto&& it : mMostRecentTimes) {
        for (auto&& it2 : it.second) {
            it2.second.runningAverage = 0.0f;
            it2.second.max = 0.0f;
        }
    }
}

void Instrumentor::writeProfile(const ProfileResult& result)
{
    const long long dur = result.End - result.Start;

    // Rolling average
    // TODO: Only if profiler is open
    const std::thread::id threadId = std::this_thread::get_id();

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
