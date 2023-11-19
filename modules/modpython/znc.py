#
# Copyright (C) 2004-2023 ZNC, see the NOTICE file for details.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

_cov = None
import os
if os.environ.get('ZNC_MODPYTHON_COVERAGE'):
    import coverage
    _cov = coverage.Coverage(auto_data=True, branch=True)
    _cov.start()

from functools import wraps
import collections.abc
import importlib.abc
import importlib.machinery
import importlib.util
import re
import sys
import traceback

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


class ModuleNVIter(collections.abc.Iterator):
    def __init__(self, cmod):
        self._cmod = cmod
        self.it = cmod.BeginNV_()

    def __next__(self):
        if self.it.is_end(self._cmod):
            raise StopIteration
        res = self.it.get()
        self.it.plusplus()
        return res


class ModuleNV(collections.abc.MutableMapping):
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
    module_types = [CModInfo.NetworkModule]

    wiki_page = ''

    has_args = False
    args_help_text = ''

    def __str__(self):
        return self.GetModName()

    @classmethod
    def t_s(cls, english, context=''):
        domain = 'znc-' + cls.__name__
        return CTranslation.Get().Singular(domain, context, english)

    @classmethod
    def t_f(cls, english, context=''):
        fmt = cls.t_s(english, context)
        # Returning bound method
        return fmt.format

    @classmethod
    def t_p(cls, english, englishes, num, context=''):
        domain = 'znc-' + cls.__name__
        fmt = CTranslation.Get().Plural(domain, context, english, englishes,
                                        num)
        return fmt.format

    @classmethod
    def t_d(cls, english, context=''):
        return CDelayedTranslation('znc-' + cls.__name__, context, english)

    def OnLoad(self, sArgs, sMessage):
        return True

    def _GetSubPages(self):
        return self.GetSubPages()

    def CreateSocket(self, socketclass=Socket, *the, **rest):
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
        self.HandleCommand(sCommand)

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

    def OnJoining(self, Channel):
        pass

    def OnJoin(self, Nick, Channel):
        pass

    def OnPart(self, Nick, Channel, sMessage=None):
        pass

    def OnInvite(self, Nick, sChan):
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

    def OnUserQuit(self, sMessage):
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

    def OnServerCap302Available(self, sCap, sValue):
        return self.OnServerCapAvailable(sCap)

    def OnServerCapResult(self, sCap, bSuccess):
        pass

    def OnTimerAutoJoin(self, Channel):
        pass

    def OnEmbeddedWebRequest(self, WebSock, sPageName, Tmpl):
        pass

    def OnAddNetwork(self, Network, sErrorRet):
        pass

    def OnDeleteNetwork(self, Network):
        pass

    def OnSendToClient(self, sLine, Client):
        pass

    def OnSendToIRC(self, sLine):
        pass

    # Command stuff
    def AddCommand(self, cls, *args, **kwargs):
        cmd = cls(*args, **kwargs)
        cmd._cmodcommand = CreatePyModCommand(self._cmod, cls.command,
                                              COptionalTranslation(cls.args),
                                              COptionalTranslation(cls.description),
                                              cmd)

        return cmd

    # Global modules
    def OnAddUser(self, User, sErrorRet):
        pass

    def OnDeleteUser(self, User):
        pass

    def OnClientConnect(self, pSock, sHost, uPort):
        pass

    def OnLoginAttempt(self, Auth):
        pass

    def OnFailedLogin(self, sUsername, sRemoteIP):
        pass

    def OnUnknownUserRaw(self, pClient, sLine):
        pass

    def OnClientCapLs(self, pClient, ssCaps):
        pass

    def IsClientCapSupported(self, pClient, sCap, bState):
        pass

    def OnClientCapRequest(self, pClient, sCap, bState):
        pass

    def OnModuleLoading(self, sModName, sArgs, eType, bSuccess, sRetMsg):
        pass

    def OnModuleUnloading(self, pModule, bSuccess, sRetMsg):
        pass

    def OnGetModInfo(self, ModInfo, sModule, bSuccess, sRetMsg):
        pass

    def OnGetAvailableMods(self, ssMods, eType):
        pass

    # In python None is allowed value, so python modules may continue using OnMode and not OnMode2
    def OnChanPermission2(self, OpNick, Nick, Channel, uMode, bAdded, bNoChange):
        return self.OnChanPermission(OpNick, Nick, Channel, uMode, bAdded, bNoChange)

    def OnOp2(self, OpNick, Nick, Channel, bNoChange):
        return self.OnOp(OpNick, Nick, Channel, bNoChange)

    def OnDeop2(self, OpNick, Nick, Channel, bNoChange):
        return self.OnDeop(OpNick, Nick, Channel, bNoChange)

    def OnVoice2(self, OpNick, Nick, Channel, bNoChange):
        return self.OnVoice(OpNick, Nick, Channel, bNoChange)

    def OnDevoice2(self, OpNick, Nick, Channel, bNoChange):
        return self.OnDevoice(OpNick, Nick, Channel, bNoChange)

    def OnMode2(self, OpNick, Channel, uMode, sArg, bAdded, bNoChange):
        return self.OnMode(OpNick, Channel, uMode, sArg, bAdded, bNoChange)

    def OnRawMode2(self, OpNick, Channel, sModes, sArgs):
        return self.OnRawMode(OpNick, Channel, sModes, sArgs)

    def OnRawMessage(self, msg):
        pass

    def OnNumericMessage(self, msg):
        pass

    # Deprecated non-Message functions should still work, for now.
    def OnQuitMessage(self, msg, vChans):
        return self.OnQuit(msg.GetNick(), msg.GetReason(), vChans)

    def OnNickMessage(self, msg, vChans):
        return self.OnNick(msg.GetNick(), msg.GetNewNick(), vChans)

    def OnKickMessage(self, msg):
        return self.OnKick(msg.GetNick(), msg.GetKickedNick(), msg.GetChan(), msg.GetReason())

    def OnJoinMessage(self, msg):
        return self.OnJoin(msg.GetNick(), msg.GetChan())

    def OnPartMessage(self, msg):
        return self.OnPart(msg.GetNick(), msg.GetChan(), msg.GetReason())

    def OnChanBufferPlayMessage(self, msg):
        modified = String()
        old = modified.s = msg.ToString(CMessage.ExcludeTags)
        ret = self.OnChanBufferPlayLine(msg.GetChan(), msg.GetClient(), modified)
        if old != modified.s:
            msg.Parse(modified.s)
        return ret

    def OnPrivBufferPlayMessage(self, msg):
        modified = String()
        old = modified.s = msg.ToString(CMessage.ExcludeTags)
        ret = self.OnPrivBufferPlayLine(msg.GetClient(), modified)
        if old != modified.s:
            msg.Parse(modified.s)
        return ret

    def OnUserRawMessage(self, msg):
        pass

    def OnUserCTCPReplyMessage(self, msg):
        target = String(msg.GetTarget())
        text = String(msg.GetText())
        ret = self.OnUserCTCPReply(target, text)
        msg.SetTarget(target.s)
        msg.SetText(text.s)
        return ret

    def OnUserCTCPMessage(self, msg):
        target = String(msg.GetTarget())
        text = String(msg.GetText())
        ret = self.OnUserCTCP(target, text)
        msg.SetTarget(target.s)
        msg.SetText(text.s)
        return ret

    def OnUserActionMessage(self, msg):
        target = String(msg.GetTarget())
        text = String(msg.GetText())
        ret = self.OnUserAction(target, text)
        msg.SetTarget(target.s)
        msg.SetText(text.s)
        return ret

    def OnUserTextMessage(self, msg):
        target = String(msg.GetTarget())
        text = String(msg.GetText())
        ret = self.OnUserMsg(target, text)
        msg.SetTarget(target.s)
        msg.SetText(text.s)
        return ret

    def OnUserNoticeMessage(self, msg):
        target = String(msg.GetTarget())
        text = String(msg.GetText())
        ret = self.OnUserNotice(target, text)
        msg.SetTarget(target.s)
        msg.SetText(text.s)
        return ret

    def OnUserJoinMessage(self, msg):
        chan = String(msg.GetTarget())
        key = String(msg.GetKey())
        ret = self.OnUserJoin(chan, key)
        msg.SetTarget(chan.s)
        msg.SetKey(key.s)
        return ret

    def OnUserPartMessage(self, msg):
        chan = String(msg.GetTarget())
        reason = String(msg.GetReason())
        ret = self.OnUserPart(chan, reason)
        msg.SetTarget(chan.s)
        msg.SetReason(reason.s)
        return ret

    def OnUserTopicMessage(self, msg):
        chan = String(msg.GetTarget())
        topic = String(msg.GetTopic())
        ret = self.OnUserTopic(chan, topic)
        msg.SetTarget(chan.s)
        msg.SetTopic(topic.s)
        return ret

    def OnUserQuitMessage(self, msg):
        reason = String(msg.GetReason())
        ret = self.OnUserQuit(reason)
        msg.SetReason(reason.s)
        return ret

    def OnCTCPReplyMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnCTCPReply(msg.GetNick(), text)
        msg.SetText(text.s)
        return ret

    def OnPrivCTCPMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnPrivCTCP(msg.GetNick(), text)
        msg.SetText(text.s)
        return ret

    def OnChanCTCPMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnChanCTCP(msg.GetNick(), msg.GetChan(), text)
        msg.SetText(text.s)
        return ret

    def OnPrivActionMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnPrivAction(msg.GetNick(), text)
        msg.SetText(text.s)
        return ret

    def OnChanActionMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnChanAction(msg.GetNick(), msg.GetChan(), text)
        msg.SetText(text.s)
        return ret

    def OnPrivTextMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnPrivMsg(msg.GetNick(), text)
        msg.SetText(text.s)
        return ret

    def OnChanTextMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnChanMsg(msg.GetNick(), msg.GetChan(), text)
        msg.SetText(text.s)
        return ret

    def OnPrivNoticeMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnPrivNotice(msg.GetNick(), text)
        msg.SetText(text.s)
        return ret

    def OnChanNoticeMessage(self, msg):
        text = String(msg.GetText())
        ret = self.OnChanNotice(msg.GetNick(), msg.GetChan(), text)
        msg.SetText(text.s)
        return ret

    def OnTopicMessage(self, msg):
        topic = String(msg.GetTopic())
        ret = self.OnTopic(msg.GetNick(), msg.GetChan(), topic)
        msg.SetTopic(topic.s)
        return ret

    def OnUnknownUserRawMessage(self, msg):
        pass

    def OnSendToClientMessage(self, msg):
        pass

    def OnSendToIRCMessage(self, msg):
        pass


