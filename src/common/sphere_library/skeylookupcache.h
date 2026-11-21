/**
* @file skeylookupcache.h
* @brief Small per-thread cache for the lookups of script keywords in sorted tables.
*/

#ifndef _INC_SKEYLOOKUPCACHE_H
#define _INC_SKEYLOOKUPCACHE_H

#include "../datatypes.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sl
{

// Script code resolves the same keys ("FLAGS", "DISTANCE", ...) over and over, and each of them goes through several
// searches in keyword tables (or in the loaded resources) before it is found, or not found at all.
// The result of a search only depends on the key and on the table, so remember it.
// Use it only for tables whose content doesn't change, or call Invalidate() every time it changes.
// A cache miss runs the usual lookup, so the result is always the same as without the cache.
class KeyLookupCache
{
public:
    // The same table can be searched in different ways (full key, key head...), with a different result.
    enum class Kind : uchar
    {
        Sorted = 1,     // FindTableSorted
        HeadSorted,     // FindTableHeadSorted
        Custom          // Any other lookup, pTable identifies it.
    };

    static constexpr size_t kuiMaxKeyLen = 47;  // Longer keys (usually with arguments) are not cached.
    static constexpr size_t kuiSlots = 4096;    // Must be a power of 2.

    // Forget everything cached, in every thread.
    static void Invalidate() noexcept
    {
        _uiGeneration.fetch_add(1, std::memory_order_relaxed);
    }

    template <typename TFunc>
    static auto Find(Kind kind, const void *pTable, int iCount, const tchar *ptcKey, TFunc &&fnLookup) -> decltype(fnLookup())
    {
        using Ret_t = decltype(fnLookup());

        // FNV-1a hash of the key.
        uint uiHash = 2166136261u;
        size_t uiLen = 0;
        for (; ptcKey[uiLen] != '\0'; ++uiLen)
        {
            if (uiLen == kuiMaxKeyLen)
                return fnLookup();
            uiHash = (uiHash ^ (uchar)ptcKey[uiLen]) * 16777619u;
        }
        const uintptr_t uiTable = (uintptr_t)pTable;
        uiHash ^= (uint)((uiTable >> 3) * 2654435761u) ^ ((uint)kind * 0x85EBCA6Bu) ^ ((uint)iCount * 0xC2B2AE35u);
        uiHash ^= (uiHash >> 15);

        Slot &slot = _GetSlots()[uiHash & (kuiSlots - 1)];
        const uint uiGeneration = _uiGeneration.load(std::memory_order_relaxed);
        if ((slot.uiGeneration == uiGeneration) && (slot.pTable == pTable) && (slot.iCount == iCount) &&
            (slot.kind == kind) && (slot.uiLen == uiLen) && !memcmp(slot.ptcKey, ptcKey, uiLen * sizeof(tchar)))
        {
            return (Ret_t)slot.iResult;
        }

        const Ret_t result = fnLookup();
        // A lookup can load or change the resources (and invalidate the cache): store it only if it didn't.
        if (uiGeneration == _uiGeneration.load(std::memory_order_relaxed))
        {
            slot.pTable = pTable;
            slot.uiGeneration = uiGeneration;
            slot.iCount = iCount;
            slot.iResult = (intptr_t)result;
            slot.kind = kind;
            slot.uiLen = (uchar)uiLen;
            memcpy(slot.ptcKey, ptcKey, uiLen * sizeof(tchar));
        }
        return result;
    }

private:
    struct Slot
    {
        const void *pTable;
        intptr_t iResult;
        uint uiGeneration;  // 0 = empty
        int iCount;
        Kind kind;
        uchar uiLen;
        tchar ptcKey[kuiMaxKeyLen + 1];
    };

    static Slot *_GetSlots()
    {
        thread_local std::unique_ptr<Slot[]> s_pSlots;
        if (!s_pSlots) [[unlikely]]
            s_pSlots = std::make_unique<Slot[]>(kuiSlots);  // Value-initialized: all empty.
        return s_pSlots.get();
    }

    static inline std::atomic<uint> _uiGeneration{1};
};

} // namespace sl

// Same as FindTableSorted and FindTableHeadSorted, for tables that never change.
inline int FindTableSortedCached(const tchar *ptcFind, const tchar * const *pptcTable, int iCount)
{
    return sl::KeyLookupCache::Find(sl::KeyLookupCache::Kind::Sorted, pptcTable, iCount, ptcFind,
        [=]() { return FindTableSorted(ptcFind, pptcTable, iCount); });
}
inline int FindTableHeadSortedCached(const tchar *ptcFind, const tchar * const *pptcTable, int iCount)
{
    return sl::KeyLookupCache::Find(sl::KeyLookupCache::Kind::HeadSorted, pptcTable, iCount, ptcFind,
        [=]() { return FindTableHeadSorted(ptcFind, pptcTable, iCount); });
}

#endif // _INC_SKEYLOOKUPCACHE_H
