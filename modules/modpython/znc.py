#
# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#

import imp
import re
import traceback
import collections

from znc_core import *


class Socket:
    ADDR_MAP = {
        'ipv4': ADDR_IPV4ONLY,
        'ipv6': ADDR_IPV6ONLY,
        'all': ADDR_ALL
    }

    def _Accepted(self, host, port):
        return getattr(self.OnAccepted(host, port), '_csock', None)

    def GetModule(self):
        return AsPyModule(self._csock.GetModule()).GetNewPyObj()

    def Listen(self, addrtype='all', port=None, bindhost='', ssl=False,
               maxconns=GetSOMAXCONN(), timeout=0):
        try:
            addr = self.ADDR_MAP[addrtype.lower()]
        except KeyError:
            raise ValueError(
                "Specified addrtype [{0}] isn't supported".format(addrtype))

        args = (
            "python socket for {0}".format(self.GetModule()),
            bindhost,
            ssl,
            maxconns,
            self._csock,
            timeout,
            addr
        )

        if port is None:
            return self.GetModule().GetManager().ListenRand(*args)

        if self.GetModule().GetManager().ListenHost(port, *args):
            return port

        return 0

    def Connect(self, host, port, timeout=60, ssl=False, bindhost=''):
        return self.GetModule().GetManager().Connect(
            host,
            port,
            'python conn socket for {0}'.format(self.GetModule()),
            timeout,
            ssl,
            bindhost,
            self._csock
        )

    def Write(self, data):
        if (isinstance(data, str)):
            return self._csock.Write(data)
        raise TypeError(
            'socket.Write needs str. If you want binary data, use WriteBytes')

    def Init(self, *a, **b):
        pass

    def OnConnected(self):
        pass

    def OnDisconnected(self):
        pass

    def OnTimeout(self):
        pass

    def OnConnectionRefused(self):
        pass

    def OnReadData(self, bytess):
        pass

    def OnReadLine(self, line):
        pass

    def OnAccepted(self, host, port):
        pass

    def OnShutdown(self):
        pass


class Timer:
    def GetModule(self):
        return AsPyModule(self._ctimer.GetModule()).GetNewPyObj()

    def RunJob(self):
        pass

    def OnShutdown(self):
        pass


class ModuleNVIter(collections.Iterator):
    def __init__(self, cmod):
        self._cmod = cmod
        self.it = cmod.BeginNV_()

    def __next__(self):
        if self.it.is_end(self._cmod):
            raise StopIteration
        res = self.it.get()
        self.it.plusplus()
        return res


class ModuleNV(collections.MutableMapping):
    def __init__(self, cmod):
        self._cmod = cmod

    def __setitem__(self, key, value):
        self._cmod.SetNV(key, value)

    def __getitem__(self, key):
        if not self._cmod.ExistsNV(key):
            raise KeyError
        return self._cmod.GetNV(key)

    def __contains__(self, key):
        return self._cmod.ExistsNV(key)

    def __delitem__(self, key):
        self._cmod.DelNV(key)

    def keys(self):
        return ModuleNVIter(self._cmod)
    __iter__ = keys

    def __len__(self):
        raise NotImplemented


