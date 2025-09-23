/**
* @file CScriptObj.h
* @brief A scriptable object
*/

#ifndef _INC_CSCRIPTOBJ_H
#define _INC_CSCRIPTOBJ_H

#include "../common/common.h"

class CScript;
class CTextConsole;
class CSFileText;
class CSString;
class CUID;
class CChar;

class CScriptTriggerArgs;
using CScriptTriggerArgsPtr = std::shared_ptr<CScriptTriggerArgs>;


enum SK_TYPE : int;

enum TRIGRUN_TYPE
{
	TRIGRUN_SECTION_EXEC,	// Just execute this. (first line already read!)
	TRIGRUN_SECTION_TRUE,	// execute this section normally.
	TRIGRUN_SECTION_FALSE,	// Just ignore this whole section.
	TRIGRUN_SINGLE_EXEC,	// Execute just this line or blocked segment. (first line already read!)
	TRIGRUN_SINGLE_TRUE,	// Execute just this line or blocked segment.
	TRIGRUN_SINGLE_FALSE	// ignore just this line or blocked segment.
};

enum TRIGRET_TYPE	// trigger script returns.
{
    TRIGRET_RET_ABORTED = INT32_MIN,
	TRIGRET_RET_FALSE = 0,	// default return. (script might not have been handled)
	TRIGRET_RET_TRUE = 1,
	TRIGRET_RET_DEFAULT,	// we just came to the end of the script.
    TRIGRET_ENDIF,
	TRIGRET_ELSE,
	TRIGRET_ELSEIF,
	TRIGRET_RET_HALFBAKED,
	TRIGRET_BREAK,
	TRIGRET_CONTINUE,
	TRIGRET_QTY
};


class CScriptObj
{
	// This object can be scripted. (but might not be)

	// Script keywords
	static lpctstr const sm_szScriptKeys[];
	static lpctstr const sm_szLoadKeys[];
	static lpctstr const sm_szVerbKeys[];

public:
	static const char* m_sClassName;
	virtual lpctstr GetName() const = 0;	// ( every object must have at least a type name )


// Script object derefenciation and validation (a keyword might be used on a different CScriptObj than the current one)
public:
	static bool IsValidRef(const CScriptObj* pRef) noexcept;
	static bool IsValidRef(const CUID& uidRef) noexcept;

private:
	// Not virtual, it's meant to be used only by CScriptObj::r_WriteVal and CScriptObj::r_Verb and it takes care
	//	of evaluating some special cases.
	bool r_GetRefFull(lpctstr& ptcKey, CScriptObj*& pRef);

public:
	// Virtual: some object may allow specific refs, allow them to handle them.
	virtual bool r_GetRef(lpctstr& ptcKey, CScriptObj*& pRef);


// Script line parsing:
	/*
	* @brief Get the value of a script keyword (enclosed by < > angular brackets).
	* "WriteVal" means: Write/replace in the script string the requested value.
	* It does check if we are requesting another ref.
	*/
	virtual bool r_WriteVal(lpctstr pKey, CSString& sVal, CTextConsole* pSrc = nullptr, bool fNoCallParent = false, bool fNoCallChildren = false);

	/*
	* @brief Execute a script command.
	* Called when parsing a script section with OnTriggerRun or if issued by a CClient.
	* It does check if we are requesting another ref.
	* It evaluates simple commands ("VERB"), which typically do NOT require a script argument.
	* If it doesn't find a VERB, call r_LoadVal to do a second-level, deeper parsing.
	*/
	virtual bool r_Verb( CScript & s, CTextConsole * pSrc );

	/*
	* @brief Internally sets the corresponding value of a script keyword.
	* WARNING: it's a second-level parsing function, call this if you are sure that calling r_Verb is superfluous.
	* "LoadVal" means: Load the value from the script and store it in our internal structures/data.
	* It does NOT check if we are requesting another ref, since it's already done by r_Verb.
	* Here we evaluate more complex commands, which typically requires also a script argument.
	*/
	virtual bool r_LoadVal(CScript& s);

	virtual bool r_Load(CScript& s);	// Loads the keyword/values of a whole script section

// Hardcoded shortcuts to trigger specific script parsing.
	bool r_ExecSingle(lpctstr ptcLine);
	bool r_SetVal(lpctstr ptcKey, lpctstr ptcVal); // Quick way to try to set a value for a script keyword


// FUNCTION methods
    static size_t r_GetFunctionIndex(lpctstr pszFunction);
    static bool r_CanCall(size_t uiFunctionIndex);
    bool r_Call( lpctstr ptcFunction, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole * pSrc, CSString * psVal = nullptr, TRIGRET_TYPE * piRet = nullptr ); // Try to execute function
    bool r_Call( size_t uiFunctionIndex, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole * pSrc, CSString * psVal = nullptr, TRIGRET_TYPE * piRet = nullptr ); // Try to execute function


// Generic section parsing
    virtual TRIGRET_TYPE OnTrigger(lpctstr pszTrigName, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc);
	bool OnTriggerFind(CScript& s, lpctstr pszTrigName);
    TRIGRET_TYPE OnTriggerScript(CScript& s, lpctstr pszTrigName, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc);
    TRIGRET_TYPE OnTriggerRun(CScript& s, TRIGRUN_TYPE trigger, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc, CSString* pReturn);
    TRIGRET_TYPE OnTriggerRunVal(CScript& s, TRIGRUN_TYPE trigger, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc);


// Special statements
private:
	// While, standard for loop and some special for loops
    TRIGRET_TYPE OnTriggerLoopGeneric(CScript& s, int iType, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc, CSString* pResult);
    TRIGRET_TYPE OnTriggerLoopForCharSpecial(CScript& s, SK_TYPE iCmd, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc, CSString* pResult);
    TRIGRET_TYPE OnTriggerLoopForCont(CScript& s, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc, CSString* pResult);
    TRIGRET_TYPE OnTriggerLoopForContSpecial(CScript& s, SK_TYPE iCmd, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc, CSString* pResult);

	// Special statements
    bool Execute_Call(CScript& s, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc);
    bool Execute_FullTrigger(CScript& s, CScriptTriggerArgsPtr const& pScriptArgs, CTextConsole* pSrc);


// Utilities
protected:
	static bool ParseError_UndefinedKeyword(lpctstr ptcKey);


// Constructors/operators
public:
	CScriptObj() = default;
	virtual ~CScriptObj() = default;

    CScriptObj(const CScriptObj& copy) = delete;
    CScriptObj& operator=(const CScriptObj& other) = delete;
    CScriptObj(CScriptObj&& move) = delete;
};

#endif	// _INC_CSCRIPTOBJ_H
