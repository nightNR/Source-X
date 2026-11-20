/**
* @file CResourceLink.h
*
*/

#ifndef _INC_CRESOURCELINK_H
#define _INC_CRESOURCELINK_H

#include "../sphere_library/CSString.h"
#include "../CScriptContexts.h"
#include "CResourceDef.h"
#include <vector>

class CResourceScript;


// Each array can store 8 bytes * 4 bits per byte = 32 bits, or 32 values for 32 triggers.
#define MAX_TRIGGERS_ARRAY	10

class CResourceLink : public CResourceDef
{
    // A single resource object that also has part of itself remain in resource file.
    // A pre-indexed link into a script file.
    // This is a CResourceDef not fully read into memory at index time.
    // We are able to lock it and read it as needed
private:
    CResourceScript * m_pScript;	// we already found the script.
    CScriptLineContext m_Context;

    dword _dwRefInstances;	// How many CResourceRef objects refer to this ?

    // Position right after each ON=@Trigger line of the linked section, filled by ScanSection.
    // Lets the trigger code jump straight to a trigger instead of reading the section line by line.
    struct TriggerContext
    {
        CSString sName;
        CScriptLineContext context;
    };
    std::vector<TriggerContext> _vTriggerContexts;
    bool _fTriggerContextsValid;    // false until ScanSection runs on the current link

public:
    dword m_dwOnTriggers[MAX_TRIGGERS_ARRAY];
    static const char *m_sClassName;

#define XTRIG_UNKNOWN 0	// bit 0 is reserved to say there are triggers here that do not conform.

public:
    inline dword GetRefInstances() const noexcept
    {
        return _dwRefInstances;
    }

    inline void AddRefInstance() noexcept
    {
        ++_dwRefInstances;
    }

    void DelRefInstance();

    bool IsLinked() const;	// been loaded from the scripts ?
    CResourceScript * GetLinkFile() const;
    int GetLinkOffset() const;
    void SetLink( CResourceScript * pScript );
    void CopyTransfer( CResourceLink * pLink );
    // pfMultiRegion (optional): set to whether the section has a MULTIREGION key.
    void ScanSection( RES_TYPE restype, bool *pfMultiRegion = nullptr );
    void ClearTriggers();
    void SetTrigger( int i );
    bool HasTrigger( int i ) const;
    bool ResourceLock( CResourceLock & s );

    // Lock the script and place it right after the ON= line of the given trigger, like ResourceLock + CScriptObj::OnTriggerFind.
    // RETURN: 1 = found, s is ready for CScriptObj::OnTriggerScriptFound.
    //         0 = the section has no such trigger.
    //        -1 = the script can't be locked (same as ResourceLock failing).
    int ResourceLockTrigger( CResourceLock & s, lpctstr ptcTrigName );

public:
    CResourceLink(const CResourceID& rid, const CVarDefContNum * pDef = nullptr);
    virtual ~CResourceLink();

    CResourceLink(const CResourceLink& copy) = delete;
    CResourceLink& operator=(const CResourceLink& other) = delete;
};

#endif // _INC_CRESOURCELINK_H
