/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include "Socket.h"

CSockManager::CSockManager() : TSocketManager<CZNCSock>()
{
}

CSockManager::~CSockManager()
{
}
