//#include "../common/CException.h" // included in the precompiled header
#include "../common/CLog.h"
#include "../game/chars/CChar.h"
#include "../game/items/CItem.h"
#include "CSector.h"
#include "CWorldSearch.h"
#include <algorithm>
#include <array>

static constexpr size_t kuiContainerScaleFactor = 2;


// This was done before we added ObjectPool class.
class CWorldSearchHolderImpl
{
    static constexpr size_t kuiPreallocateSize = 20;
    static constexpr size_t kuiCachedInstances = 20;
    std::array<CSReferenceCountedOwned<CWorldSearch>, kuiCachedInstances> _instances;

public:
    ~CWorldSearchHolderImpl() noexcept = default;
    CWorldSearchHolderImpl() noexcept = default;

    CSReferenceCounted<CWorldSearch> GetOne(const CPointMap& pt, int iDist)
    {
        for (auto& inst : _instances)
        {
            //if (inst.use_count() == 1)
            if (inst._counted_references == 1)
            {
                // It's free, it's only referenced by me (i'm owning it).
                inst->Reset(pt, iDist);
                return inst.GetRef();
            }
        }

        // No free instances!
        throw CSError(LOGL_CRIT, 0, "Not enough instances of CWorldSearch!");
    }
};

//--------------

CSReferenceCounted<CWorldSearch> CWorldSearchHolder::GetInstance(const CPointMap& pt, int iDist)    // static
{
    // Thread-unsafe!
    static CWorldSearchHolderImpl holder;
    return holder.GetOne(pt, iDist);
}

CWorldSearch::CWorldSearch() noexcept :
    _iDist(0), _fAllShow(false), _fSearchSquare(false),
    _fInertToggle(false), _eSearchType(ws_search_e::None),
    _ppCurContObjs(nullptr), _pObj(nullptr),
    _uiCurObjIndex(0), _uiObjArrayCapacity(0), _uiObjArraySize(0),
    _uiTileCharIndex(0), _pTileCharsList(nullptr), _uiTileCharsModCount(0), _fTileChars(false),
    _iSectorCur(0),  // Get upper left of search rect.
    _pSectorBase(nullptr), _pSector(nullptr),
    _rectSectorIndexingHints{},
    _distanceFunction(nullptr), _fMaxAxisDist(false)
{
}

CWorldSearch::CWorldSearch(size_t uiPreallocateSize) :
    CWorldSearch()
{
    if (!uiPreallocateSize)
        return;

    _uiObjArrayCapacity = uiPreallocateSize * kuiContainerScaleFactor;
    _ppCurContObjs = new CSObjContRec* [_uiObjArrayCapacity];
}

CWorldSearch::~CWorldSearch() noexcept
{
    if (nullptr != _ppCurContObjs)
        delete[] _ppCurContObjs;
}

void CWorldSearch::Reset(const CPointMap& pt, int iDist)
{
    //ADDTOCALLSTACK("CWorldSearch::Reset");
    // define a search of the world.

    _fAllShow = _fSearchSquare = _fInertToggle = false;
    _eSearchType = ws_search_e::None;
    _pObj = nullptr;
    _uiCurObjIndex = _uiObjArraySize = 0;
    _fTileChars = false;
    //_uiObjArrayCapacity = 0;   // Don't! Recycle the allocated space for _ppCurContObjs.
    _iSectorCur = 0; // Get upper left of search rect.

    _pt = pt;
    _iDist = iDist;

    _pSectorBase = _pSector = pt.GetSector();
    _rectSector.SetRect(
        pt.m_x - iDist,
        pt.m_y - iDist,
        pt.m_x + iDist + 1,
        pt.m_y + iDist + 1,
        pt.m_map);

    _rectSectorIndexingHints = _rectSector.PrecomputeSectorIndexingHints();

    SetDistanceFunction();
}

void CWorldSearch::SetAllShow(bool fView)
{
	//ADDTOCALLSTACK_DEBUG("CWorldSearch::SetAllShow");
    if (_fAllShow == fView)
        return;

    _fAllShow = fView;
    SetDistanceFunction();
}

