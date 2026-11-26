/**
* @file CSectorBase.h
*
*/

#ifndef _INC_CSECTORBASE_H
#define _INC_CSECTORBASE_H

#include "../common/sphere_library/CSObjCont.h"
#include "../common/sphere_library/CSObjSortArray.h"
#include "../common/CRect.h"
#include "CTeleport.h"

class CChar;
class CItem;
class CSector;
class CTeleport;

class CSectorObjCont
{
    // Marker class, consider it as a "tag".
};

class CCharsDisconnectList : public CSObjCont, public CSectorObjCont
{
public:
	CCharsDisconnectList();
    virtual ~CCharsDisconnectList();
	CCharsDisconnectList(const CCharsDisconnectList& copy) = delete;
	CCharsDisconnectList& operator=(const CCharsDisconnectList& other) = delete;

	void AddCharDisconnected(CChar* pChar);
};

class CCharsActiveList : public CSObjCont, public CSectorObjCont
{
public:
    // Index of the chars by their x,y position, used to find the chars in a single tile without checking every char
    //  in the sector. Built only when needed, then kept updated when a char is added, removed or moves (chars move
    //  all the time, rebuilding it would cost more than checking the whole list). Any other change of the list
    //  (seen through the modification counter of CSObjCont) makes it build again on the next search.
    struct TileIndexEntry
    {
        uint uiKey;     // x << 16 | y
        uint uiSeq;     // Increases with the position in the list: the entries of a tile are in the order of the list.
        CSObjContRec* pChar;
    };
    static constexpr size_t kuiTileIndexMinChars = 32;  // Smaller lists are checked whole: it's faster.

private:
	int m_iClients;				// How many clients in this sector now?
	int64 m_iTimeLastClient;	// age the sector based on last client here.

    std::vector<TileIndexEntry> _vTileIndex;  // Sorted by key, then seq.
    uint _uiTileIndexModCount;  // Modification counter of the list the index is up to date with.
    uint _uiTileIndexNextSeq;
    bool _fTileIndexValid;

protected:
	void OnRemoveObj(CSObjContRec* pObjRec);	// Override this = called when removed from list.
    virtual void OnTrackedContentPosChanged(const CSObjContRec* pRec, short iOldX, short iOldY, short iNewX, short iNewY) noexcept override;

public:
	CCharsActiveList();
    ~CCharsActiveList();
	CCharsActiveList(const CCharsActiveList& copy) = delete;
	CCharsActiveList& operator=(const CCharsActiveList& other) = delete;

	void AddCharActive(CChar* pChar);
	int GetClientsNumber() const noexcept {
		return m_iClients;
	}
	int64 GetTimeLastClient() const noexcept {
		return m_iTimeLastClient;
	}
	void SetTimeLastClient(int64 iTime) noexcept {
		m_iTimeLastClient = iTime;
	}

    /**
    * @brief Find the chars at the given position (z and map not checked), in the same order they have in the list.
    *   The returned entries are valid until the contents (or the position of a char) change.
    * @return false if the list is too small to have an index: check the whole list instead.
    */
    bool FindTileChars(short x, short y, const TileIndexEntry*& pFirst, const TileIndexEntry*& pLast);

private:
    void _BuildTileIndex();
    bool _TileIndexCanUpdate() noexcept;
    void _TileIndexUpdated() noexcept;
    TileIndexEntry* _TileIndexFind(const CSObjContRec* pChar, short x, short y) noexcept;
};

class CItemsList : public CSObjCont, public CSectorObjCont
{
public:
    // Index of the items by their x,y position, used to find the items in a single tile without checking every item
    //  in the sector. Built only when needed, and built again on the first search after the contents changed.
    struct TileIndexEntry
    {
        uint uiKey;     // x << 16 | y
        uint uiIndex;   // Index in the contents.
    };
    static constexpr size_t kuiTileIndexMinItems = 32;  // Smaller lists are checked whole: it's faster.

private:
    std::vector<TileIndexEntry> _vTileIndex;
    uint _uiTileIndexModCount;
    bool _fTileIndexValid;

public:
	CItemsList() noexcept;
	CItemsList(const CItemsList& copy) = delete;
	CItemsList& operator=(const CItemsList& other) = delete;

