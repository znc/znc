/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "ZNCString.h"
#include "ZNCDebug.h"

static int testEqual(const CString& a, const CString& b, const CString& what)
{
	if (a == b)
		return 0;
	std::cout << what << " failed for '" << b << "', result is '" << a << "'\n";
	return 1;
}

static int testString(const CString& in, const CString& url,
		const CString& html, const CString& sql) {
	CString out;
	int errors = 0;

	// Encode, then decode again and check we still got the same string

	out = in.Escape_n(CString::EASCII, CString::EURL);
	errors += testEqual(out, url, "EURL encode");
	out = out.Escape_n(CString::EURL, CString::EASCII);
	errors += testEqual(out, in, "EURL decode");

	out = in.Escape_n(CString::EASCII, CString::EHTML);
	errors += testEqual(out, html, "EHTML encode");
	out = out.Escape_n(CString::EHTML, CString::EASCII);
	errors += testEqual(out, in, "EHTML decode");

	out = in.Escape_n(CString::EASCII, CString::ESQL);
	errors += testEqual(out, sql, "ESQL encode");
	out = out.Escape_n(CString::ESQL, CString::EASCII);
	errors += testEqual(out, in, "ESQL decode");

	return errors;
}

int main() {
	unsigned int failed = 0;

	//                    input      url          html             sql
	failed += testString("abcdefg", "abcdefg",   "abcdefg",       "abcdefg");
	failed += testString("\n\t\r",  "%0A%09%0D", "\n\t\r",        "\\n\\t\\r");
	failed += testString("'\"",     "%27%22",    "'&quot;",       "\\'\\\"");
	failed += testString("&<>",     "%26%3C%3E", "&amp;&lt;&gt;", "&<>");

	return failed;
}