class Command:
    command = ''
    args = ''
    description = ''

    def __call__(self, sLine):
        pass

    def GetModule(self):
        return self._cmodcommand.GetModule().GetNewPyObj()


def make_inherit(cl, parent, attr):
    def make_caller(parent, name, attr):
        return lambda self, *a: parent.__dict__[name](self.__dict__[attr], *a)
    while True:
        for x in parent.__dict__:
            if not x.startswith('_') and x not in cl.__dict__:
                setattr(cl, x, make_caller(parent, x, attr))
        if parent.__bases__:
            # Multiple inheritance is not supported (yet?)
            parent = parent.__bases__[0]
        else:
            break

make_inherit(Socket, CPySocket, '_csock')
make_inherit(Module, CPyModule, '_cmod')
make_inherit(Timer, CPyTimer, '_ctimer')
make_inherit(Command, CPyModCommand, '_cmodcommand')


class ZNCModuleLoader(importlib.abc.SourceLoader):
    def __init__(self, modname, pypath):
        self.pypath = pypath

    def create_module(self, spec):
        self._datadir = spec.loader_state[0]
        self._package_dir = spec.loader_state[1]
        return super().create_module(spec)

    def get_data(self, path):
        with open(path, 'rb') as f:
            return f.read()

    def get_filename(self, fullname):
        return self.pypath


