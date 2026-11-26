#include "../common/CLog.h"
#include "chars/CChar.h"
#include "CEntityProps.h"
#include <algorithm>
#include <vector>

CEntityProps::CEntityProps()
{
    _lComponentProps.clear();
}

CEntityProps::~CEntityProps()
{
    ADDTOCALLSTACK("CEntityProps::~CEntityProps");
    ClearPropComponents();
}

void CEntityProps::ClearPropComponents()
{
    ADDTOCALLSTACK("CEntityProps::ClearPropComponents");
    if (_lComponentProps.empty())
        return;

#ifdef _DEBUG
    for (auto it = _lComponentProps.begin(), itEnd = _lComponentProps.end(); it != itEnd; ++it)
    {
        CComponentProps *pComponent = it->second.get();
        ASSERT(pComponent);
    }
#endif

    _lComponentProps.clear();
}

void CEntityProps::UnsubscribeComponentProps(CComponentProps *pComponent)
{
    if (_lComponentProps.empty())
    {
        return;
    }
    const COMPPROPS_TYPE compType = pComponent->GetType();
    auto it = _lComponentProps.find(compType);
    if (it == _lComponentProps.end())
    {
        g_Log.EventError("Trying to unsuscribe not suscribed prop component (%d)\n", (int)pComponent->GetType());    // Should never happen?
        return;
    }
    _lComponentProps.erase(it);  // iterator invalidation!
}

CComponentProps * CEntityProps::GetComponentProps(COMPPROPS_TYPE type)
{
    ASSERT(type < COMP_PROPS_QTY);
    if (_lComponentProps.empty())
    {
        return nullptr;
    }
    auto it = _lComponentProps.find(type);
    return (it == _lComponentProps.end()) ? nullptr : it->second.get();
}

const CComponentProps * CEntityProps::GetComponentProps(COMPPROPS_TYPE type) const
{
    if (!_lComponentProps.empty())
    {
        auto it = _lComponentProps.find(type);
        return (it == _lComponentProps.end()) ? nullptr : it->second.get();
    }
    return nullptr;
}

void CEntityProps::r_Write(CScript & s) // Storing data in the worldsave.
{
    ADDTOCALLSTACK("CEntityProps::r_Write");
    if (_lComponentProps.empty() && !s.IsWriteMode())
        return;
    for (auto it = _lComponentProps.begin(), itEnd = _lComponentProps.end(); it != itEnd; ++it)
    {
        CComponentProps *pComponent = it->second.get();
        ASSERT(pComponent);
        pComponent->r_Write(s);
    }
}


bool CEntityProps::CEPLoopLoad(CEPLoopRet_t *pRet, CScript& s, CObjBase* pLinkedObj, const RESDISPLAY_VERSION iLimitToExpansion)
{
    const lpctstr ptcKey = s.GetKey();
    CEPLoopRet_t localRet(*pRet); // init with implicit copy constructor (working with a copy on the stack should be faster?)
    for (const auto& pairElem : _lComponentProps)
    {
        if (CComponentProps* pComponent = pairElem.second.get())
        {
            const KeyTableDesc_s ktd = pComponent->GetPropertyKeysData();
            localRet.iPropIndex = (CComponentProps::PropertyIndex_t) FindTableSorted(ptcKey, ktd.pptcTable, ktd.iTableSize - 1);
            if (localRet.iPropIndex == (CComponentProps::PropertyIndex_t) - 1)
            {
                // The key doesn't belong to this CComponentProps.
                continue;
            }
            localRet.fPropStr = pComponent->IsPropertyStr(localRet.iPropIndex);
            if (pComponent->FindLoadPropVal(s, pLinkedObj, iLimitToExpansion, localRet.iPropIndex, localRet.fPropStr))
            {
                // The property belongs to this CCP and it is set.
                *pRet = localRet;
                return true;
            }
            // The property belongs to this CCP but is not set. Remember which CCP this is, so if needed we can search directly for this CCP type instead of looping through all of them again.
            localRet.iCCPType = pComponent->GetType();
            *pRet = localRet;
            return true;
        }
    }
    return false;
}

