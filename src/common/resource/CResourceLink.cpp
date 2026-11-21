/**
* @file CResourceLink.cpp
*
*/

#include "../../game/chars/CChar.h"
#include "../../game/items/CItem.h"
#include "../../game/triggers.h"
#include "../CLog.h"
#include "sections/CSkillDef.h"
#include "sections/CSpellDef.h"
#include "sections/CRegionResourceDef.h"
#include "sections/CWebPageDef.h"
#include "CResourceLock.h"
#include "CResourceLink.h"


CResourceLink::CResourceLink(const CResourceID& rid, const CVarDefContNum * pDef) :
    CResourceDef( rid, pDef )
{
    m_pScript = nullptr;
    _dwRefInstances = 0;
    _fTriggerContextsValid = false;
    ClearTriggers();
}

CResourceLink::~CResourceLink() = default;

void CResourceLink::ScanSection( RES_TYPE restype )
{
    ADDTOCALLSTACK("CResourceLink::ScanSection");
    // Scan the section we are linking to for useful stuff.
    ASSERT(m_pScript);
    lpctstr const * ppTable = nullptr;
    int iQty = 0;

    switch (restype)
    {
        case RES_CHAMPION:
        case RES_TYPEDEF:
        case RES_ITEMDEF:
            ppTable = CItem::sm_szTrigName;
            iQty = ITRIG_QTY;
            break;
        case RES_CHARDEF:
        case RES_EVENTS:
        case RES_SKILLCLASS:
            ppTable = CChar::sm_szTrigName;
            iQty = CTRIG_QTY;
            break;
        case RES_SKILL:
            ppTable = CSkillDef::sm_szTrigName;
            iQty = SKTRIG_QTY;
            break;
        case RES_SPELL:
            ppTable = CSpellDef::sm_szTrigName;
            iQty = SPTRIG_QTY;
            break;
        case RES_AREA:
        case RES_ROOM:
        case RES_REGIONTYPE:
            ppTable = CRegionWorld::sm_szTrigName;
            iQty = RTRIG_QTY;
            break;
        case RES_WEBPAGE:
            ppTable = CWebPageDef::sm_szTrigName;
            iQty = WTRIG_QTY;
            break;
        case RES_REGIONRESOURCE:
            ppTable = CRegionResourceDef::sm_szTrigName;
            iQty = RRTRIG_QTY;
            break;
        default:
            break;
    }
    ClearTriggers();
    _vTriggerContexts.clear();

    while ( m_pScript->ReadKey(false) )
    {
        if ( m_pScript->IsKeyHead( "DEFNAME", 7 ) )
        {
            m_pScript->ParseKeyLate();
            SetResourceName( m_pScript->GetArgRaw() );
        }
        else if ( m_pScript->IsKeyHead( "ON", 2 ) )
        {
            // Remember where the first section with this name starts, CScriptObj::OnTriggerFind stops at the first match too.
            m_pScript->ParseKeyLate();
            lpctstr ptcTrigName = m_pScript->GetArgRaw();
            if (_vTriggerContexts.end() == std::find_if(_vTriggerContexts.begin(), _vTriggerContexts.end(),
                [ptcTrigName](const TriggerContext& tc) { return !strcmpi(tc.sName.GetBuffer(), ptcTrigName); }))
            {
                _vTriggerContexts.push_back({CSString(ptcTrigName), m_pScript->GetContext()});
            }

            int iTrigger;
            if ( iQty )
            {
                iTrigger = FindTableSorted( m_pScript->GetArgRaw(), ppTable, iQty );

                if ( iTrigger < 0 )	// unknown triggers ?
                    iTrigger = XTRIG_UNKNOWN;
                else
                {
                    TriglistAdd(m_pScript->GetArgRaw());
                    if ( HasTrigger(iTrigger) )
                    {
                        DEBUG_ERR(( "Duplicate trigger '%s' in '%s'\n", ppTable[iTrigger], GetResourceName() ));
                        continue;
                    }
                }
            }
            else
            {
                iTrigger = XTRIG_UNKNOWN;
            }

            SetTrigger(iTrigger);
        }
    }
    _fTriggerContextsValid = true;
}

void CResourceLink::DelRefInstance()
{
#ifdef _DEBUG
    ASSERT(_dwRefInstances != (dword)-1);    // catching underflows
#endif
    --_dwRefInstances;
}

bool CResourceLink::IsLinked() const
{
    ADDTOCALLSTACK("CResourceLink::IsLinked");
    if ( !m_pScript )
        return false;
    return m_Context.IsValid();
}

