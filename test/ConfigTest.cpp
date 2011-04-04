/*
 * Copyright (C) 2004-2011  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "ZNCDebug.h"
#include "FileUtils.h"
#include "Config.h"
#include <cstdlib>

class CConfigTest {
public:
	CConfigTest(const CString& sConfig) : m_sConfig(sConfig) { }
	virtual ~CConfigTest() { m_File.Delete(); }

	virtual bool Test() = 0;

protected:
	CFile& WriteFile() {
		char sName[] = "./temp-XXXXXX";
		int fd = mkstemp(sName);
		m_File.Open(sName, O_RDWR);
		close(fd);

		m_File.Write(m_sConfig);

		return m_File;
	}

private:
	CFile   m_File;
	CString m_sConfig;
};

class CConfigErrorTest : public CConfigTest {
public:
	CConfigErrorTest(const CString& sConfig, const CString& sError)
		: CConfigTest(sConfig), m_sError(sError) { }

	bool Test() {
		CFile &File = WriteFile();

		CConfig conf;
		CString sError;
		bool res = conf.Parse(File, sError);
		if (res) {
			std::cout << "Didn't error out!\n";
			return false;
		}

		if (sError != m_sError) {
			std::cout << "Wrong error\n Expected: " << m_sError << "\n Got: " << sError << std::endl;
			return false;
		}

		return true;
	}
private:
	CString m_sError;
};

class CConfigSuccessTest : public CConfigTest {
public:
	CConfigSuccessTest(const CString& sConfig, const CString& sExpectedOutput)
		: CConfigTest(sConfig), m_sOutput(sExpectedOutput) { }

	bool Test() {
		CFile &File = WriteFile();
		// Verify that Parse() rewinds the file
		File.Seek(12);

		CConfig conf;
		CString sError;
		bool res = conf.Parse(File, sError);
		if (!res) {
			std::cout << "Error'd out! (" + sError + ")\n";
			return false;
		}
		if (!sError.empty()) {
			std::cout << "Non-empty error string!\n";
			return false;
		}

		CString sOutput;
		ToString(sOutput, conf);

		if (sOutput != m_sOutput) {
			std::cout << "Wrong output\n Expected: " << m_sOutput << "\n Got: " << sOutput << std::endl;
			return false;
		}

		return true;
	}

	void ToString(CString& sRes, CConfig& conf) {
		CConfig::EntryMapIterator it = conf.BeginEntries();
		while (it != conf.EndEntries()) {
			const CString& sKey = it->first;
			const VCString& vsEntries = it->second;
			VCString::const_iterator i = vsEntries.begin();
			if (i == vsEntries.end())
				sRes += sKey + " <- Error, empty list!\n";
			else
				while (i != vsEntries.end()) {
					sRes += sKey + "=" + *i + "\n";
					i++;
				}
			++it;
		}

		CConfig::SubConfigMapIterator it2 = conf.BeginSubConfigs();
		while (it2 != conf.EndSubConfigs()) {
			map<CString, CConfig::CConfigEntry>::const_iterator it3 = it2->second.begin();

			while (it3 != it2->second.end()) {
				sRes += "->" + it2->first + "/" + it3->first + "\n";
				ToString(sRes, *it3->second.m_pSubConfig);
				sRes += "<-\n";
				++it3;
			}

			++it2;
		}
	}

private:
	CString m_sOutput;
};

int main() {
#define TEST_ERROR(a, b) new CConfigErrorTest(a, b)
#define TEST_SUCCESS(a, b) new CConfigSuccessTest(a, b)
#define ARRAY_SIZE(a) (sizeof(a)/(sizeof((a)[0])))
	CConfigTest *tests[] = {
		TEST_SUCCESS("", ""),
		/* duplicate entries */
		TEST_SUCCESS("Foo = bar\nFoo = baz\n", "foo=bar\nfoo=baz\n"),
		TEST_SUCCESS("Foo = baz\nFoo = bar\n", "foo=baz\nfoo=bar\n"),
		/* sub configs */
		TEST_ERROR("</foo>", "Error on line 1: Closing tag \"foo\" which is not open."),
		TEST_ERROR("<foo a>\n</bar>\n", "Error on line 2: Closing tag \"bar\" which is not open."),
		TEST_ERROR("<foo bar>", "Error on line 1: Not all tags are closed at the end of the file. Inner-most open tag is \"foo\"."),
		TEST_ERROR("<foo>\n</foo>", "Error on line 1: Empty block name at begin of block."),
		TEST_ERROR("<foo 1>\n</foo>\n<foo 1>\n</foo>", "Error on line 4: Duplicate entry for tag \"foo\" name \"1\"."),
		TEST_SUCCESS("<foo a>\n</foo>", "->foo/a\n<-\n"),
		TEST_SUCCESS("<a b>\n  <c d>\n </c>\n</a>", "->a/b\n->c/d\n<-\n<-\n"),
		TEST_SUCCESS(" \t <A B>\nfoo = bar\n\tFooO = bar\n</a>", "->a/B\nfoo=bar\nfooo=bar\n<-\n"),
		/* comments */
		TEST_SUCCESS("Foo = bar // baz\n// Bar = baz", "foo=bar // baz\n"),
		TEST_SUCCESS("Foo = bar /* baz */\n/*** Foo = baz ***/\n   /**** asdsdfdf \n Some quite invalid stuff ***/\n", "foo=bar /* baz */\n"),
		TEST_ERROR("<foo foo>\n/* Just a comment\n</foo>", "Error on line 3: Comment not closed at end of file."),
		TEST_SUCCESS("/* Foo\n/* Bar */", ""),
		TEST_SUCCESS("/* Foo\n// */", ""),
	};
	unsigned int i;
	unsigned int failed = 0;

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		if (!tests[i]->Test())
			failed++;
		delete tests[i];
	}

	return failed;
}
