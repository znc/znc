/*
 * droproot.cpp
 *
 * Copyright (c) 2009 Vadtec (vadtec@vadtec.net)
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/znc.h>
#include <znc/User.h>
#include <pwd.h>
#include <grp.h>

class CDroproot : public CModule {

public:
	MODCONSTRUCTOR(CDroproot) {
	}

	virtual ~CDroproot() {
	}

	uid_t GetUser(const CString& sUser, CString& sMessage) {
		uid_t ret = sUser.ToUInt();

		if (ret != 0)
			return ret;

		struct passwd *pUser = getpwnam(sUser.c_str());

		if (!pUser) {
			sMessage = "User [" + sUser + "] not found!";
			return 0;
		}

		return pUser->pw_uid;
	}

	gid_t GetGroup(const CString& sGroup, CString& sMessage) {
		gid_t ret = sGroup.ToUInt();

		if (ret != 0)
			return ret;

		struct group *pGroup = getgrnam(sGroup.c_str());

		if (!pGroup) {
			sMessage = "Group [" + sGroup + "] not found!";
			return 0;
		}

		return pGroup->gr_gid;
	}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		CString sUser = sArgs.Token(0);
		CString sGroup = sArgs.Token(1, true);

		if (sUser.empty() || sGroup.empty()) {
			sMessage = "Usage: LoadModule = Droproot <uid> <gid>";
			return false;
		}

		m_user = GetUser(sUser, sMessage);

		if (m_user == 0) {
			sMessage
					= "Error: Cannot run as root, check your config file | Useage: LoadModule = Droproot <uid> <gid>";
			return false;
		}

		m_group = GetGroup(sGroup, sMessage);

		if (m_group == 0) {
			sMessage
					= "Error: Cannot run as root, check your config file | Useage: LoadModule = Droproot <uid> <gid>";
			return false;
		}

		return true;
	}

	virtual bool OnBoot() {
		int u, eu, g, eg, sg;

		if ((geteuid() == 0) || (getuid() == 0) || (getegid() == 0) || (getgid()
				== 0)) {

			CUtils::PrintAction("Dropping root permissions");

			// Clear all the supplementary groups
			sg = setgroups(0, NULL);

			if (sg < 0) {
				CUtils::PrintStatus(false,
						"Could not remove supplementary groups! ["
								+ CString(strerror(errno)) + "]");

				return false;
			}

			// Set the group (if we are root, this sets all three group IDs)
			g = setgid(m_group);
			eg = setegid(m_group);

			if ((g < 0) || (eg < 0)) {
				CUtils::PrintStatus(false, "Could not switch group id! ["
						+ CString(strerror(errno)) + "]");

				return false;
			}

			// and set the user (if we are root, this sets all three user IDs)
			u = setuid(m_user);
			eu = seteuid(m_user);

			if ((u < 0) || (eu < 0)) {
				CUtils::PrintStatus(false, "Could not switch user id! ["
						+ CString(strerror(errno)) + "]");

				return false;
			}

			CUtils::PrintStatus(true);

			return true;
		}

		return true;
	}

protected:
	uid_t m_user;
	gid_t m_group;
};

GLOBALMODULEDEFS(CDroproot, "Allows ZNC to drop root privileges and run as an un-privileged user.")