class ZNCModuleFinder(importlib.abc.MetaPathFinder):
    @staticmethod
    def find_spec(fullname, path, target=None):
        if fullname == 'znc_modules':
            spec = importlib.util.spec_from_loader(fullname, None, is_package=True)
            return spec
        parts = fullname.split('.')
        if parts[0] != 'znc_modules':
            return
        def dirs():
            if len(parts) == 2:
                # common case
                yield from CModules.GetModDirs()
            else:
                # the module is a package and tries to load a submodule of it
                for libdir in sys.modules['znc_modules.' + parts[1]].__loader__._package_dir:
                    yield libdir, None
        for libdir, datadir in dirs():
            finder = importlib.machinery.FileFinder(libdir,
                    (ZNCModuleLoader, importlib.machinery.SOURCE_SUFFIXES))
            spec = finder.find_spec('.'.join(parts[1:]))
            if spec:
                spec.name = fullname
                spec.loader_state = (datadir, spec.submodule_search_locations)
                # It almost works with original submodule_search_locations,
                # then python will find submodules of the package itself,
                # without calling out to ZNCModuleFinder or ZNCModuleLoader.
                # But updatemod will be flaky for those submodules because as
                # of py3.8 importlib.invalidate_caches() goes only through
                # sys.meta_path, but not sys.path_hooks. So we make them load
                # through ZNCModuleFinder too, but still remember the original
                # dir so that the whole module comes from a single entry in
                # CModules.GetModDirs().
                spec.submodule_search_locations = []
                return spec