void CWorldSearch::SetSearchSquare(bool fSquareSearch)
{
	//ADDTOCALLSTACK_DEBUG("CWorldSearch::SetSearchSquare");
    if (_fSearchSquare == fSquareSearch)
        return;

    _fSearchSquare = fSquareSearch;
    SetDistanceFunction();
}

void CWorldSearch::RestartSearch()
{
	//ADDTOCALLSTACK_DEBUG("CWorldSearch::RestartSearch");
	_eSearchType = ws_search_e::None;
	_pObj = nullptr;
    _uiCurObjIndex = _uiObjArraySize = 0;
    _fTileChars = false;
    _pSector = _pSectorBase;
    // We could memset 0 _ppCurContObjs, but it isn't actually necessary...
}

bool CWorldSearch::GetNextSector()
{
    ADDTOCALLSTACK_DEBUG("CWorldSearch::GetNextSector");
	// Move search into nearby CSector(s) if necessary

	if (!_iDist)
		return false;

	while (true)
	{
        _pSector = _rectSector.GetSectorAtIndexWithHints(_iSectorCur, _rectSectorIndexingHints);
        _iSectorCur += 1;
		if (_pSector == nullptr)
			return false;	// done searching.
		if (_pSectorBase == _pSector)
			continue;	// same as base.

		_eSearchType = ws_search_e::None;
		_pObj = nullptr;	// start at head of next Sector.
        _uiCurObjIndex = _uiObjArraySize = 0;
        _fTileChars = false;

		return true;
	}
}

void CWorldSearch::ReserveObjArray(size_t uiCount)
{
    bool fAllocate = false;
    if (_ppCurContObjs != nullptr)
    {
        if (_uiObjArrayCapacity < uiCount)
        {
            delete[] _ppCurContObjs;
            fAllocate = true;
        }
    }
    else
    {
        fAllocate = true;
    }

    if (fAllocate)
    {
        _uiObjArrayCapacity = uiCount * kuiContainerScaleFactor;
        _ppCurContObjs = new CSObjContRec * [_uiObjArrayCapacity];
    }
}

void CWorldSearch::LoadSectorObjs(CSObjCont const& pSectorObjList)
{
    ADDTOCALLSTACK_DEBUG("CWorldSearch::LoadSectorObjs");
    const size_t sector_obj_num = pSectorObjList.size();
    if (0 != sector_obj_num)
    {
        ReserveObjArray(sector_obj_num);
        memcpy(_ppCurContObjs, pSectorObjList.data(), sector_obj_num * sizeof(CSObjContRec*)); // I need this to be as fast as possible
    }

    _uiObjArraySize = sector_obj_num;
    _uiCurObjIndex = 0;
}

void CWorldSearch::SetDistanceFunction()
{
    if (_fAllShow)
    {
        if (_fSearchSquare)
            _distanceFunction = &CPointMap::GetDistSightBase;
        else
            _distanceFunction = &CPointMap::GetDistBase;
    }
    else
    {
        if (_fSearchSquare)
            _distanceFunction = &CPointMap::GetDistSight;
        else
            _distanceFunction = &CPointMap::GetDist;
    }

    // GetDistSightBase and GetDistSight always use max(dx, dy), GetDistBase and GetDist do it with the default distance formula.
    _fMaxAxisDist = (_fSearchSquare || (g_Cfg.m_iDistanceFormula == DISTANCE_FORMULA_NODIAGONAL_NOZ));
}

void CWorldSearch::LoadSectorItems()
{
    // Only the items in a single tile: use the index of the sector, if it has one.
    CItemsList& items = _pSector->m_Items;
    const CItemsList::TileIndexEntry *pFirst, *pLast;
    if ((_iDist != 0) || !items.FindTileItems(_pt.m_x, _pt.m_y, pFirst, pLast))
    {
        LoadSectorObjs(items);
        return;
    }

    const size_t uiCount = (size_t)(pLast - pFirst);
    if (uiCount != 0)
    {
        ReserveObjArray(uiCount);
        for (size_t i = 0; i < uiCount; ++i)
        {
            _ppCurContObjs[i] = items.GetContentIndex(pFirst[i].uiIndex);
        }
    }
    _uiObjArraySize = uiCount;
    _uiCurObjIndex = 0;
}