bool CEntityProps::r_LoadPropVal(CScript & s, CObjBase* pObjEntityProps, CBaseBaseDef *pBaseEntityProps) // static
{
    ADDTOCALLSTACK("CEntityProps::r_LoadPropVal");
    // return false: invalid property for any of the subscribed components
    // return true: valid property, whether it has a defined value or not

    ASSERT(pBaseEntityProps);
    const RESDISPLAY_VERSION iLimitToExpansion = pBaseEntityProps->_iEraLimitProps;
    CEPLoopRet_t loopRet{ (CComponentProps::PropertyIndex_t) - 1, false, COMP_PROPS_INVALID };

    if (pObjEntityProps == nullptr)    // I'm calling it from a base CEntityProps
    {
        if (pBaseEntityProps->_lComponentProps.empty())
            return false;
        return pBaseEntityProps->CEPLoopLoad(&loopRet, s, nullptr, iLimitToExpansion);  // pEntityProps is already a base cep
    }

    ASSERT(pObjEntityProps);
    if (!pObjEntityProps->_lComponentProps.empty() && pObjEntityProps->CEPLoopLoad(&loopRet, s, pObjEntityProps, iLimitToExpansion)) // Check the object dynamic props first, then the base
    {
        // A subscribed CComponentProps can store this prop...
        if (loopRet.iCCPType == COMP_PROPS_INVALID)
        {
            // The prop is set. We found it.
            return true;
        }
        else
        {
            // But the prop isn't set. Let's check the base. We already have iPropIndex and fPropStr.
            CComponentProps *pComp = pBaseEntityProps->GetComponentProps(loopRet.iCCPType);
            if (!pComp)
                return true;    // The base doesn't have this component, but the obj did -> return true
            ASSERT(loopRet.iPropIndex != (CComponentProps::PropertyIndex_t)-1);
            pComp->FindLoadPropVal(s, nullptr, iLimitToExpansion, (CComponentProps::PropertyIndex_t)loopRet.iPropIndex, loopRet.fPropStr);
            return true;        // return true regardlessly of the value being set or not (it's still a valid property)
        }
    }

    return ((pBaseEntityProps->_lComponentProps.empty() == false) && pBaseEntityProps->CEPLoopLoad(&loopRet, s, nullptr, iLimitToExpansion));
}


struct CEntityProps::CEPKey_t
{
    lpctstr ptcKey;
    uchar uiQty;                                                // How many CComponentProps types have this key.
    COMPPROPS_TYPE piType[COMP_PROPS_QTY];                      // In COMPPROPS_TYPE order, like _lComponentProps.
    CComponentProps::PropertyIndex_t piPropIndex[COMP_PROPS_QTY];
};

namespace
{
    // The property keys of every CComponentProps type, in COMPPROPS_TYPE order (the same tables of the CComponentProps classes).
    #define ADDPROP(a,b,c) b,
    lpctstr const sk_ptcKeysItemChar[] = {
        #include "../tables/CCPropsItemChar_props.tbl"
    };
    lpctstr const sk_ptcKeysChar[] = {
        #include "../tables/CCPropsChar_props.tbl"
    };
    lpctstr const sk_ptcKeysItem[] = {
        #include "../tables/CCPropsItem_props.tbl"
    };
    lpctstr const sk_ptcKeysItemEquippable[] = {
        #include "../tables/CCPropsItemEquippable_props.tbl"
    };
    lpctstr const sk_ptcKeysItemWeapon[] = {
        #include "../tables/CCPropsItemWeapon_props.tbl"
    };
    lpctstr const sk_ptcKeysItemWeaponRanged[] = {
        #include "../tables/CCPropsItemWeaponRanged_props.tbl"
    };
    #undef ADDPROP

    struct CEPKeyTables_s
    {
        lpctstr const *pptcTable;
        int iCount;
    };
    const CEPKeyTables_s sk_KeyTables[] =
    {
        { sk_ptcKeysItemChar,           (int)ARRAY_COUNT(sk_ptcKeysItemChar) },             // COMP_PROPS_ITEMCHAR
        { sk_ptcKeysChar,               (int)ARRAY_COUNT(sk_ptcKeysChar) },                 // COMP_PROPS_CHAR
        { sk_ptcKeysItem,               (int)ARRAY_COUNT(sk_ptcKeysItem) },                 // COMP_PROPS_ITEM
        { sk_ptcKeysItemEquippable,     (int)ARRAY_COUNT(sk_ptcKeysItemEquippable) },       // COMP_PROPS_ITEMEQUIPPABLE
        { sk_ptcKeysItemWeapon,         (int)ARRAY_COUNT(sk_ptcKeysItemWeapon) },           // COMP_PROPS_ITEMWEAPON
        { sk_ptcKeysItemWeaponRanged,   (int)ARRAY_COUNT(sk_ptcKeysItemWeaponRanged) }      // COMP_PROPS_ITEMWEAPONRANGED
    };
    static_assert(ARRAY_COUNT(sk_KeyTables) == COMP_PROPS_QTY, "A key table is missing for a CComponentProps type.");
}