class Module:
    description = '< Placeholder for a description >'

    wiki_page = ''

    def __str__(self):
        return self.GetModName()

    def OnLoad(self, sArgs, sMessage):
        return True

    def _GetSubPages(self):
        return self.GetSubPages()

    def CreateSocket(self, socketclass, *the, **rest):
        socket = socketclass()
        socket._csock = CreatePySocket(self._cmod, socket)
        socket.Init(*the, **rest)
        return socket

    def CreateTimer(self, timer, interval=10, cycles=1, label='pytimer',
                    description='Some python timer'):
        t = timer()
        t._ctimer = CreatePyTimer(self._cmod, interval, cycles, label,
                                  description, t)
        return t

    def GetSubPages(self):
        pass

    def OnShutdown(self):
        pass

    def OnBoot(self):
        pass

    def WebRequiresLogin(self):
        pass

    def WebRequiresAdmin(self):
        pass

    def GetWebMenuTitle(self):
        pass

    def OnWebPreRequest(self, WebSock, sPageName):
        pass

    def OnWebRequest(self, WebSock, sPageName, Tmpl):
        pass

    def OnPreRehash(self):
        pass

    def OnPostRehash(self):
        pass

    def OnIRCDisconnected(self):
        pass

    def OnIRCConnected(self):
        pass

    def OnIRCConnecting(self, IRCSock):
        pass

    def OnIRCConnectionError(self, IRCSock):
        pass

    def OnIRCRegistration(self, sPass, sNick, sIdent, sRealName):
        pass

    def OnBroadcast(self, sMessage):
        pass

    def OnChanPermission(self, OpNick, Nick, Channel, uMode, bAdded,
        bNoChange):
        pass

    def OnOp(self, OpNick, Nick, Channel, bNoChange):
        pass

    def OnDeop(self, OpNick, Nick, Channel, bNoChange):
        pass

    def OnVoice(self, OpNick, Nick, Channel, bNoChange):
        pass

    def OnDevoice(self, OpNick, Nick, Channel, bNoChange):
        pass

    def OnMode(self, OpNick, Channel, uMode, sArg, bAdded, bNoChange):
        pass

    def OnRawMode(self, OpNick, Channel, sModes, sArgs):
        pass

    def OnRaw(self, sLine):
        pass

    def OnStatusCommand(self, sCommand):
        pass

    def OnModCommand(self, sCommand):
        pass

    def OnModNotice(self, sMessage):
        pass

    def OnModCTCP(self, sMessage):
        pass

    def OnQuit(self, Nick, sMessage, vChans):
        pass

    def OnNick(self, Nick, sNewNick, vChans):
        pass

    def OnKick(self, OpNick, sKickedNick, Channel, sMessage):
        pass

    def OnJoin(self, Nick, Channel):
        pass

    def OnPart(self, Nick, Channel, sMessage=None):
        pass

    def OnChanBufferStarting(self, Chan, Client):
        pass

    def OnChanBufferEnding(self, Chan, Client):
        pass

    def OnChanBufferPlayLine(self, Chan, Client, sLine):
        pass

    def OnPrivBufferPlayLine(self, Client, sLine):
        pass

    def OnClientLogin(self):
        pass

    def OnClientDisconnect(self):
        pass

    def OnUserRaw(self, sLine):
        pass

    def OnUserCTCPReply(self, sTarget, sMessage):
        pass

    def OnUserCTCP(self, sTarget, sMessage):
        pass

    def OnUserAction(self, sTarget, sMessage):
        pass

    def OnUserMsg(self, sTarget, sMessage):
        pass

    def OnUserNotice(self, sTarget, sMessage):
        pass

    def OnUserJoin(self, sChannel, sKey):
        pass

    def OnUserPart(self, sChannel, sMessage):
        pass

    def OnUserTopic(self, sChannel, sTopic):
        pass

    def OnUserTopicRequest(self, sChannel):
        pass

    def OnCTCPReply(self, Nick, sMessage):
        pass

    def OnPrivCTCP(self, Nick, sMessage):
        pass

    def OnChanCTCP(self, Nick, Channel, sMessage):
        pass

    def OnPrivAction(self, Nick, sMessage):
        pass

    def OnChanAction(self, Nick, Channel, sMessage):
        pass

    def OnPrivMsg(self, Nick, sMessage):
        pass

    def OnChanMsg(self, Nick, Channel, sMessage):
        pass

    def OnPrivNotice(self, Nick, sMessage):
        pass

    def OnChanNotice(self, Nick, Channel, sMessage):
        pass

    def OnTopic(self, Nick, Channel, sTopic):
        pass

    def OnServerCapAvailable(self, sCap):
        pass

    def OnServerCapResult(self, sCap, bSuccess):
        pass

    def OnTimerAutoJoin(self, Channel):
        pass

    def OnEmbeddedWebRequest(self, WebSock, sPageName, Tmpl):
        pass

def make_inherit(cl, parent, attr):
    def make_caller(parent, name, attr):
        return lambda self, *a: parent.__dict__[name](self.__dict__[attr], *a)
    while True:
        for x in parent.__dict__:
            if not x.startswith('_') and x not in cl.__dict__:
                setattr(cl, x, make_caller(parent, x, attr))
        if '_s' in parent.__dict__:
            parent = parent._s
        else:
            break

make_inherit(Socket, CPySocket, '_csock')
make_inherit(Module, CPyModule, '_cmod')
make_inherit(Timer, CPyTimer, '_ctimer')


def find_open(modname):
    '''Returns (pymodule, datapath)'''
    for d in CModules.GetModDirs():
        # d == (libdir, datadir)
        try:
            x = imp.find_module(modname, [d[0]])
        except ImportError:
            # no such file in dir d
            continue
        # x == (<open file './modules/admin.so', mode 'rb' at 0x7fa2dc748d20>,
        #       './modules/admin.so', ('.so', 'rb', 3))
        # x == (<open file './modules/pythontest.py', mode 'U' at
        #       0x7fa2dc748d20>, './modules/pythontest.py', ('.py', 'U', 1))
        if x[0] is None:
            # the same
            continue
        if x[2][0] == '.so':
            try:
                pymodule = imp.load_module(modname, *x)
            except ImportError:
                # found needed .so but can't load it...
                # maybe it's normal (non-python) znc module?
                # another option here could be to "continue"
                # search of python module in other moddirs.
                # but... we respect C++ modules ;)
                return (None, None)
            finally:
                x[0].close()
        else:
            # this is not .so, so it can be only python module .py or .pyc
            try:
                pymodule = imp.load_module(modname, *x)
            finally:
                x[0].close()
        return (pymodule, d[1])
    else:
        # nothing found
        return (None, None)


