/**
* @file CCacheableScriptFile.h
*
*/

#ifndef _INC_CACHEABLESCRIPTFILE_H
#define _INC_CACHEABLESCRIPTFILE_H

#include <string>
#include <vector>
#include "sphere_library/CSFileText.h"


// Precomputed parsing of a cached script line, done once when the file is loaded instead of on every read.
// The offsets are relative to the start of the line, kuiNone means none (or that the line can't use the fast path).
struct CScriptLineInfo
{
    static constexpr ushort kuiNone = 0xFFFF;

    ushort uiLen;       // Length of the line without the comment and the trailing whitespace, 0 if the line is blank.
    ushort uiKey;       // Start of the key (Str_Parse), kuiNone if the line has to be parsed as usual.
    ushort uiArg;       // Start of the argument.
    ushort uiNul;       // Where Str_Parse ends the key with a '\0', kuiNone if it doesn't.
};
static_assert(sizeof(CScriptLineInfo) == 8);

class CCacheableScriptFile : public CSFileText
{
public:
	CCacheableScriptFile();
	~CCacheableScriptFile();

    CCacheableScriptFile(const CCacheableScriptFile& copy) = delete;
    CCacheableScriptFile& operator=(const CCacheableScriptFile& other) = delete;

protected:  virtual bool _Open(lpctstr ptcFilename = nullptr, uint uiModeFlags = OF_READ|OF_SHARE_DENY_NONE) override;
public:     virtual bool Open(lpctstr ptcFilename = nullptr, uint uiModeFlags = OF_READ|OF_SHARE_DENY_NONE) override;
protected:  virtual void _Close() override;
public:     virtual void Close() override;
            virtual bool _IsFileOpen() const override;
            virtual bool IsFileOpen() const override;
protected:  virtual int _Seek(int iOffset = 0, int iOrigin = SEEK_SET) override;
public:     virtual int Seek(int iOffset = 0, int iOrigin = SEEK_SET) override;

protected:  virtual bool _IsEOF() const override;
public:     virtual bool IsEOF() const override;
protected:  virtual int _GetPosition() const override;
public:     virtual int GetPosition() const override;

protected:  virtual tchar * _ReadString(tchar *pBuffer, int sizemax) override;
public:     virtual tchar * ReadString(tchar *pBuffer, int sizemax) override;

protected:
    void _dupeFrom(CCacheableScriptFile *other);
    void dupeFrom(CCacheableScriptFile *other);

    // Reads the next non blank line from the cache, already trimmed as by CScriptKeyAlloc::ParseKeyEnd.
    // RETURN: 1 = line read, 0 = end of file, -1 = the next line must be read as usual.
    //  iLinesRead is increased by the lines consumed, blank ones included.
    int _ReadTrimmedLine(tchar *pBuffer, int& iLinesRead, const CScriptLineInfo*& pInfo);

protected:  bool _HasCache() const;
public:     bool HasCache() const;

public:
	bool _fClosed;
	bool _fRealFile;
	int _iCurrentLine;

protected:
	std::vector<std::string>* _fileContent; // It's better to have a pointer so that CResourceLock can access to this
    std::vector<CScriptLineInfo>* _lineInfo; // One for each line of _fileContent, built by CScript. Owned like _fileContent.

protected:  bool _useDefaultFile() const;
//public:     bool useDefaultFile() const;
};

#endif // _INC_CACHEABLESCRIPTFILE_H