const CEntityProps::CEPKey_t* CEntityProps::CEPFindKey(lpctstr ptcKey) // static
{
    // A single lookup tells if the key is a property of any CComponentProps type, and of which ones,
    //  instead of a lookup in the key table of every CComponentProps of the object and of its base.
    struct MergedKeys_s
    {
        std::vector<CEPKey_t> vKeys;
        std::vector<lpctstr> vptcKeys;  // The keys of vKeys, sorted, for the lookup.
    };
    static const MergedKeys_s s_merged = []()
    {
        struct KeyRef_s
        {
            lpctstr ptcKey;
            COMPPROPS_TYPE iType;
            CComponentProps::PropertyIndex_t iPropIndex;
        };
        std::vector<KeyRef_s> vRefs;
        for (int iType = 0; iType < COMP_PROPS_QTY; ++iType)
        {
            const CEPKeyTables_s& table = sk_KeyTables[iType];
            for (int i = 0; i < table.iCount; ++i)
            {
                // Keep only the keys the binary search in the CComponentProps key table finds, so the result stays the same.
                if (FindTableSorted(table.pptcTable[i], table.pptcTable, table.iCount) == i)
                    vRefs.push_back({ table.pptcTable[i], (COMPPROPS_TYPE)iType, (CComponentProps::PropertyIndex_t)i });
            }
        }
        // Stable: the types of the same key stay in COMPPROPS_TYPE order.
        std::stable_sort(vRefs.begin(), vRefs.end(),
            [](const KeyRef_s& lhs, const KeyRef_s& rhs) { return strcmpi(lhs.ptcKey, rhs.ptcKey) < 0; });

        MergedKeys_s merged;
        for (const KeyRef_s& ref : vRefs)
        {
            if (merged.vKeys.empty() || (strcmpi(merged.vKeys.back().ptcKey, ref.ptcKey) != 0))
                merged.vKeys.push_back({ ref.ptcKey, 0, {}, {} });
            CEPKey_t& key = merged.vKeys.back();
            ASSERT(key.uiQty < COMP_PROPS_QTY);
            key.piType[key.uiQty] = ref.iType;
            key.piPropIndex[key.uiQty] = ref.iPropIndex;
            ++key.uiQty;
        }
        merged.vptcKeys.reserve(merged.vKeys.size());
        for (const CEPKey_t& key : merged.vKeys)
            merged.vptcKeys.push_back(key.ptcKey);
        return merged;
    }();

    const int i = FindTableSortedCached(ptcKey, s_merged.vptcKeys.data(), (int)s_merged.vptcKeys.size());
    return (i < 0) ? nullptr : &s_merged.vKeys[i];
}

bool CEntityProps::CEPLoopWrite(CEPLoopRet_t* pRet, const CEPKey_t& key, CSString& sVal) const
{
    // Same order as looping through _lComponentProps: the first subscribed CComponentProps having the key gets it.
    for (uchar i = 0; i < key.uiQty; ++i)
    {
        if (const CComponentProps* pComponent = GetComponentProps(key.piType[i]))
        {
            pRet->iPropIndex = key.piPropIndex[i];
            DEBUG_ASSERT(!strcmpi(pComponent->GetPropertyKeysData().pptcTable[pRet->iPropIndex], key.ptcKey));
            pRet->fPropStr = pComponent->IsPropertyStr(pRet->iPropIndex);
            if (pComponent->FindWritePropVal(sVal, pRet->iPropIndex, pRet->fPropStr))
            {
                // The property belongs to this CCP and it is set.
                return true;
            }
            // The property belongs to this CCP but is not set. Remember which CCP this is, so if needed we can search directly for this CCP type instead of looping through all of them again.
            pRet->iCCPType = pComponent->GetType();
            return true;
        }
    }
    return false;
}

