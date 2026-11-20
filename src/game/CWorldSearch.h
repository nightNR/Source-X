#ifndef _INC_CWORLDSEARCH_H
#define _INC_CWORLDSEARCH_H

#include "../common/sphere_library/CSReferenceCount.h"
#include "../common/CRect.h"

/*
#ifdef MSVC_COMPILER
#   pragma warning(push)
#   pragma warning(disable : 4244)
#   pragma warning(disable : 4267)
#   pragma warning(disable : 4324)
#endif

#include <cdrc/rc_ptr.h>

#ifdef MSVC_COMPILER
#   pragma warning(pop)
#endif
*/

class CSObjContRec;
class CObjBase;
class CSector;
class CChar;
class CItem;


// Search for dynamic objects on a specified zone
class CWorldSearch
{
    enum class ws_search_e
    {
        None = 0,
        Items,
        Chars
    };

    int _iDist;                 // How far from the point are we interested in
    CPointMap _pt;              // Base point of our search.
    bool _fAllShow;				// Include Even inert items.
    bool _fSearchSquare;		// Search in a square (uo-sight distance) rather than a circle (standard distance).

    bool _fInertToggle;			// We are now doing the inert chars.
    ws_search_e _eSearchType;

    CSObjContRec** _ppCurContObjs;	// Array with a copy of the pointers to the objects inside the sector we are in searching right now.
    CObjBase* _pObj;			// The current object of interest.
    size_t _uiCurObjIndex;
    size_t _uiObjArrayCapacity;
    size_t _uiObjArraySize;

    int		 _iSectorCur;		// What is the current Sector index in m_rectSector
    CSector* _pSectorBase;		// Don't search the center sector 2 times.
    CSector* _pSector;			// current Sector
    CRectMap _rectSector;		// A rectangle containing our sectors we can search.
    CRectMap::SectIndexingHints_s _rectSectorIndexingHints;

    // Pointer to distance function:
    //  The ::* operator means "pointer to member of CPointMap".
    int (CPointMap::* _distanceFunction)(const CPointBase&) const noexcept;
    bool _fMaxAxisDist;         // The distance function is max(dx, dy): check it inline, without calling _distanceFunction.

public:
    static const char* m_sClassName;
    CWorldSearch(const CWorldSearch& copy) = delete;
    CWorldSearch& operator=(const CWorldSearch& other) = delete;

    CWorldSearch() noexcept;
    CWorldSearch(size_t uiPreallocateSize);
    ~CWorldSearch() noexcept;

public:
    void Reset(const CPointMap& pt, int iDist = 0);
    void SetAllShow(bool fView);
    void SetSearchSquare(bool fSquareSearch);
    void RestartSearch();

    CChar* GetChar();
    CItem* GetItem();

private:
    void SetDistanceFunction();

    inline bool IsInRange(const CPointMap& ptObj) const noexcept;
    bool GetNextSector();
    void LoadSectorObjs(CSObjCont const& pSectorObjList);
};

// This is called for every object in the searched sectors, so keep it inline and cheap.
bool CWorldSearch::IsInRange(const CPointMap& ptObj) const noexcept
{
    if (!_fMaxAxisDist)
    {
        return ((_pt.*_distanceFunction)(ptObj) <= _iDist);
    }

    const int dx = (_pt.m_x > ptObj.m_x) ? (_pt.m_x - ptObj.m_x) : (ptObj.m_x - _pt.m_x);
    const int dy = (_pt.m_y > ptObj.m_y) ? (_pt.m_y - ptObj.m_y) : (ptObj.m_y - _pt.m_y);
    if (((dx >= dy) ? dx : dy) > _iDist)
    {
        return false;
    }
    if (_fAllShow || (_iDist >= INT16_MAX))
    {
        return true;
    }
    // GetDist and GetDistSight return INT16_MAX for a point on another map or not valid: check it only for the objects in range.
    return ((ptObj.m_map == _pt.m_map) && ptObj.IsValidPoint());
}

struct CWorldSearchHolder
{
    // Thread-unsafe!
    static CSReferenceCounted<CWorldSearch> GetInstance(const CPointMap& pt, int iDist = 0);
};

#endif  // _INC_CWORLDSEARCH_H