def load_module(modname, args, user, retmsg, modpython):
    '''Returns 0 if not found, 1 on loading error, 2 on success'''
    if re.search(r'[^a-zA-Z0-9_]', modname) is not None:
        retmsg.s = 'Module names can only contain letters, numbers and ' \
                    'underscores, [{0}] is invalid.'.format(modname)
        return 1
    pymodule, datapath = find_open(modname)
    if pymodule is None:
        return 0
    if modname not in pymodule.__dict__:
        retmsg.s = "Python module [{0}] doesn't have class named [{1}]".format(
            pymodule.__file__, modname)
        return 1
    cl = pymodule.__dict__[modname]
    module = cl()
    module._cmod = CreatePyModule(user, modname, datapath, module, modpython)
    module.nv = ModuleNV(module._cmod)
    module.SetDescription(cl.description)
    module.SetArgs(args)
    module.SetModPath(pymodule.__file__)
    user.GetModules().push_back(module._cmod)
    try:
        loaded = True
        if not module.OnLoad(args, retmsg):
            if retmsg.s == '':
                retmsg.s = 'Module [{0}] aborted.'.format(modname)
            else:
                retmsg.s = 'Module [{0}] aborted: {1}'.format(modname,
                    retmsg.s)
            loaded = False
    except BaseException:
        if retmsg.s == '':
            retmsg.s = 'Got exception: {0}'.format(traceback.format_exc())
        else:
            retmsg.s = '{0}; Got exception: {1}'.format(retmsg.s,
                traceback.format_exc())
        loaded = False
    except:
        if retmsg.s == '':
            retmsg.s = 'Got exception.'
        else:
            retmsg.s = '{0}; Got exception.'.format(retmsg.s)
        loaded = False

    if loaded:
        if retmsg.s == '':
            retmsg.s = "[{0}]".format(pymodule.__file__)
        else:
            retmsg.s = "[{1}] [{0}]".format(pymodule.__file__,
                retmsg.s)
        return 2
    print(retmsg.s)

    unload_module(module)
    return 1


def unload_module(module):
    module.OnShutdown()
    cmod = module._cmod
    del module._cmod
    cmod.GetUser().GetModules().removeModule(cmod)
    cmod.DeletePyModule()
    del cmod


def get_mod_info(modname, retmsg, modinfo):
    '''0-not found, 1-error, 2-success'''
    pymodule, datadir = find_open(modname)
    if pymodule is None:
        return 0
    if modname not in pymodule.__dict__:
        retmsg.s = "Python module [{0}] doesn't have class named [{1}]".format(
            pymodule.__file__, modname)
        return 1
    cl = pymodule.__dict__[modname]
    modinfo.SetGlobal(False)
    modinfo.SetDescription(cl.description)
    modinfo.SetWikiPage(cl.wiki_page)
    modinfo.SetName(modname)
    modinfo.SetPath(pymodule.__file__)
    return 2


def get_mod_info_path(path, modname, modinfo):
    try:
        x = imp.find_module(modname, [path])
    except ImportError:
        return 0
    # x == (<open file './modules/admin.so', mode 'rb' at 0x7fa2dc748d20>,
    #       './modules/admin.so', ('.so', 'rb', 3))
    # x == (<open file './modules/pythontest.py', mode 'U' at 0x7fa2dc748d20>,
    #       './modules/pythontest.py', ('.py', 'U', 1))
    if x[0] is None:
        return 0
    try:
        pymodule = imp.load_module(modname, *x)
    except ImportError:
        return 0
    finally:
        x[0].close()
    if modname not in pymodule.__dict__:
        return 0
    cl = pymodule.__dict__[modname]
    modinfo.SetGlobal(False)
    modinfo.SetDescription(cl.description)
    modinfo.SetWikiPage(cl.wiki_page)
    modinfo.SetName(modname)
    modinfo.SetPath(pymodule.__file__)
    return 1


CONTINUE = CModule.CONTINUE
HALT = CModule.HALT
HALTMODS = CModule.HALTMODS
HALTCORE = CModule.HALTCORE
UNLOAD = CModule.UNLOAD

HaveSSL = HaveSSL_()
HaveCAres = HaveCAres_()
HaveIPv6 = HaveIPv6_()
Version = GetVersion()
VersionMajor = GetVersionMajor()
VersionMinor = GetVersionMinor()
VersionExtra = GetVersionExtra()


def CreateWebSubPage(name, title='', params=dict(), admin=False):
    vpair = VPair()
    for k, v in params.items():
        VPair_Add2Str_(vpair, k, v)
    flags = 0
    if admin:
        flags |= CWebSubPage.F_ADMIN
    return CreateWebSubPage_(name, title, vpair, flags)