bool CEntityProps::r_WritePropVal(lpctstr ptcKey, CSString & sVal, const CObjBase *pObjEntityProps, const CBaseBaseDef *pBaseEntityProps) // static
{
    ADDTOCALLSTACK("CEntityProps::r_WritePropVal");
    // return false: invalid property for any of the subscribed components
    // return true: valid property, whether it has a defined value or not

    CEPLoopRet_t loopRet{ (CComponentProps::PropertyIndex_t) - 1, false, COMP_PROPS_INVALID };

    if (pObjEntityProps == nullptr)    // I'm calling it from a base CEntityProps
    {
        ASSERT(pBaseEntityProps);
        if (pBaseEntityProps->_lComponentProps.empty())
            return false;

        const CEPKey_t *pKey = CEPFindKey(ptcKey);
        return pKey && pBaseEntityProps->CEPLoopWrite(&loopRet, *pKey, sVal);  // pEntityProps is already a base cep
    }

    ASSERT(pObjEntityProps);
    const CEPKey_t *pKey = CEPFindKey(ptcKey);
    if (!pKey)
        return false;   // Not a property of any CComponentProps type.

    if (!pObjEntityProps->_lComponentProps.empty() && pObjEntityProps->CEPLoopWrite(&loopRet, *pKey, sVal)) // Check the object dynamic props first, then the base
    {
        // A subscribed CComponentProps can store this prop...
        if (loopRet.iCCPType == COMP_PROPS_INVALID)
        {
            // The prop is set. We found it.
            return true;
        }
        else
        {
            // But the prop isn't set. Let's check the base. We already have iPropIndex and fPropStr.
            ASSERT(pBaseEntityProps);
            const CComponentProps *pComp = pBaseEntityProps->GetComponentProps(loopRet.iCCPType);
            if (!pComp)
                return true;    // The base doesn't have this component, but the obj did -> return true
            ASSERT(loopRet.iPropIndex != (CComponentProps::PropertyIndex_t)-1);
            pComp->FindWritePropVal(sVal, (CComponentProps::PropertyIndex_t)loopRet.iPropIndex, loopRet.fPropStr);
            return true;        // return true regardlessly of the value being set or not (it's still a valid property)
        }
    }

    return (pBaseEntityProps && (pBaseEntityProps->_lComponentProps.empty() == false) && pBaseEntityProps->CEPLoopWrite(&loopRet, *pKey, sVal));
}

void CEntityProps::AddPropsTooltipData(CObjBase* pObj)
{
    ADDTOCALLSTACK("CEntityProps::AddPropsTooltipData");
    if (_lComponentProps.empty())
        return;
    for (auto it = _lComponentProps.begin(), itEnd = _lComponentProps.end(); it != itEnd; ++it)
    {
        CComponentProps *pComponent = it->second.get();
        ASSERT(pComponent);
        pComponent->AddPropsTooltipData(pObj);
    }
    return;
}

void CEntityProps::Copy(const CEntityProps *target)
{
    ADDTOCALLSTACK("CEntityProps::Copy");
    if (_lComponentProps.empty())
        return;
    for (auto it = target->_lComponentProps.begin(), itEnd = target->_lComponentProps.end(); it != itEnd; ++it)
    {
        CComponentProps *pTarget = it->second.get();    // the CComponent to copy from
        ASSERT(pTarget);
        CComponentProps *pCopy = GetComponentProps(pTarget->GetType());    // the CComponent to copy to.
        if (pCopy)
        {
            pCopy->Copy(pTarget);
        }
    }
}

void CEntityProps::DumpComponentProps(CTextConsole *pSrc, lpctstr ptcPrefix) const
{
    ADDTOCALLSTACK("CEntityProps::DumpComponentProps");
    // List out all the keys.
    ASSERT(pSrc);

    if ( ptcPrefix == nullptr )
        ptcPrefix = "";

    bool fIsClient = false;
    if (const CChar* pChar = pSrc->GetChar())
    {
        fIsClient = pChar->IsClientActive();
    }

    CSString sPropVal;
    CComponentProps::PropertyValNum_t iPropVal;
    for (CComponentProps::PropertyIndex_t iCCP = 0; iCCP < COMP_PROPS_QTY; ++iCCP)
    {
        const CComponentProps* pCCP = GetComponentProps((COMPPROPS_TYPE)iCCP);
        if (!pCCP)
            continue;
        lpctstr ptcCCPName = pCCP->GetName();
        const CComponentProps::PropertyIndex_t iCCPQty = pCCP->GetPropsQty();
        for (CComponentProps::PropertyIndex_t iPropIndex = 0; iPropIndex < iCCPQty; ++iPropIndex)
        {
            bool fPropFound;
            if (pCCP->IsPropertyStr(iPropIndex))
            {
                fPropFound = pCCP->GetPropertyStrPtr(iPropIndex, &sPropVal, false);
            }
            else
            {
                fPropFound = pCCP->GetPropertyNumPtr(iPropIndex, &iPropVal);
                if (fPropFound)
                    sPropVal.Format("%d", iPropVal);
            }

            if (fPropFound)
            {
                lpctstr ptcPropName = pCCP->GetPropertyName(iPropIndex);
                if (fIsClient)
                    pSrc->SysMessagef("%s[%s]%s=%s", ptcPrefix, ptcCCPName, ptcPropName, sPropVal.GetBuffer());
                else
                    g_Log.Event(LOGL_EVENT,"%s[%s]%s=%s\n", ptcPrefix, ptcCCPName, ptcPropName, sPropVal.GetBuffer());
            }

        }
    }
}
