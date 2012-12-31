/*
 * Copyright (C) 2004-2013  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>

class CMissingMotd : public CModule {
public:
	MODCONSTRUCTOR(CMissingMotd) {}

	virtual void OnClientLogin() {
		PutUser(":irc.znc.in 422 :MOTD File is missing");
	}
};

USERMODULEDEFS(CMissingMotd, "Sends 422 to clients when they login")

