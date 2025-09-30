/**
* @file CSectorList.h
*
*/

#ifndef _INC_CSECTORLIST_H
#define _INC_CSECTORLIST_H

#include "uo_files/CUOMapList.h"
#include "CSector.h"


// Sector data for a given map
struct MapSectorsData
{
    friend class CSectorList;

    // Pre-calculated values, for faster retrieval
    int iSectorSize;
    int iSectorColumns;  // how much sectors are in a column (x) in a given map
    int iSectorRows;     // how much sectors are in a row (y) in a given map
    int iSectorQty;      // how much sectors are in a map
    uint16 uiSectorDivShift;    // precalculated value to avoid a division in sector/rect calculations

private:
    std::unique_ptr<CSector[]> _pSectors;
};

class CSectorList
{
public:
	static const char* m_sClassName;

    //std::array<MapSectorsData, MAP_SUPPORTED_QTY> _SectorData; // Use plain C-style vectors, to remove even the minimal overhead of std::array methods
	MapSectorsData _SectorData[MAP_SUPPORTED_QTY];
	bool _fInitialized;

public:
	CSectorList();
	~CSectorList();

	CSectorList(const CSectorList& copy) = delete;
	CSectorList& operator=(const CSectorList& other) = delete;

public:
    static const CSectorList& Get() noexcept;

	void Init();
	void Close(bool fClosingWorld);

    [[nodiscard]]
    const MapSectorsData& GetMapSectorData(int map) const;

    CSector* GetSectorByIndex(int map, int index) const noexcept;	// gets sector # from one map
    CSector* GetSectorByCoordsUnchecked(int map, short x, short y) const noexcept;

	int GetSectorAbsoluteQty() const noexcept;
	CSector* GetSectorAbsolute(int index) noexcept;
};


#endif // _INC_CSECTORLIST_H