sys.meta_path.append(ZNCModuleFinder())

_py_modules = set()

def find_open(modname):
    '''Returns (pymodule, datapath)'''
    fullname = 'znc_modules.' + modname
    for m in _py_modules:
        if m.GetModName() == modname:
            break
    else:
        # module is not loaded, clean up previous attempts to load it or even
        # to list as available modules
        # This is to to let updatemod work
        to_remove = []
        for m in sys.modules:
            if m == fullname or m.startswith(fullname + '.'):
                to_remove.append(m)
        for m in to_remove:
            del sys.modules[m]
    try:
        module = importlib.import_module(fullname)
    except ImportError:
        return (None, None)
    if not isinstance(module.__loader__, ZNCModuleLoader):
        # If modname/ is a directory, it was "loaded" using _NamespaceLoader.
        # This is the case for e.g. modperl.
        # https://github.com/znc/znc/issues/1757
        return (None, None)
    return (module, os.path.join(module.__loader__._datadir, modname))

def load_module(modname, args, module_type, user, network, retmsg, modpython):
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

    if module_type not in cl.module_types:
        retmsg.s = "Module [{}] doesn't support type.".format(modname)
        return 1

    module = cl()
    module._cmod = CreatePyModule(user, network, modname, datapath, module_type, module, modpython)
    module.nv = ModuleNV(module._cmod)
    module.SetDescription(cl.description)
    module.SetArgs(args)
    module.SetModPath(pymodule.__file__)
    _py_modules.add(module)

    if module_type == CModInfo.UserModule:
        if not user:
            retmsg.s = "Module [{}] is UserModule and needs user.".format(modname)
            unload_module(module)
            return 1
        cont = user
    elif module_type == CModInfo.NetworkModule:
        if not network:
            retmsg.s = "Module [{}] is Network module and needs a network.".format(modname)
            unload_module(module)
            return 1
        cont = network
    elif module_type == CModInfo.GlobalModule:
        cont = CZNC.Get()
    else:
        retmsg.s = "Module [{}] doesn't support that module type.".format(modname)
        unload_module(module)
        return 1

    cont.GetModules().append(module._cmod)

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
    if (module not in _py_modules):
        return False
    module.OnShutdown()
    _py_modules.discard(module)
    cmod = module._cmod
    if module.GetType() == CModInfo.UserModule:
        cont = cmod.GetUser()
    elif module.GetType() == CModInfo.NetworkModule:
        cont = cmod.GetNetwork()
    elif module.GetType() == CModInfo.GlobalModule:
        cont = CZNC.Get()
    cont.GetModules().removeModule(cmod)
    del module._cmod
    cmod.DeletePyModule()
    del cmod
    return True