	void AddItemToSector( CItem * pItem );

    /**
    * @brief Find the items at the given position (z not checked), in the same order they have in the list.
    *   The returned entries are valid until the contents (or the position of an item) change.
    * @return false if the list is too small to have an index: check the whole list instead.
    */
    bool FindTileItems(short x, short y, const TileIndexEntry*& pFirst, const TileIndexEntry*& pLast);

protected:
	void OnRemoveObj(CSObjContRec* pObRec);	// Override this = called when removed from list.

private:
    void _BuildTileIndex();
};


class CSectorBase		// world sector
{
	template <typename T>
    class CObjPointSortArray : public CSObjSortArray< T*, int >
    {
		static_assert(std::is_base_of_v<CPointSort, T>, "Type has to inherit from CPointSort");

    public:
        static const char *m_sClassName;

		CObjPointSortArray() = default;
		virtual ~CObjPointSortArray() override = default;

		CObjPointSortArray(const CObjPointSortArray& copy) = delete;
		CObjPointSortArray& operator=(const CObjPointSortArray& other) = delete;

        // Find a point fast.
		virtual int CompareKey(int id, T* pBase, bool fNoSpaces) const override {
			UnreferencedParameter(fNoSpaces);
			return (id - pBase->GetPointSortIndex());
		}
    };


protected:
    int	m_index;            // Sector index in the 'sector grid'
    CPointBase m_BasePointSectUnits; // Sector coordinates in the 'sector grid'.
    CRectMap   m_MapRectWorldUnits;   // Map area inside this sector, in map coordinates.

private:
    // TODO: store indices instead of pointers, to make the class smaller?
	CSector* _ppAdjacentSectors[DIR_QTY];

public:
	static const char  *m_sClassName;
	CObjPointSortArray<CTeleport>	m_Teleports;		//	CTeleport array
	CRegionLinks		m_RegionLinks;		//	CRegion(s) in this CSector
	dword			    m_dwFlags;

	CCharsActiveList		m_Chars_Active;		// CChar(s) activte in this CSector.
	CCharsDisconnectList	m_Chars_Disconnect;	// dead NPCs, etc
    CItemsList m_Items;                         // CItem(s) in this CSector (not relevant if they have a timer set or not).

public:
    /*
    * @brief Assign its adjacent sectors
    */
    void SetAdjacentSectors();

protected:
    CSector *_GetAdjacentSector(DIR_TYPE dir) const;

public:
	CSectorBase();
	virtual ~CSectorBase() = default;

	CSectorBase(const CSectorBase& copy) = delete;
	CSectorBase& operator=(const CSectorBase& other) = delete;

public:
	virtual void Init(int index, uchar map, short x, short y);

	// Location map units.
    int GetIndex() const noexcept               { return m_index; }
    int GetMap() const noexcept                 { return m_BasePointSectUnits.m_map; }
    CPointBase GetBasePointMapUnits() const noexcept;
    constexpr const CPointBase& GetBasePointSectUnits() noexcept    { return m_BasePointSectUnits; }
    constexpr const CRectMap& GetRectWorldUnits() const noexcept    { return m_MapRectWorldUnits; }

	// CRegion
	CRegion * GetRegion( const CPointBase & pt, dword dwType ) const;
	size_t GetRegions( const CPointBase & pt, dword dwType, CRegionLinks *pRLinks ) const;

	bool UnLinkRegion( CRegion * pRegionOld );
	bool LinkRegion( CRegion * pRegionNew );

	// CTeleport(s) in the region.
	CTeleport * GetTeleport( const CPointMap & pt ) const;
	bool AddTeleport( CTeleport * pTeleport );

    constexpr bool IsFlagSet( dword dwFlag ) const noexcept {
        return (m_dwFlags & dwFlag);
    }

#define SECF_NoSleep	0x00000001
#define SECF_InstaSleep	0x00000002
};

#endif // _INC_CSECTORBASE_H
