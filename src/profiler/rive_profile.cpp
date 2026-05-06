#include "rive/profiler/rive_profile.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_host.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/nested_artboard.hpp"
#include <algorithm>

#ifdef RIVE_MICROPROFILE
#include "rive/profiler/microprofile_emscripten.h"
#include "microprofile.h"

namespace
{
void writeInt64(rive::VectorBinaryWriter& writer, int64_t value)
{
    writer.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}
void writeUint64(rive::VectorBinaryWriter& writer, uint64_t value)
{
    writer.write(reinterpret_cast<const uint8_t*>(&value), sizeof(value));
}

void writeProfileHeader(rive::VectorBinaryWriter& writer)
{
    MicroProfile* pState = MicroProfileGet();
    writer.write((uint32_t)0x52505246); // "RPRF" magic
    writer.write((uint32_t)2);          // version 2
    writeInt64(writer, MicroProfileTicksPerSecondCpu());
    writer.writeVarUint(pState->nTotalTimers);
    for (uint32_t i = 0; i < pState->nTotalTimers; i++)
    {
        const auto& info = pState->TimerInfo[i];
        writer.write(std::string(info.pName));
        writer.write((uint32_t)info.nGroupIndex);
        writer.write((uint32_t)info.nColor);
    }
    writer.writeVarUint(pState->nGroupCount);
    for (uint32_t i = 0; i < pState->nGroupCount; i++)
    {
        writer.write(std::string(pState->GroupInfo[i].pName));
    }
}

void writeFrameEvents(rive::VectorBinaryWriter& writer,
                      MicroProfile* pState,
                      uint32_t frameIndex,
                      uint32_t nextFrameIndex)
{
    const MicroProfileFrameState& frame = pState->Frames[frameIndex];
    const MicroProfileFrameState& nextFrame = pState->Frames[nextFrameIndex];

    std::vector<uint8_t> payload;
    rive::VectorBinaryWriter pw(&payload);
    writeInt64(pw, frame.nFrameStartCpu);
    writeInt64(pw, nextFrame.nFrameStartCpu);
    uint32_t totalEvents = 0;
    for (uint32_t threadIdx = 0; threadIdx < MICROPROFILE_MAX_THREADS;
         threadIdx++)
    {
        MicroProfileThreadLog* pLog = pState->Pool[threadIdx];
        if (!pLog)
            continue;
        uint32_t logStart = frame.nLogStart[threadIdx];
        uint32_t logEnd = nextFrame.nLogStart[threadIdx];
        if (logEnd >= logStart)
            totalEvents += (logEnd - logStart);
        else
            totalEvents += (MICROPROFILE_BUFFER_SIZE - logStart) + logEnd;
    }
    pw.writeVarUint(totalEvents);
    for (uint32_t threadIdx = 0; threadIdx < MICROPROFILE_MAX_THREADS;
         threadIdx++)
    {
        MicroProfileThreadLog* pLog = pState->Pool[threadIdx];
        if (!pLog)
            continue;
        uint32_t logStart = frame.nLogStart[threadIdx];
        uint32_t logEnd = nextFrame.nLogStart[threadIdx];
        for (uint32_t k = logStart; k != logEnd;
             k = (k + 1) % MICROPROFILE_BUFFER_SIZE)
        {
            MicroProfileLogEntry entry = pLog->Log[k];
            uint64_t nType = MicroProfileLogType(entry);
            uint64_t nTimerIndex = MicroProfileLogTimerIndex(entry);
            int64_t nTick =
                MicroProfileLogTickDifference(frame.nFrameStartCpu, entry);
            uint8_t eventType = (nType == MP_LOG_ENTER)   ? 0
                                : (nType == MP_LOG_LEAVE) ? 1
                                                          : 2;
            pw.write(eventType);
            pw.writeVarUint((uint32_t)nTimerIndex);
            writeInt64(pw, nTick);
        }
    }

    writer.write((uint8_t)0x01);
    writer.writeVarUint(static_cast<uint32_t>(payload.size()));
    writer.write(payload.data(), payload.size());
}
} // namespace
#endif