def unload_all():
    while len(_py_modules) > 0:
        mod = _py_modules.pop()
        # add it back to set, otherwise unload_module will be sad
        _py_modules.add(mod)
        unload_module(mod)
    if _cov:
        _cov.stop()


def gather_mod_info(cl, modinfo):
    translation = CTranslationDomainRefHolder("znc-" + modinfo.GetName())
    modinfo.SetDescription(cl.description)
    modinfo.SetWikiPage(cl.wiki_page)
    modinfo.SetDefaultType(cl.module_types[0])
    modinfo.SetArgsHelpText(cl.args_help_text);
    modinfo.SetHasArgs(cl.has_args);
    for module_type in cl.module_types:
        modinfo.AddType(module_type)


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
    modinfo.SetName(modname)
    modinfo.SetPath(pymodule.__file__)
    gather_mod_info(cl, modinfo)
    return 2


CONTINUE = CModule.CONTINUE
HALT = CModule.HALT
HALTMODS = CModule.HALTMODS
HALTCORE = CModule.HALTCORE
UNLOAD = CModule.UNLOAD

HaveSSL = HaveSSL_()
HaveIPv6 = HaveIPv6_()
HaveCharset = HaveCharset_()
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

CUser.GetNetworks = CUser.GetNetworks_
CIRCNetwork.GetChans = CIRCNetwork.GetChans_
CIRCNetwork.GetServers = CIRCNetwork.GetServers_
CIRCNetwork.GetQueries = CIRCNetwork.GetQueries_
CChan.GetNicks = CChan.GetNicks_
CZNC.GetUserMap = CZNC.GetUserMap_


def FreeOwnership(func):
    """
        Force release of python ownership of user object when adding it to znc

        This solves #462
    """
    @wraps(func)
    def _wrap(self, obj, *args):
        # Bypass if first argument is not an SWIG object (like base type str)
        if not hasattr(obj, 'thisown'):
            return func(self, obj, *args)
        # Change ownership of C++ object from SWIG/python to ZNC core if function was successful
        if func(self, obj, *args):
            # .thisown is magic SWIG's attribute which makes it call C++ "delete" when python's garbage collector deletes python wrapper
            obj.thisown = 0
            return True
        else:
            return False
    return _wrap

CZNC.AddListener = FreeOwnership(func=CZNC.AddListener)
CZNC.AddUser = FreeOwnership(func=CZNC.AddUser)
CZNC.AddNetworkToQueue = FreeOwnership(func=CZNC.AddNetworkToQueue)
CUser.AddNetwork = FreeOwnership(func=CUser.AddNetwork)
CIRCNetwork.AddChan = FreeOwnership(func=CIRCNetwork.AddChan)
CModule.AddSocket = FreeOwnership(func=CModule.AddSocket)
CModule.AddSubPage = FreeOwnership(func=CModule.AddSubPage)


class ModulesIter(collections.abc.Iterator):
    def __init__(self, cmod):
        self._cmod = cmod

    def __next__(self):
        if self._cmod.is_end():
            raise StopIteration

        module = self._cmod.get()
        self._cmod.plusplus()
        return module
CModules.__iter__ = lambda cmod: ModulesIter(CModulesIter(cmod))


# e.g. msg.As(znc.CNumericMessage)
def _CMessage_As(self, cl):
    return getattr(self, 'As_' + cl.__name__, lambda: self)()
CMessage.As = _CMessage_As


def str_eq(self, other):
    if str(other) == str(self):
        return True

    return id(self) == id(other)

CChan.__eq__ = str_eq
CNick.__eq__ = str_eq
CUser.__eq__ = str_eq
CIRCNetwork.__eq__ = str_eq
CPyRetString.__eq__ = str_eq