CResourceScript *CResourceLink::GetLinkFile() const
{
    ADDTOCALLSTACK("CResourceLink::GetLinkFile");
    return m_pScript;
}

int CResourceLink::GetLinkOffset() const
{
    ADDTOCALLSTACK("CResourceLink::GetLinkOffset");
    return m_Context.m_iOffset;
}

void CResourceLink::SetLink(CResourceScript *pScript)
{
    ADDTOCALLSTACK("CResourceLink::SetLink");
    m_pScript = pScript;
    m_Context = pScript->GetContext();
    _vTriggerContexts.clear();
    _fTriggerContextsValid = false;     // until ScanSection runs
}

void CResourceLink::CopyTransfer(CResourceLink *pLink)
{
    ADDTOCALLSTACK("CResourceLink::CopyTransfer");
    ASSERT(pLink);
    CResourceDef::CopyDef( pLink );
    m_pScript = pLink->m_pScript;
    m_Context = pLink->m_Context;
    memcpy(m_dwOnTriggers, pLink->m_dwOnTriggers, sizeof(m_dwOnTriggers));
    _vTriggerContexts = pLink->_vTriggerContexts;
    _fTriggerContextsValid = pLink->_fTriggerContextsValid;
    _dwRefInstances = pLink->_dwRefInstances;
    pLink->_dwRefInstances = 0;	// instance has been transfered.
}

void CResourceLink::ClearTriggers()
{
    memset(m_dwOnTriggers, 0, sizeof(m_dwOnTriggers));
}

void CResourceLink::SetTrigger(int i)
{
    ADDTOCALLSTACK("CResourceLink::SetTrigger");
    if ( i >= 0 )
    {
        for ( dword j = 0; j < MAX_TRIGGERS_ARRAY; ++j )
        {
            if ( i < 32 )
            {
                const dword flag = 1u << i;
                m_dwOnTriggers[j] |= flag;
                return;
            }
            i -= 32;
        }
    }
}

bool CResourceLink::HasTrigger(int i) const
{
    ADDTOCALLSTACK("CResourceLink::HasTrigger");
    // Specific to the RES_TYPE; CTRIG_QTY, ITRIG_QTY or RTRIG_QTY
    if ( i < XTRIG_UNKNOWN )
        i = XTRIG_UNKNOWN;

    for ( dword j = 0; j < MAX_TRIGGERS_ARRAY; ++j )
    {
        if ( i < 32 )
        {
            const dword flag = 1u << i;
            return ((m_dwOnTriggers[j] & flag) != 0);
        }
        i -= 32;
    }
    return false;
}

bool CResourceLink::ResourceLock( CResourceLock &s )
{
    ADDTOCALLSTACK("CResourceLink::ResourceLock");
    // Find the definition of this item in the scripts.
    // Open a locked copy of this script
    // NOTE: How can we tell the file has changed since last open ?
    // RETURN: true = found it.
    if ( !IsLinked() )	// has already failed previously.
        return false;

    ASSERT(m_pScript);

    //	Give several tryes to lock the script while multithreading
    int iRet = s.OpenLock( m_pScript, m_Context );
    if ( ! iRet )
        return true;

    s.AttachObj( this );

    // ret = -2 or -3
    lpctstr pszName = GetResourceName();
    DEBUG_ERR(("ResourceLock '%s':%d id=%s FAILED\n", s.GetFilePath(), m_Context.m_iOffset, pszName));

    return false;
}

int CResourceLink::ResourceLockTrigger( CResourceLock &s, lpctstr ptcTrigName )
{
    ADDTOCALLSTACK_DEBUG("CResourceLink::ResourceLockTrigger");
    if ( !_fTriggerContextsValid )
    {
        // Section not scanned: read it line by line, as before.
        if ( !ResourceLock(s) )
            return -1;
        return OnTriggerFind(s, ptcTrigName) ? 1 : 0;
    }

    if ( !IsLinked() )
        return -1;

    const auto it = std::find_if(_vTriggerContexts.begin(), _vTriggerContexts.end(),
        [ptcTrigName](const TriggerContext& tc) { return !strcmpi(tc.sName.GetBuffer(), ptcTrigName); });
    if ( it == _vTriggerContexts.end() )
        return 0;   // not in this section, no need to lock the script

    ASSERT(m_pScript);
    const int iRet = s.OpenLock( m_pScript, it->context );
    if ( !iRet )
        return 1;

    s.AttachObj( this );
    DEBUG_ERR(("ResourceLockTrigger '%s':%d id=%s trigger=%s FAILED\n", s.GetFilePath(), it->context.m_iOffset, GetResourceName(), ptcTrigName));
    return -1;
}