namespace rive
{

RiveProfile& RiveProfile::instance()
{
    static RiveProfile s_instance;
    return s_instance;
}

void RiveProfile::init()
{
#ifdef RIVE_MICROPROFILE
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceEnable(true);
    MicroProfileOnThreadCreate("MainThread");
    MicroProfileInit();
#endif
}

void RiveProfile::frame()
{
#ifdef RIVE_MICROPROFILE
    // Frame is typically a no-op at start of loop; MicroProfile tracks
    // internally
#endif
}

void RiveProfile::endFrame()
{
#ifdef RIVE_MICROPROFILE
    if (m_profilingActive)
    {
        MicroProfileFlip();
    }
#endif
}

void RiveProfile::start()
{
#ifdef RIVE_MICROPROFILE
    m_profilingActive = true;
    m_headerWritten = false;
    m_lastFlushedFrameIndex = 0;
    m_stringTable.clear();
    m_stringList.clear();
    MicroProfileSetEnableAllGroups(true);
    MicroProfileSetForceEnable(true);
    MicroProfile* pState = MicroProfileGet();
    if (pState != nullptr)
    {
        m_lastFlushedFrameIndex = pState->nFramePutIndex;
    }
#endif
}

void RiveProfile::stop()
{
#ifdef RIVE_MICROPROFILE
    m_profilingActive = false;
    MicroProfileSetForceEnable(false);
#endif
}

bool RiveProfile::isActive() const
{
#ifdef RIVE_MICROPROFILE
    return m_profilingActive;
#else
    return false;
#endif
}

void RiveProfile::flushFrameDataTo(std::vector<uint8_t>& buffer)
{
#ifdef RIVE_MICROPROFILE
    MicroProfile* pState = MicroProfileGet();
    if (pState == nullptr)
        return;
    VectorBinaryWriter writer(&buffer);
    if (!m_headerWritten)
    {
        writeProfileHeader(writer);
        m_headerWritten = true;
    }
    uint64_t currentFrameIndex = pState->nFramePutIndex;
    const uint32_t GPU_DELAY = MICROPROFILE_GPU_FRAME_DELAY + 1;
    if (currentFrameIndex <= m_lastFlushedFrameIndex + GPU_DELAY)
        return;
    uint64_t endFrameIndex = currentFrameIndex - GPU_DELAY;
    uint64_t maxHistory = MICROPROFILE_MAX_FRAME_HISTORY - GPU_DELAY - 2;
    if (endFrameIndex - m_lastFlushedFrameIndex > maxHistory)
    {
        m_lastFlushedFrameIndex = endFrameIndex - maxHistory;
    }
    for (uint64_t frameIdx = m_lastFlushedFrameIndex; frameIdx < endFrameIndex;
         frameIdx++)
    {
        uint32_t ringIdx =
            static_cast<uint32_t>(frameIdx % MICROPROFILE_MAX_FRAME_HISTORY);
        uint32_t nextRingIdx = static_cast<uint32_t>(
            (frameIdx + 1) % MICROPROFILE_MAX_FRAME_HISTORY);
        writeFrameEvents(writer, pState, ringIdx, nextRingIdx);
    }
    m_lastFlushedFrameIndex = endFrameIndex;
#endif
}

#ifdef RIVE_MICROPROFILE
namespace
{
std::vector<ArtboardPathSegment> buildArtboardPath(Artboard* artboard,
                                                   RiveProfile* profile)
{
    std::vector<ArtboardPathSegment> pathSegments;
    if (artboard == nullptr || profile == nullptr)
    {
        return pathSegments;
    }
    Artboard* current = artboard;
    while (ArtboardHost* host = current->host())
    {
        ArtboardPathSegment segment;
        if (host->type() == NestedArtboardBase::typeKey)
        {
            auto na = static_cast<NestedArtboard*>(host);
            segment.type = 0;
            segment.nameId = profile->resolveStringId(na->name());
            segment.index = -1;
        }
        else if (host->type() == ArtboardComponentListBase::typeKey)
        {
            auto acl = static_cast<ArtboardComponentList*>(host);
            segment.type = 1;
            segment.nameId = profile->resolveStringId(acl->name());
            segment.index = current->isInstance()
                                ? acl->indexOfArtboardInstance(
                                      static_cast<ArtboardInstance*>(current))
                                : -1;
        }
        else
        {
            break;
        }
        pathSegments.push_back(segment);
        ArtboardPathSegment abSegment;
        abSegment.type = 0;
        abSegment.nameId = profile->resolveStringId(current->name());
        abSegment.index = -1;
        pathSegments.push_back(abSegment);
        current = host->parentArtboard();
        if (current == nullptr)
        {
            break;
        }
    }
    std::reverse(pathSegments.begin(), pathSegments.end());
    return pathSegments;
}
} // namespace
#endif

void RiveProfile::recordTransition(std::string artboardName,
                                   std::string smName,
                                   std::string layerName,
                                   std::string fromStateName,
                                   std::string toStateName,
                                   Artboard* artboardForPath)
{
#ifdef RIVE_MICROPROFILE
    if ((m_logFlags & ProfileLogTransitionRecords) == 0)
        return;
    if (!isActive() || !m_flushCallback)
        return;
    TransitionRecord rec;
    rec.artboardId = resolveStringId(artboardName);
    rec.smId = resolveStringId(smName);
    rec.layerId = resolveStringId(layerName);
    rec.fromStateId = resolveStringId(fromStateName);
    rec.toStateId = resolveStringId(toStateName);
    rec.tick = MP_TICK();
    if (artboardForPath != nullptr)
        rec.path = buildArtboardPath(artboardForPath, this);
    m_transitionRecords.push_back(std::move(rec));
#endif
}

uint32_t RiveProfile::resolveStringId(const std::string& str)
{
#ifdef RIVE_MICROPROFILE
    auto it = m_stringTable.find(str);
    if (it != m_stringTable.end())
    {
        return it->second;
    }
    const uint32_t id = static_cast<uint32_t>(m_stringList.size());
    m_stringTable[str] = id;
    m_stringList.push_back(str);
    return id;
#else
    (void)str;
    return 0;
#endif
}

const std::vector<std::string>& RiveProfile::getStringTable() const
{
    return m_stringList;
}

void RiveProfile::setFlushCallback(FlushCallback callback)
{
    m_flushCallback = std::move(callback);
}

void RiveProfile::flushTransitionRecords()
{
    if (m_flushCallback && !m_transitionRecords.empty())
    {
        m_flushCallback(m_transitionRecords);
    }
    m_transitionRecords.clear();
}

void RiveProfile::recordListenerPerformChange(std::string artboardName,
                                              std::string smName,
                                              std::string listenerName,
                                              uint32_t listenerType,
                                              uint32_t hitEvent,
                                              uint32_t pointerId)
{
#ifdef RIVE_MICROPROFILE
    if ((m_logFlags & ProfileLogListenerPerformChanges) == 0)
        return;
    if (!isActive() || !m_listenerPerformChangeFlushCallback)
        return;
    ListenerPerformChangeRecord rec;
    rec.artboardId = resolveStringId(artboardName);
    rec.smId = resolveStringId(smName);
    rec.listenerNameId = resolveStringId(listenerName);
    rec.listenerType = listenerType;
    rec.hitEvent = hitEvent;
    rec.pointerId = pointerId;
    rec.tick = MP_TICK();
    m_listenerPerformChangeRecords.push_back(std::move(rec));
#else
    (void)artboardName;
    (void)smName;
    (void)listenerName;
    (void)listenerType;
    (void)hitEvent;
    (void)pointerId;
#endif
}

void RiveProfile::setListenerPerformChangeFlushCallback(
    ListenerPerformChangeFlushCallback callback)
{
    m_listenerPerformChangeFlushCallback = std::move(callback);
}

void RiveProfile::flushListenerPerformChangeRecords()
{
    if (m_listenerPerformChangeFlushCallback &&
        !m_listenerPerformChangeRecords.empty())
    {
        m_listenerPerformChangeFlushCallback(m_listenerPerformChangeRecords);
    }
    m_listenerPerformChangeRecords.clear();
}

} // namespace rive
