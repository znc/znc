/* C implementation by Christophe Devine, C++ "class-ified" by [T3] */

#ifndef _MD5_H
#define _MD5_H

#include "zncconfig.h"
#include <string>
using std::string;

#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif

typedef struct
{
    uint32 total[2];
    uint32 state[4];
    uint8 buffer[64];
}
md5_context;

class CMD5 {
protected:
	char m_szMD5[33];

public:
	CMD5();
	CMD5(const string& sText);
	CMD5(const char* szText, uint32 nTextLen);
	~CMD5();

	operator string() const
	{
		return (string) m_szMD5;
	}

	operator char*() const
	{
		return (char*)m_szMD5;
	}

	char* MakeHash(const char* szText, uint32 nTextLen);

protected:
	void md5_starts( md5_context *ctx ) const;
	void md5_update( md5_context *ctx, const uint8 *input, uint32 length ) const;
	void md5_finish( md5_context *ctx, uint8 digest[16] ) const;

private:
	void md5_process( md5_context *ctx, const uint8 data[64] ) const;
};

#endif /* _MD5_H */