void CWorldSearch::LoadSectorChars()
{
    CCharsActiveList& chars = _pSector->m_Chars_Active;
    LoadSectorObjs(chars);
    _fTileChars = false;

    // Only the chars in a single tile: use the index of the sector, if it has one.
    // The whole list is copied anyway: if it changes while we are returning the chars, we go on like without the index,
    //  checking every char in the copy after the current one.
    const CCharsActiveList::TileIndexEntry *pFirst, *pLast;
    if ((_iDist != 0) || (_uiObjArraySize == 0) || !chars.FindTileChars(_pt.m_x, _pt.m_y, pFirst, pLast))
    {
        return;
    }

    _vTileChars.clear();
    for (const CCharsActiveList::TileIndexEntry* pEntry = pFirst; pEntry != pLast; ++pEntry)
    {
        _vTileChars.emplace_back(pEntry->pChar);
    }
    _uiTileCharIndex = 0;
    _pTileCharsList = &chars;
    _uiTileCharsModCount = chars.GetModCount();
    _fTileChars = true;
}

CItem* CWorldSearch::GetItem()
{
	// This method is called very frequently, ADDTOCALLSTACK unneededly sucks cpu
	//ADDTOCALLSTACK_DEBUG("CWorldSearch::GetItem");

	while (true)
	{
		if (_pObj == nullptr)
		{
			ASSERT(_eSearchType == ws_search_e::None);
			_eSearchType = ws_search_e::Items;

            LoadSectorItems();
		}
		else
		{
            ++_uiCurObjIndex;
		}

		ASSERT(_eSearchType == ws_search_e::Items);
        if (_uiCurObjIndex >= _uiObjArraySize)
		{
            _pObj = nullptr;
			if (GetNextSector())
				continue;

			return nullptr;
		}

        _pObj = static_cast<CObjBase*>(_ppCurContObjs[_uiCurObjIndex]);
        if (IsInRange(_pObj->GetTopPoint()))
            return static_cast <CItem*> (_pObj);
	}
}

CChar* CWorldSearch::GetChar()
{
	// This method is called very frequently, ADDTOCALLSTACK unneededly sucks cpu
	//ADDTOCALLSTACK_DEBUG("CWorldSearch::GetChar");

	while (true)
	{
		if (_pObj == nullptr)
		{
			ASSERT(_eSearchType == ws_search_e::None);
			_eSearchType = ws_search_e::Chars;
			_fInertToggle = false;

            LoadSectorChars();
		}
        else if (_fTileChars)
        {
            if (_pTileCharsList->GetModCount() == _uiTileCharsModCount)
            {
                ++_uiTileCharIndex;
            }
            else
            {
                // The list changed (a char was added, removed or moved): check every char after the current one,
                //  like the search without the index would do. Nothing changes while we skip chars in the same call,
                //  so the current one is the last we returned.
                _fTileChars = false;
                const auto ppEnd = _ppCurContObjs + _uiObjArraySize;
                _uiCurObjIndex = (size_t)(std::find(_ppCurContObjs, ppEnd, _pObj) - _ppCurContObjs) + 1;
            }
        }
		else
		{
            ++_uiCurObjIndex;
		}

		ASSERT(_eSearchType == ws_search_e::Chars);
        if (_fTileChars)
        {
            if (_uiTileCharIndex < _vTileChars.size())
            {
                _pObj = static_cast<CObjBase*>(_vTileChars[_uiTileCharIndex]);
                if (IsInRange(_pObj->GetTopPoint()))
                    return static_cast <CChar*> (_pObj);
                continue;
            }
            _fTileChars = false;
            _uiCurObjIndex = _uiObjArraySize;   // Done with this list.
        }

        if (_uiCurObjIndex >= _uiObjArraySize)
		{
            _pObj = nullptr;
			if (!_fInertToggle && _fAllShow)
			{
				_fInertToggle = true;

                LoadSectorObjs(_pSector->m_Chars_Disconnect);
                if (_uiCurObjIndex < _uiObjArraySize)
					goto jumpover;
                _pObj = nullptr;
			}

			if (GetNextSector())
				continue;

			return nullptr;
		}

	jumpover:
        _pObj = static_cast<CObjBase*>(_ppCurContObjs[_uiCurObjIndex]);
        if (IsInRange(_pObj->GetTopPoint()))
            return static_cast <CChar*> (_pObj);
	}
}
