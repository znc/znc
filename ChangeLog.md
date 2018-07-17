# ZNC 1.7.1 (2018-07-17)

## Security critical fixes
* CVE-2018-14055: non-admin user could gain admin privileges and shell access by injecting values into znc.conf.
* CVE-2018-14056: path traversal in HTTP handler via ../ in a web skin name.

## Core
* Fix znc-buildmod to not hardcode the compiler used to build ZNC anymore in CMake build
* Fix language selector. Russian and German were both not selectable.
* Fix build without SSL support
* Fix several broken strings
* Stop spamming users about debug mode. This feature was added in 1.7.0, now reverted.

## New
* Add partial Spanish, Indonesian, and Dutch translations

## Modules
* adminlog: Log the error message again (regression of 1.7.0)
* admindebug: New module, which allows admins to turn on/off --debug in runtime
* flooddetach: Fix description of commands
* modperl: Fix memory leak in NV handling
* modperl: Fix functions which return VCString
* modpython: Fix functions which return VCString
* webadmin: Fix fancy CTCP replies editor for Firefox. It was showing the plain version even when JS is enabled

## Internal
* Deprecate one of the overloads of CMessage::GetParams(), rename it to CMessage::GetParamsColon()
* Don't throw from destructor in the integration test
* Fix a warning with integration test / gmake / znc-buildmod interaction.



# ZNC 1.7.0 (2018-05-01)

## New
* Add CMake build. Minimum supported CMake version is 3.1. For now ZNC can be built with either CMake or autoconf. In future autoconf is going to be removed. 
    * Currently `znc-buildmod` requires python if CMake was used; if that's a concern for you, please open a bug.
* Increase minimum GCC version from 4.7 to 4.8. Minimum Clang version stays at 3.2.
* Make ZNC UI translateable to different languages (only with CMake), add partial Russian and German translations.
    * If you want to translate ZNC to your language, please join https://crowdin.com/project/znc-bouncer
* Configs written before ZNC 0.206 can't be read anymore
* Implement IRCv3.2 capabilities `away-notify`, `account-notify`, `extended-join`
* Implement IRCv3.2 capabilities `echo-message`, `cap-notify` on the "client side"
* Update capability names as they are named in IRCv3.2: `znc.in/server-time-iso`→`server-time`, `znc.in/batch`→`batch`. Old names will continue working for a while, then will be removed in some future version.
* Make ZNC request `server-time` from server when available
* Increase accepted line length from 1024 to 2048 to give some space to message tags
* Separate buffer size settings for channels and queries
* Support separate `SSLKeyFile` and `SSLDHParamFile` configuration in addition to existing `SSLCertFile`
* Add "AuthOnlyViaModule" global/user setting
* Added pyeval module
* Added stripcontrols module
* Add new substitutions to ExpandString: `%empty%` and `%network%`.
* Stop defaulting real name to "Got ZNC?"
* Make the user aware that debug mode is enabled.
* Added `ClearAllBuffers` command
* Don't require CSRF token for POSTs if the request uses HTTP Basic auth.
* Set `HttpOnly` and `SameSite=strict` for session cookies
* Add SNI SSL client support
* Add support for CIDR notation in allowed hosts list and in trusted proxy list
* Add network-specific config for cert validation in addition to user-supplied fingerprints: `TrustAllCerts`, defaults to false, and `TrustPKI`, defaults to true.
* Add `/attach` command for symmetry with `/detach`. Unlike `/join` it allows wildcards.
* Timestamp format now supports sub-second precision with `%f`. Used in awaystore, listsockets, log modules and buffer playback when client doesn't support server-time
* Build on macOS using ICU, Python, and OpenSSL from Homebrew, if available
* Remove `--with-openssl=/path` option from ./configure. SSL is still supported and is still configurable

## Fixes
* Revert tables to how they were in ZNC 1.4
* Remove flawed Add/Del/ListBindHost(s). They didn't correctly do what they were intended for, but users often confused them with the SetBindHost option. SetBindHost still works.
* Fix disconnection issues when being behind NAT by decreasing the interval how often PING is sent and making it configurable via a setting to change ping timeout time
* Change default flood rates to match RFC1459, prevent excess flood problems
* Match channel names and hostmasks case-insensitively in autoattach, autocycle, autoop, autovoice, log, watch modules
* Fix crash in shell module which happens if client disconnects at a wrong time
* Decrease CPU usage when joining channels during startup or reconnect, add config write delay setting
* Always send the users name in NOTICE when logging in.
* Don't try to quit multiple times
* Don't send PART to client which sent QUIT
* Send failed logins to NOTICE instead of PRIVMSG
* Stop creating files with odd permissions on Solaris
* Save channel key on JOIN even if user was not on the channel yet
* Stop buffering and echoing CTCP requests and responses to other clients with self-message, except for /me
* Support discovery of tcl 8.6 during `./configure`

## Modules
* adminlog:
    * Make path configurable
* alias:
    * Add `Dump` command to copy your config between users
* awaystore:
    * Add `-chans` option which records channel highlights
* blockmotd:
    * Add `GetMotd` command
* clearbufferonmsg:
    * Add options which events trigger clearation of buffers.
* controlpanel:
    * Add the `DelServer` command.
    * Add `$user` and `$network` aliases for `$me` and `$net` respectively
    * Allow reseting channel-specific `AutoClearChanBuffer` and `BufferSize` settings by setting them to `-`
    * Change type of values from "double" to "number", which is more obvious for non-programmers
* crypt:
    * Fix build with LibreSSL
    * Cover notices, actions and topics
    * Don't use the same or overlapping NickPrefix as StatusPrefix
    * Add DH1080 key exchange
    * Add Get/SetNickPrefix commands, hide the internal keyword from ListKeys
* cyrusauth:
    * Improve UI
* fail2ban:
    * Make timeout and attempts configurable, add BAN, UNBAN and LIST commands
* flooddetach:
    * Detach on nick floods
* keepnick:
    * Improve behaviour by listening to ircd-side numeric errors
* log:
    * Add `-timestamp` option
    * Add options to hide joins, quits and nick changes.
    * Stop forcing username and network name to be lower case in filenames
    * Log user quit messages
* missingmotd:
    * Include nick in IRC numeric 422 command, reduce client confusion
* modperl:
    * Provide `operator ""` for `ZNC::String`
    * Honor `PERL5LIB` env var
    * Fix functions like `HasPerm()` which accept `char`
    * When a broken module couldn't be loaded, it couldn't be loaded anymore even if it was fixed later.
    * Force strings to UTF-8 in modperl to fix double encoding during concatenation/interpolation.
* modpython:
    * Require ZNC to be built with encodings support
    * Disable legacy encoding mode when modpython is loaded.
    * Support `CQuery` and `CServer`
* nickserv:
    * Use `/nickserv identify` by default instead of `/msg nickserv`.
    * Support messages from X3 services
* notify_connect:
    * Show client identification
* sasl:
    * Add web interface
    * Enable all known mechanisms by default
    * Make the first requirement for SET actually mandatory, return information about settings if no input for SET
* schat:
    * Require explicit path to certificate.
* simple_away:
    * Use ExpandString for away reason, rename old `%s` to `%awaytime%`
    * Add `MinClients` option
* stickychan:
    * Save registry on every stick/unstick action, auto-save if channel key changes
    * Stop checking so often, increase delay to once every 3 minutes
* webadmin:
    * Make server editor and CTCP replies editor more fancy, when JS is enabled
    * Make tables sortable. 
    * Allow reseting chan buffer size by entering an empty value
    * Show per-network traffic info
    * Make the traffic info page visible for non-admins, non-admins can see only their traffic

## Internal
* Stop pretending that ZNC ABI is stable, when it's not. Make module version checks more strict and prevent crashes when loading a module which are built for the wrong ZNC version.
* Add an integration test
* Various HTML changes
* Introduce a CMessage class and its subclasses
* Add module callbacks which accept CMessage, deprecate old callbacks
* Add `OnNumericMessage` module callback, which previously was possible only with `OnRaw`, which could give unexpected results if the message has IRCv3.2 tags.
* Modernize code to use more C++11 features
* Various code cleanups
* Fix CSS of `_default_` skin for Fingerprints section
* Add `OnUserQuitMessage()` module hook.
* Add `OnPrivBufferStarting()` and `OnPrivBufferEnding()` hooks
* `CString::WildCmp()`: add an optional case-sensitivity argument
* Do not call `OnAddUser()` hook during ZNC startup
* Allow modules to override CSRF protection.
* Rehash now reloads only global settings
* Remove `CAP CLEAR`
* Add `CChan::GetNetwork()`
* `CUser`: add API for removing and clearing allowed hosts
* `CZNC`: add missing SSL-related getters and setters
* Add a possibility (not an "option") to disable launch after --makeconf
* Move Unix signal processing to a dedicated thread.
* Add clang-format configuration, switch tabs to spaces.
* `CString::StripControls()`: Strip background colors when we reset foreground
* Make chan modes and permissions to be char instead of unsigned char.

## Cosmetic
* Alphabetically sort the modules we compile using autoconf/Makefile
* Alphabetically sort output of `znc --help`
* Change output during startup to be more compact
* Show new server name when reconnecting to a different server with `/znc jump`
* Hide passwords in listservers output
* Filter out ZNC passwords in output of `znc -D`
* Switch znc.in URLs to https



# ZNC 1.6.6 (2018-03-05)

* Fix use-after-free in `znc --makepem`. It was broken for a long time, but
  started segfaulting only now. This is a useability fix, not a security fix,
  because self-signed (or signed by a CA) certificates can be created
  without using `--makepem`, and then combined into znc.pem.
* Fix build on Cygwin.



# ZNC 1.6.5 (2017-03-12)

## Fixes

* Fixed a regression of 1.6.4 which caused a crash in modperl/modpython.
* Fixed the behavior of `verbose` command in the sasl module.



# ZNC 1.6.4 (2016-12-10)

## Fixes

* Fixed build with OpenSSL 1.1.
* Fixed build on Cygwin.
* Fixed a segfault after cloning a user. The bug was introduced in ZNC 1.6.0.
* Fixed a segfault when deleting a user or network which is waiting for DNS
  during connection. The bug was introduced in ZNC 1.0.
* Fixed a segfault which could be triggered using alias module.
* Fixed an error in controlpanel module when setting the bindhost of another user.
* Fixed route_replies to not cause client to disconnect by timeout.
* Fixed compatibility with the Gitter IRC bridge.

## Internal
* Fixed `OnInvite` for modpython and modperl.
* Fixed external location of GoogleTest for `make test`.



# ZNC 1.6.3 (2016-02-23)

## Core
* New character encoding is now applied immediately, without reconnect.
* Fixed build with LibreSSL.
* Fixed error 404 when accessing the web UI with the configured URI prefix,
  but without the `/` in the end.
* `znc-buildmod` now exits with non-zero exit code when the .cpp file is not found.
* Fixed `znc-buildmod` on Cygwin.
* ExpandString got expanded. It now expands `%znc%` to
  `ZNC <version> - http://znc.in`, honoring the global "Hide version" setting.
* Default quit message is switched from `ZNC <version> - http://znc.in` to `%znc%`,
  which is the same, but "automatically" changes the shown version when ZNC gets upgraded.
  Before, the old version was recorded in the user's quit message, and stayed the same
  regardless of the current version of ZNC.

## Modules
* modperl:
    * Fixed a memory leak.
* sasl:
    * Added an option to show which mechanisms failed or succeeded.
* webadmin:
    * Fixed an error message on invalid user settings to say what exactly was invalid.
    * No more autocomplete password in user settings. It led to an error when ZNC
      thought the user is going to change a password, but the passwords didn't match.



# ZNC 1.6.2 (2015-11-15)

## Fixes

* Fixed a use-after-delete in webadmin. It was already partially fixed in ZNC 1.4; since 1.4 it has been still possible to trigger, but much harder.
* Fixed a startup failure when awaynick and simple_away were both loaded, and simple_away had arguments.
* Fixed a build failure when using an ancient OpenSSL version.
* Fixed a build failure when using OpenSSL which was built without SSLv3 support.
* Bindhost was sometimes used as ident.
* `CAP :END` wasn't parsed correctly, causing timeout during login for some clients.
* Fixed channel keys if client joined several channels in single command.
* Fixed memory leak when reading an invalid config.

## Modules

* autovoice:
    * Check for autovoices when we are opped.
* controlpanel:
    * Fixed `DelCTCPReply` case-insensitivity.
* dcc:
    * Add missing return statement. It was harmless.
* modpython:
    * Fixed a memory leak.
* modules_online:
    * Wrong ident was used before.
* stickychan:
    * Fixed to unstick inaccessible channels to avoid infinite join loops.

## Internal

* Fixed the nick passed to `CModule::OnChanMsg()` so it has channel permissions set.
* Fixed noisy `-Winconsistent-missing-override` compilation warnings.
* Initialized some fields in constructors of modules before `OnLoad()`.

## Cosmetic

* Various modules had commands with empty descriptions.
* perform:
    * Say "number" instead of "nr".
* route_replies:
    * Make the timeout error message more clear.



# ZNC 1.6.1 (2015-08-04)

## Fixes

* Fixed the problem that channels were no longer removed from the config despite of chansaver being loaded.
* Fixed query buffer size for users who have the default channel buffer size set to 0.
* Fixed a startup failure when simple_away was loaded after awaynick.
* Fixed channel matching commands, such as DETACH, to be case insensitive.
* Specified the required compiler versions in the configure script.
* Fixed a rare conflict of HTTP-Basic auth and cookies.
* Hid local IP address from the 404 page.
* Fixed a build failure for users who have `-Werror=missing-declarations` in their `CXXFLAGS`.
* Fixed `CXXFLAGS=-DVERSION_EXTRA="foo"` which is used by some distros to package ZNC.
* Fixed `znc-buildmod` on Cygwin.

## Modules

* chansaver:
    * Fixed random loading behavior due to an uninitialized member variable.
* modpython:
    * Fixed access to `CUser::GetUserClients()` and `CUser::GetAllClients()`.
* sasl:
    * Improved help texts for the SET and REQUIREAUTH commands.
* savebuff:
    * Fixed periodical writes on the disk when the module is loaded after startup.
* webadmin:
    * Fixed module checkboxes not to claim that all networks/users have loaded a module when there are no networks/users.
    * Added an explanation that ZNC was built without ICU support, when encoding settings are disabled for that reason.
    * Improved the breadcrumbs.
    * Mentioned ExpandString in CTCP replies.
    * Added an explanation how to delete port which is used to access webadmin.

## Internal

* Fixed `CThreadPool` destructor to handle spurious wakeups.
* Fixed `make distclean` to remove `zncconfig.h`.
* Improved the error message about `--datadir`.
* Fixed a compilation warning when `HAVE_LIBSSL` is not defined.
* Fixed 'comparision' typos in CString documentation.
* Added a non-minified version of the jQuery source code to make Linux distributions (Debian) happy, even though the jQuery license does not require this.



# ZNC 1.6.0 (2015-02-12)

## New

* Switch versioning scheme to <major>.<minor>.<patch>.
* Add settings for which SSL/TLS protocols to use (SSLProtocols), which ciphers to enable (SSLCiphers). By default TLSv1+ are enabled, SSLv2/3 are disabled. Default ciphers are what Mozilla advices: https://wiki.mozilla.org/Security/Server_Side_TLS#Intermediate_compatibility_.28default.29
* Validate SSL certificates.
* Allow clients to specify an ID as part of username (user[@identifier][/network]). Currently not used, but modules can use it.
* Add alias module for ZNC-side command interception and processing.
* Support character encodings with separate settings for networks, and for clients. It replaces older charset module, which didn't work well with webadmin, log and other modules.
* Support X-Forwarded-For HTTP header, used with new TrustedProxy setting.
* Add URIPrefix option for HTTP listeners, used with reverse proxy.
* Store query buffers per query the same way it's done for channels, add new option AutoClearQueryBuffer.
* Add DisableChan command to *status, it was available only in webadmin before.
* Allow wildcards in arguments of Help commands of *status and various modules.
* Support IRCv3.2 batches, used for buffer playbacks.
* Support IRCv3.2 self-message.
* Remove awaynick module. It's considered bad etiquette.
* Add JoinDelay setting, which allows a delay between connection to server, and joining first channel. By default it joins immediately after connect.
* Make Detach, EnableChan and DisableChan commands of *status accept multiple channels.
* znc-buildmod: Build output to the current working directory.
* Wrap long lines in tables (e.g. in Help or ListAvailMods commands).
* Support ECDHE if available in OpenSSL.
* Report ZNC version more consistently, add HideVersion setting, which hides ZNC version from public.
* Bump compiler requirements to support C++11. This means GCC 4.7+, Clang 3.2+, SWIG 3.0.0+.


## Fixes

* Disable TLS compression.
* Disallow setting ConnectDelay to zero, don't hammer server with our failed connects.
* Simplify --makeconf.
* Fix logic to find an available nick when connecting to server.
* Fix handling of CTCP flood.
* Allow network specific quit messages.
* Make various text labels gender-neutral.
* Fix finding SWIG 3 on FreeBSD.
* Handle multi-receiver NOTICE and PRIVMSG.
* Make channels follow user-level settings when appropriate.
* Write disabled status to config for disabled channels.
* Fix double output in messages from modules.
* Fix memory leak in gzip compression in HTTP server.
* Use random DNS result instead of choosing the same one every time.
* Fix HTTP basic auth.
* Mention network in message shown if client didn't send PASS.


## Modules

* autoattach:
    * Make it also a network module.
* autoreply:
    * Use NOTICE instead of PRIVMSG.
* autoop:
    * Add support for multiple hostmasks per user.
* awaystore:
    * Store CTCP ACTIONs too.
    * Reset timer and return from away when a client does a CTCP ACTION.
    * Allows use of strftime formatting in away messages.
* bouncedcc:
    * Fix quotes in file names.
    * Fix check for "Connected" state.
* buffextras:
    * Make it also a network module.
* chansaver:
    * Fix saving channel keys.
    * Add support for loading as a global module.
* controlpanel:
    * Add AddChan, DelChan commands, useful for admins to edit other users' channels, was available only in webadmin before.
    * Check if adding a new channel succeeded.
    * Revise Help output.
    * Allow wildcards for GetChan and SetChan.
* flooddetach:
    * Show current value in Lines and Secs commands.
    * Add Silent [yes|no] command, similar to route_replies.
* listsockets:
    * Show traffic stats.
* log:
    * Use only lower case characters in log filenames.
    * Use directories and YYYY-MM-DD filename by default.
    * Add support for logging rules. E.g. /msg *log setrules #znc !#*
* modperl:
    * Fix some int_t types.
* modpython:
    * Fix calling overloaded methods with parameter CString&.
    * Support CZNC::GetUserMap().
    * Set has_args and args_help_text from module.
    * Release python/swig ownership when adding object created in python to ZNC container.
    * Fix some int_t types.
    * Enable default arguments feature of SWIG 3.0.4. No functionality change, it just makes generated code a bit more beautiful.
* nickserv:
    * Support tddirc.net.
    * Remove commands Ghost, Recover, Release, Group. The same functionality is available via new alias module.
* q:
    * Add JoinOnInvite, JoinAfterCloaked options.
    * Don't cloak host on first module load if already connected to IRC.
    * Add web configuration.
    * Use HMAC-SHA-256 instead of HMAC-MD5.
* route_replies:
    * Handle numerics 307 and 379 in /whois reply. Handle IRCv3.2 METADATA numerics.
* sample:
    * Make it a network module, which are easier to write.
* sasl:
    * Remove DH-BLOWFISH and DH-AES. See http://nullroute.eu.org/~grawity/irc-sasl-dh.html and http://kaniini.dereferenced.org/2014/12/26/do-not-use-DH-AES-or-DH-BLOWFISH.html for details.
* savebuff:
    * Do not skip channels with AutoClearChanBuffer=true.
    * Handle empty password in SetPass the same way as during startup.
* simple_away:
    * Apply auto-away on load if no user is connected.
* stickychan:
    * Don't join channels when not connected.
* watch:
    * Add support for detached-only clients, and detached-only channels.
* webadmin:
    * Combine "List Users" and "Add User".
    * Module argument autocomplete="off", for nickserv module, which contains password in argument before first save.
    * For every module show in which other levels that module is loaded (global/user/network).
    * Open links to wiki pages about modules in separate window/tab.
    * Support renaming a network (it was already possible outside of webadmin, via /znc MoveNetwork). However, it doesn't support moving networks between users yet, for that use /znc command.
    * Add missing page title on Traffic page.
    * Improve navigation: "Save and continue".
    * Clarify that timestamp format is useless with server-time.


## Internal

* Move Csocket to git submodule.
* Unit tests, via GTest.
* Allow lambdas for module command callbacks.
* New modules hooks: OnSendToClient, OnSendToIRC, OnJoining, OnMode2, OnChanBufferPlayLine2, OnPrivBufferPlayLine2.
* Add methods to CString: StartsWith, EndsWith, Join, Find, Contains, and Convert.
* Add limited support for using threads in modules: CModuleJob class.
* Inherit CClient and CIRCSock from a common class CIRCSocket.
* Add CZNC::CreateInstance to make porting ZNC to MSVC a bit easier.
* Add CUtils::Get/SetMessageTags().
* Add CIRCNetwork::FindChans().
* Add CChan::SendBuffer(client, buffer) overload.
* Add CIRCNetwork::LoadModule() helper.
* Add CClient::IsPlaybackActive().
* Web: Discard sessions in LRU order.
* Introduce CaseSensitivity enum class.
* Fix CNick::Parse().
* Remove redundant CWebSocket::GetModule().
* Switch from CSmartPtr to std::shared_ptr.
* Fix GetClients() const correctness.
* Make self-signed cert with SHA-256, provide DH parameters in --makepem.
* Use override keyword.
* Show username of every http request in -D output.
* Split CUserTimer into CIRCNetworkPingTimer and CIRCNetworkJoinTimer.
* Give a reason for disabled features during ./configure, where it makes sense.
* Use make-tarball.sh for nightlies too.
* Revise CChan::JoinUser() & AttachUser().
* Modules: use public API.
* Modules: use AddCommand().
* Add ChangeLog.md.



# ZNC 1.4 (2014-05-08)

This release is done to fix a denial of service attack through webadmin. After authentication, users can crash ZNC through a use-after-delete.
Additionally, a number of fixes and nice, low-risk additions from our development branch is included.

In detail, these are:


## New

* Reduce users' confusion during --makeconf.
* Warn people that making ZNC listen on port 6667 might cause problems with some web browsers.
* Always generate a SSL certificate during --makeconf.
* Stop asking for a bind host / listen host in --makeconf. People who don't want wildcard binds can configure this later.
* Don't create ~/.znc/modules if it doesn't exist yet.


## Fixes

* Fix a use-after-delete in webadmin. CVE-2014-9403
* Honor the BindHost whitelist when configuring BindHosts in controlpanel module.
* Ignore trailing whitespace in /znc jump arguments.
* Change formatting of startup messages so that we never overwrite part of a message when printing the result of an action.
* Fix configure on non-bash shells.
* Send the correct error for invalid CAP subcommands.
* Make sure znc-buildmod includes zncconfig.h at the beginning of module code.


## Modules

* Make awaystore automatically call the Ping command when the Back command is used.
* Add SSL information and port number to servers in network list in webadmin.
* Disable password autocompletion when editing users in webadmin.
* Make nickserv  module work on StarChat.net and ircline.org.
* Remove accidental timeout for run commands in shell module.
* certauth now uses a case insensitive comparison on hexadecimal fingerprints.

### controlpanel

* Correct double output.
* Add support for the MaxNetworks global setting.
* Add support for the BindHost per-network setting.

### modperl and modpython

* Make OnAddNetwork and OnDeleteNetwork module hooks work.
* Don't create .pyc files during compilation.
* Fix modperl on MacOS X. Twice.
* Require at least SWIG 2.0.12 on MacOS X.


## Internal

* Don't redefine _FORTIFY_SOURCE if compiler already defines it.
* Cache list of available timezones instead of re-reading it whenever it is needed.
* Improve const-correctness.
* Fix various low-priority compiler warnings.
* Change in-memory storage format for ServerThrottle.
* Use native API on Win32 to replace a file with another file.
* Add src/version.cpp to .gitignore.



# ZNC 1.2 (2013-11-04)

## New

* ZNC has been relicensed to Apache 2.0
* Show password block in --makepass in new format
* Return MaxJoins setting, it helps against server sending ZNC too many lines
  at once and disconnecting with "Max SendQ exceeded"
* Make /znc detach case insensitive, allow "/detach #chan1,#chan2" syntax
* No longer store 381 in the buffer


## Fixes

* CModule::OnMode(): Fix a stupid NULL pointer dereference
* Fix NULL pointer dereference in webadmin.
* Fix a crash when you delete a user with more than one attached client
* Fix a random crash with module hooks
* Revert "Rewrite the JOIN channel logic, dropping MaxJoins"
* Fix build on some systems
* Fix build of shallow git clone
* Fix build of git tags
* Fix OOT builds with swig files in source dir
* Don't send NAMES and TOPIC for detached channels when a client connects
* Fix memory leak
* Consistency between Del* and Rem* in command names
* Fix changing client nick when client connects.
* Timezone GMT+N is really GMT+N now. It behaved like -N before.
* Escape special characters in debug output (znc --debug)
* Don't disconnect networkless users without PINGing them first.
* Don't lose dlerror() message.
* Fix use-after-free which may happen during shutdown
* Fix "Error: Success" message in SSL
* Fixed double forward slashes and incorrect active module highlighting.
* make clean: Only delete files that can be regenerated
* Don't make backup of znc.conf readable by everyone.
* makepem: create pem only rw for the user, on non-win32
* Don't ever try to overwrite /usr/bin/git
* Fix user modes
* Request secure cookie transmission for HTTPS
* "make clean" removes .depend/
* Fix support for /msg @#chan :hi
* Fix saving config on some cygwin installations
* Fix error message for invalid network name


## Modules

* Return old fakeonline module (accidentally removed in 1.0) as modules_online
* autoattach: add string searching
* autocycle: Convert to a network module
* chansaver: Fix chansaver to not rewrite the config each time a user joins a
  channel on startup
* cert: Make default type of cert mod to be network.
* watch: Don't handle multiple matching patterns for each target
* route_replies: Add some WHOIS numerics
* block_motd: Allow block_motd to be loaded per-network and globally
* notify_connect: Fixed syntax on attach/detach messages to be more consistent
* cyrusauth: Fix user creation

### controlpanel

* Support network module manipulation
* Increases general verbosity of command results.
* Fix bug for "Disconnect" help
* Standardize error wordings

### webadmin

* Allow loading webadmin as user module.
* Show instructions on how to use networks in Add Network too
* clarify that + is SSL
* Show example timezone in webadmin
* Enable embedding network modules.
* Enable embedding modules to network pages.
* Change save network to show the network and not redirect user

### sasl

* Implement DH-AES encrypted password scheme.
* Add missing length check
* Description line for DH-BLOWFISH
* Fixing unaligned accesses

### awaystore

* Fix loading old configs which referred to "away" module
* Fix displaying IPv6 addresses

### crypt

* Add time stamp to buffered messages
* Use ASCII for nick prefix and make it configurable

### nickserv

* Make NickServ nickname configurable.
* Add support for NickServ on wenet.ru and Azzurra
* nickserv: don't confuse people so much

### log

* Add -sanitize option to log module.
* Convert / and \ character to - in nicks for filenames.
* Create files with the same permissions as the whole log directory.

### charset

* Don't try to build charset module if iconv is not found
* Fix: Converted raw string include NULL character in charset module

### modperl

* A bit more debug output on modperl
* Fix perl modules being shown incorrectly in the webadmin

### partyline

* Fix PartyLine so that forced channels may not be left at all - users will be
rejoined at once.
* Fix partyline rejoin on user deletion


## Internal

* Require SWIG 2.0.8 for modperl/modpython (removes hacks to make older SWIG
  work)
* Web interface now supports gzip compression
* Update server-time to new specs with ISO 8601
* Add a generic threads abstraction
* Add CString::StripControls to strip controls (Colors, C0) from strings
* Change PutModule to handle multiple lines
* Debug output: Only print queued lines if they are really just queued
* Add initial unit tests, runnable by "make test"
* Add nick comparison function CNick::NickEquals
* Force including zncconfig.h at the beginning of every .cpp
* Add OnAddNetwork, OnDeleteNetwork module hooks

# ZNC 1.0 (2012-11-07)

## The Big News
Multiple networks per user
Think about new users as "user groups", while new networks are similar to old users.

To login to ZNC, use user/network:password as password, or user/network as username. Also, you can switch between different networks on the fly using the /znc JumpNetwork command.

When you first run ZNC 1.0, it will automatically convert your config and create a network called "default" for each user. Settings from each user are moved into these "default" networks. When you log into ZNC without setting a network, the "default" network will automatically be chosen for you.

Users can create new networks up to an admin-configurable limit. By default, this limit is one network per user.

Existing user-per-network setups can be migrated to the new multinetwork setup using the /znc MoveNetwork command.

You can see a list of networks via /znc ListNetworks and /znc ListAllUserNetworks.

## Timezones
Timezone can now be configured by name, e.g. "GMT-9", or "Europe/Madrid". Old TimezoneOffset setting (which was the number of hours between the server's timezone and the user's timezone) is deprecated and should not be used anymore. Its old value is lost. The reason for this change is that the old TimezoneOffset was not trivial to count and often broke during switches to/from daylight savings time.

So if you previously used the TimezoneOffset option, you now have to configure your timezone again (via the webadmin or controlpanel module).

## No more ZNC-Extra
Most modules from ZNC-Extra are now enabled in the usual installation. It was pointless to have them shipped in the tarball, but requiring user to add some weird flags to ./configure.

Antiidle, fakeonline and motdfile modules are dropped.

Away module is renamed to awaystore to better explain its meaning.

## Fixes
* Don't try IPv6 servers when IPv6 isn't available. Use threads for non-blocking DNS instead of c-ares.
* Fix debug output of identfile.
* Don't forward WHO replies with multi-prefix to clients which don't support multi-prefix
* Send nick changes to clients before we call the OnNick module hook
* Don't connect to SSLed IRC servers when ZNC is compiled without SSL support
* Fix check for visibility support in the compiler
* Fix compilation on cygwin again, including modperl and modpython
* Support parting several channels at once
* Fix a crash in admin (now controlpanel) module
* Fix webadmin to deny setting a bindhost that is not on the global list of allowed bindhosts.
* Fix using empty value for defaults in user page in webadmin.

## Minor Stuff
* Rename admin module to controlpanel to make it clearer that it's not the same as admin flag of a user.
* Add protection from flood. If you send multiple lines at once, they will be slowed down, so that the server will not disconnect ZNC due to flood. It can be configured and can be completely turned off. Default settings are: 1 line per second, first 4 lines are sent at once.
* Modules can support several types now: a module can be loaded as a user module, as a network module and as a global module, if the module supports these types.
* Rename (non-)KeepBuffer to AutoClearChanBuffer
* Process starttls numeric
* Improvements to modperl, modpython, modtcl.
* Add timestamps to znc --debug
* Listeners editor in webadmin
* Add sasl module which uses SASL to authenticate to NickServ.
* Rename saslauth to cyrusauth, to make it clearer that it's not needed to do SASL authentication to NickServ.
* Modules get a way to describe their arguments.
* webadmin: allow editing of the bindhost without global list.
* Don't send our password required notice until after CAP negotiation
* Rewrite the JOIN channel logic, dropping MaxJoins
* Support messages directed to specific user prefixes (like /msg @#channel Hello)
* Show link to http://znc.in/ from web as a link. It was plain text before.
* Webadmin: use HTML5 numeric inputs for numbers.
* Add SSL/IPv6/DNS info to znc --version
* Clarify that only admins can load the shell module.
* cyrusauth: Allow creating new users on first login
* Clear channel buffers when keep buffer is disabled if we're online
* send_raw: Add a command to send a line to the current client
* webadmin: Implement clone user
* autoreply: Honor RFC 2812.
* Add 381 to the buffer ("You are now an IRC Operator")
* identfile: Pause the connection queue while we have a locked file
* Add ShowBindHost command
* autoop: Check for autoops when we get op status
* Improvements and fixes to the partyline module
* partyline Drop support for fixed channels
* Check that there're modules available on startup. Check if ZNC is installed or not.
* Modified description field for bouncedcc module to explain what the module actually does.
* nickserv: add support for nickserv requests on wenet.ru and rusnet.
* send 422 event if MOTD buffer is empty
* route_replies: Handle much more replies
* Clear text colors before appending timestamps to buffer lines, add space before AppendTimestamp for colorless lines.
* Don't replace our motd with a different servers motd
* webadmin: Add a "Disabled" checkbox for channels
* Send a 464 ERR_PASSWDMISMATCH to clients that did not supply a password
* Separate compilation and linking for modules.
* Trim spaces from end of commands to autoattach.
* nickserv: add ghost, recover and release
* Warn if config was saved in a newer ZNC version.
* Backup znc.conf when upgrading ZNC.

## Internal Stuff
* #include <znc/...h> instead of #include "...h"
* Add string formatting function with named params.
* Python, perl: support global, user, network modules.
* Csock: able use non-int number of secs for timer.
* CString("off").ToBool() shouldn't be true
* Python: Override __eq__ to allow comparison of strings
* python: Allow iterating over CModules
* Add methods to CModule to get the web path
* Rework modperl to better integrate with perl.
* Store all 005 values in a map.
* Python: Use znc.Socket if no socket class is specified in CreateSocket()
* CZNC::WriteConfig(): Better --debug output
* Slight refactor of CBuffer & CBufLine.
* Implemented an OnInvite hook
* Allow a client to become "away"
* Create a connection queue
* Set default TrimPrefix to ":"
* Add a config writer
* Wrap MODULECALL macros in a do-while
* Don't require CTimer's label to be unique if its empty
* Allow loading python modules with modpython (ex. modname/__init__.py)
* bNoChange in On{,De}{Op,Voice} wast incorrect
* Drop znc-config, change znc-buildmod so it doesn't need znc-config

# ZNC 0.206 (2012-04-05)

## Fixes
* Identfile: don't crash when ZNC is shutting down.
* CTCPReplies setting with empty value now blocks those CTCP requests to the client.
* Show more sane error messages instead of "Error: Success".
* Imapauth: Follow RFC more closely.
* "No" is a false value too.

## Minor stuff
* Add Show command to identfile, which should help you understand what's going on, if identfile is blocking every connection attempt for some reason.
* Make TLS certs valid for 10 years.
* Ask for port > 1024 in --makeconf.
* Reset JoinTries counter when we enable a channel.

# ZNC 0.204 (2012-01-22)

This release fixes CVE-2012-0033,
http://www.openwall.com/lists/oss-security/2012/01/08/2
https://bugs.gentoo.org/show_bug.cgi?id=CVE-2012-0033
https://bugzilla.redhat.com/show_bug.cgi?id=CVE-2012-0033

## Fixes
* Fix a crash in bouncedcc module with DCC RESUME.
* Fix modperl compilation.
* Don't use mkdir during install.
* Fix compilation failures, which happened sometimes when an older ZNC was already installed.
* Check for the swig2.0 binary too, instead of only swig.

## Minor stuff
* Unload modules in reverse order.
* Don't send server redirects (numeric 010) to clients.
* Make it possible to filter the result of the help command.
* Drop @DEFS@ from the build system so that we don't force HAVE_CONFIG_H upon others.
* Move autocycle to extra.
* Handle raw 482 in route_replies.
* Improve identfile's debug messages.
* Send a MODE request when JOINing.
* Block raw 301 in antiidle.

# ZNC 0.202 (2011-09-21)

This is a bugfix-mostly release.

## Fixes
* Fix a crash when a user changes the buffer size of a channel.
* Fix a NULL pointer dereference in buffer-related module hooks.
* Fix the autocycle module to not fight with ChanServ.
* Fix the getchan command in the admin module.
* Don't timeout bouncedcc connections so that idling DCC chats are possible.
* Fix build error when compiling against uclibc(++).

## Minor stuff
* Improve the timeout message in the route_replies module.
* Add the -r parameter to the man page of ZNC.
* Install .py files along with .pyc.

# ZNC 0.200 (2011-08-20)

## The Big News
* Move ident spoofing from ZNC core into new identfile module.
* Move dcc handling from ZNC core into new modules bouncedcc and dcc.
* Remove the obsolete fixfreenode module.
* New module: cert
* Move away into ZNC-Extra.

## Fixes
* In ZNC 0.098 there was a memleak whenever someone JOINs a channel.
* Compile even when OpenSSL was built with no-ssl2.
* Correctly handle excessive web sessions.
* Correctly save non-ASCII characters to the NV.
* Fix znc-buildmod when ZNC was compiled out of tree.
* Don't always use IPv6 when verifying the listener in --makeconf.

## Minor Things
* Remove a pointless MODE request which ZNC sent on every JOIN.
* Raise ZNC's timeouts.
* Log's logging path becomes configurable.
* Add a replay command to away.
* Add a get command to notes.
* Add -disableNotesOnLogin argument to notes.
* Add hostmask handling to autoattach.
* Make it possible for modules to provide additional info, e.g. providing a homepage URL.
* Various improvements to modpython.
* Hardcode a default entry for the CN in znc --makepem.
* Work around Mac OS' and Solaris' brokenness.
* Make ZNC compile without getopt_long(). This fixes compilation on e.g. Solaris 9 and hopefully Irix.
* Check for errors like "no space left on disk" while writing the config file.
* Improve the error handling when reading the config.
* Move module data files to own directory in the source and in installation prefix.
* Handle Listeners after SSLCertFile during startup.
* Check for required SWIG version in ./configure.
* Make it possible to use ExpandString-stuff in QuitMsg.
* znc-buildmod: Print ZNC's version number.
* Add config option ProtectWebSessions which makes it possible to disable the IP check for web sessions.

## Internal Stuff
* Build modules with hidden symbol visibility.
* Clean up includes. This might break external modules.
* New CModCommand for simplifiying module commands.
* Add the OnIRCConnectionError(CIRCSock *pIRCSock) module hook
* Remove config-related module hooks.
* Fix CString::Escape_n()
* Make the CUser::IsIRCConnected method check if ZNC already successfully logged in to IRC.
* and more...

# ZNC 0.098 (2011-03-28)


## New stuff
* Add a list of features to the output of /znc version. (cce5824)
* Add modpython. (a564e2) (88c84ef) (1854e1) (9745dcb) (644632) (4c6d52c) (b7700fe) (0f2265c)
* Verify during --makeconf that the specified listener works. (11ffe9d)
* Add TimestampFormat and StatusPrefix settings to admin. (853ddc5)
* Add DCCBindHost, channel keys and some more to webadmin. (570fab6) (eb26386) (1e0585c)
* Add a web interface to listsockets. (144cdf)
* Add a web interface to perform. (c8910c) (89edf703) (ba183e46)
* Don't reply to CTCP floods. (142eeb)
* Accept wildcards for /znc DetachChan, EnableChan, ClearBuffer and SetBuffer. (e66b24)
* Added reconnect and disconnect commands to admin. (65ae83)
* Moved from SourceForge to GitHub. (daa610) (ed17804) (86c0e97) (e6bff0c) (087f01)
* Don't force --foreground when compiling with --enable-debug. (778449) (fbd8d6)
* Add functions for managing CTCPReplies to admin. (3f0e200)
* Allow omitting user names with some commands in admin. (4faad67)
* Some fixed with ISpoofFile and SSLCertFile paths which use "~". (cd7822) (f69aeff) (ce10cee)

## Fixes
* Send more than a single channel per JOIN command. (3327a97) (6a1d27)
* Properly unload perl modules so that their code is reread on next load. (ce45917)
* Make certauth remember its module list across restarts again. (451b7e32)
* Ignore dereferenced sockets in listsockets. (50b57b)
* Fix a cross-compilation problem in configure. (d9b4ba1)
* Bind web sessions to IP addresses. (577a097) (4556cc)
* Limit the number of web sessions per IP. (913a3c8) (bf6dc45)
* Build on cygwin again. (37b70a)
* Allow admins to ignore MaxBufferSize in webadmin. (b37e23)
* Fix some compiler warning generated by clang. (c7c12f0)
* Call modules for mode-changes done by not-in-channel nicks. (a53306)
* Fix installation with checkinstall and the permissions of some static data. (4c7808) (3d3235)

## Minor stuff
* Properly report errors in admin's addserver command. (ed924cb)
* Improvements of modperl. (1baa019) (12b1cf6) (7237b9) (ece2c88)
* Check for modperl that we have at least Perl 5.10. (0bc606)
* Verify in configure that tcl actually works. (89bd527)
* Add a warning header to znc.conf that warns about editing the file. (2472ea) (8cadb6)
* Improve the ISpoof debug output. (128af8e)
* Improve HTTP client-side caching for static files. (9ef41ae) (4e5f9e8)
* Removed all generated/copied scripts from the repo. (fde73c60) (9574d6) (e2ce2cf) (5a6a7b)
* Only allow admins to use email. (81c864)
* Make clearbufferonmsg clear the buffer a little less often. (ddd302fbf)
* Make the output from /znc help smaller. (0d928c)
* Add a web interface to send_raw. (d8b181) (a93a586)

## Internal stuff
* Some optimizations with lots of users and channels. (b359f) (5e070e)
* Various changes to the Makefiles. (33e1ccc) (0ad5cf) (df3409) (e17348c) (936b43) (18234a) (0cc8beb) (d21a1be) (517307b) (40632f) (afa16df) (4be0572) (452e3f) (9fec8f) (f76f1e7) (6b396f)
* Added a third argument to the OnPart module hook. (a0c0b7) (1d10335) (e4b48d5)
* Added vim modelines to some files. (dc8a39)
* Added an auto-generated zncconfig.h (8a1c2a4) (aeeb1eb3) (f4927709) (40a1bb) (3ecbf13) (87037f) (b6c8e1)
* Update to latest Csocket. (cc552f)
* Handle paths like "~/foo" in CFile. (cb2e50a)
* Add some generic interface for module commands. (ebd7e53) (8e59fb9) (31bbffa)
* CUser::m_sUserName was made const. (d44e590)

# ZNC 0.096 (2010-11-06)

## New stuff
* Added a new module: clearbufferonmsg. (r2107) (r2151)
* Added an optional server name argument to /znc jump. (r2109)
* Big overhaul for modperl. (r2119) (r2120) (r2122) (r2123) (r2125) (r2127) (r2133) (r2136) (r2138) (r2140) (r2142) (r2143) (r2144) (r2146) (r2147) (r2156) (r2160)
* Modules can now directly influence other modules' web pages. (r2128) (r2129) (r2130) (r2131) (r2132) (r2134) (r2135)

## Fixes
* The route_replies module now handles "354" who replies. (r2112)
* Fixed a bogus "invalid password" error during login with some clients. (r2117)
* Reject long input lines on incoming connections. (r2124)
* The lastseen module should only link to webadmin if the latter is loaded. (r2126)
* Fixed cases where HTTP requests were incorrectly dropped. (r2148) (r2149)
* Fixed partyline to work with servers that don't send a 005 CHANTYPES. (r2162)
* Fixed error message from configure if dlopen() isn't found. (r2166)

## Minor stuff
* Renamed "vhost" to "bindhost" to better describe what the option does. (r2113)
* Honor timezone offset in the simple_away module. (r2114)
* Load global modules as soon as their config line is read. (r2118)
* Use poll() instead of select() by default. (r2153) (r2165)
* Ignore the channel key "*" in the chansaver module. (r2155)

## Internal stuff
* Fixed some function prototypes. (r2108)
* Rearranged ZNC's CAP handling to IRCds. (r2137)
* Added more doxygen comments. (r2139) (r2145) (r2150) (r2152) (r2154) (r2157)
* Removed some useless typedefs. (r2158)
* Clean up the lastseen module. (r2163) (r2164)

# ZNC 0.094 (2010-08-20)

## New stuff
* Add new global setting MaxBufferSize instead of hardcoding a value. (r2020) (r2025)
* Support CAP. (r2022) (r2024) (r2027) (r2041) (r2048) (r2070) (r2071) (r2097) (r2098) (r2099) (r2100)
* Add new module certauth which works similar to certfp. (r2029)
* route_replies now also supports routing channel ban lists, ban exemptions and invite exceptions. (r2035)
* Add a -nostore flag to the away module. (r2044)
* Add a new config option SSLCertFile. (r2086) (r2088)

## Fixes
* Fix configure to automatically disable modperl if perl is not found. (r2017)
* Include the port number in cookie names to make them unique across different znc instances on the same box. (r2030)
* Make sure that we have at least c-ares 1.5.0. (r2055)
* Make znc work on solaris. (r2064) (r2065) (r2067) (r2068)
* Improve configure's and make's output. (r2079) (r2080) (r2094) (r2101)
* Complain about truncated config files. (r2083)
* Fix some std::out_of_range error triggerable by people with a valid login. (r2087) (r2093) (r2095)
* Make fakeonline behave while we are not connected to an IRC server. (r2091)
* Always attach to channels when joining them. (r2092)
* Fix a NULL pointer dereference in route_replies. (r2102) (r2103)

## Minor stuff
* Allow leading and trailing spaces in config entries. (r2010)
* Various minor changes. (r2012) (r2014) (r2021)
* Use pkg-config for finding openssl, if it's available. We still fall back to the old code if this fails. (r2018)
* znc no longer accepts an alternative file name for znc.conf as its argument. (r2037)
* Generate correct HTTP status codes in webmods and make sure this doesn't happen again. (r2039) (r2040)
* Rewrite our PING/PONG handling. (r2043)
* Raise the size of the query buffer to 250. (r2089)
* Update to latest Csocket. (r2096)

## Internal stuff
* Remove the fake module usage in WebMods. (r2011)
* Remove fake modules completely. (r2012) (r2015)
* Make CTable more robust. (r2031)
* Move the OnKick() module call so it is issued when the nick still is visible in the channel. (r2038)
* Remove CZNC::GetUser() since CZNC::FindUser() does the same. (r2046)
* Minor changes to webmod skins. (r2061) (r2062)
* Add new macros GLOBALMODULECALL and ALLMODULECALL. (r2074) (r2075) (r2076)
* Remove a bogus CClient* argument from some module calls. (r2077)
* Mark some functions as const. (r2081) (r2082) (r2084) (r2085)

# ZNC 0.092 (2010-07-03)

This is a bugfix-only release, mainly for fixing CVE-2010-2488.

## Fixes
* ZNC wrongly counted outgoing connections towards the AnonIPLimit config option. (r2050)
* The traffic stats caused a NULL pointer dereference if there were any unauthenticated connections. CVE-2010-2488 (r2051)
* Csocket had a bug where a wrong error message was generated and one that caused busy loops with c-ares. (r2053)

# ZNC 0.090 (2010-06-06)

## Upgrading from previous versions

## Errors during start-up
The shell, email and imapauth modules have been moved from the regular module set to the "extra" set, you have to use --enable-extra with ./configure to compile them.

So, to fix these errors, edit the znc.conf file in ~/.znc/configs and don't load those modules, or recompile znc with extra.

### WebMods
While previously only the "webadmin" provided an HTTP server/interface, the HTTP server is now integrated into ZNC's core. This means that all modules (not only webadmin) can now provide web pages. Examples shipping with ZNC are lastseen, stickychan and notes. Old-style module arguments to webadmin will be automatically converted to the new syntax.

Please note that the WebMods interface uses session cookies instead of 'Basic' HTTP authentication.

All URLs to webadmin's settings pages have changed. Please adjust your scripts etc. if necessary.

### Running without installing
If you want to run ZNC without doing make install, i.e. if you want to run it from the source dir, you will have to add --enable-run-from-source as an argument to ./configure. You do not have to care about this if you use a --prefix= or if you install ZNC system-wide.

### I upgraded and WebAdmin/WebMods is acting weird, Log Out does not work.
Starting with 0.090, ZNC uses cookies instead of HTTP Basic authentication. If your browser is still sending the Basic credentials to ZNC, e.g. because you have saved them in a bookmark, or password manager, or simply haven't restarted your browser in a while, those will continue to work, even after you click the Log Out button.

To fix this, remove any user:pass@host portions from your bookmarks, remove all entries for ZNC's web interface from your password manager, and restart your browser.

## New stuff
* Webmods - Every module can now provide its own webpages. (r1784) (r1785) (r1787) (r1788) (r1789) (r1790) (r1791) (r1792) (r1793) (r1795) (r1796) (r1797) (r1800) (r1801) (r1802) (r1804) (r1805) (r1806) (r1824) (r1825) (r1826) (r1827) (r1843) (r1844) (r1868) (r1886) (r1888) (r1915) (r1916) (r1931) (r1934) (r1870) (r1871) (r1872) (r1873) (r1874) (r1875) (r1876) (r1879) (r1887) (r1891) (r1967) (r1982) (r1984) (r1996) (r1997) (r2000) (r2002) (r2003)
* Webmods and thus webadmin now use cookies for managing sessions instead of HTTP authentication. (r1799) (r1819) (r1823) (r1839) (r1840) (r1857) (r1858) (r1859) (r1861) (r1862)
* WebMod-enabled lastseen, stickychan modules. (r1880) (r1881) (r1889) (r1918)
* Partyline now also handles notices, /me and CTCP. (r1758)
* Partyline now saves channel topics across restarts. (r1898) (r1901)
* Added a "number of channels" column to /znc listusers. (r1769)
* Added an optional user name argument to /znc listchans. (r1770)
* Support for the general CAP protocol and the multi-prefix and userhost-in-names caps on connections to the IRC server. (r1812)
* ZNC can now listen on IPv4-only, IPv6-only or on both-IP sockets. Renamed "Listen" config option to "Listener". (r1816) (r1817) (r1977)
* Added LoadModule, UnLoadModule, ListMods commands to the Admin module. (r1845) (r1864)
* Added ability to set/get TimezoneOffset to the Admin module. (r1906)
* Added "Connect to IRC + automatically re-connect" checkbox to webadmin. (r1851)
* Remember "automatically connect + reconnect" flag across restarts by writing it to the config file. (r1852)
* Added AddPort, DelPort, ListPorts command to *status. (r1899) (r1913)
* Added optional quit message argument to disconnect command. (r1926)
* Added new charset module to extra. (r1942) (r1947) (r1977) (r1985) (r1994)
* Added a traffic info page to webadmin. (r1958) (r1959)

## Fixes
* Don't let ZNC connect to itself. (r1760)
* Added a missing error message to /znc updatemod. (r1772)
* Generate cryptographically stronger certificates in --makepem. (r1774)
* Autoattach now triggers on channel actions. (r1778)
* --disable-tcl now really disables TCL instead of enabling it. (r1782)
* User name comparison in blockuser is now case-sensitive. (r1786)
* Fixed /names when route_replies is loaded. (r1811)
* autoreply now ignores messages from self. (r1828)
* Don't forward our own QUIT messages to clients. (r1860)
* Do not create empty directories if one does ./znc --datadir=NON_EXISTING_DIR. (r1878)
* Query to Raw send the command to IRC instead of to the client. (r1892)
* Fixed desync in Partyline after addfixchan or delfixchan. (r1904)
* Save passwords for Nickserv module as NV instead of keeping them as arguments. (r1914)
* CSRF Protection. (r1932) (r1933) (r1935) (r1936) (r1938) (r1940) (r1944)
* Fixed a rare configure failure with modperl. (r1946)
* disconkick now only sends kicks for channels the client actually joined. (r1952)
* More sanity checks while rewriting znc.conf. (r1962)
* Fixed static compilation with libcrypto which needs libdl by checking for libdl earlier. (r1969)
* Fixed modtcl with newer tcl versions. (r1970)
* Better error message if pkg-config is not found. (r1983)
* Fixed a possible race condition in autoop which could cause bogous "invalid password" messages. (r1998)

## Minor stuff
* Fixed a memory leak and some coding style thanks to cppcheck. (r1761) (r1762) (r1763) (r1764) (r1776) (r1777)
* Updated to latest Csocket. (r1766) (r1767) (r1814) (r1905) (r1930)
* Cleanup to /znc help. (r1771)
* Removed --disable-modules. Modules are now always enabled. (r1794) (r1829)
* saslauth: Error out "better" on invalid module arguments. (r1809)
* Changed the default ConnectDelay from 30s to 5s. (r1822)
* Misc style/skin fixes to webadmin/webmods. (r1853) (r1854) (r1856) (r1883) (r1884) (r1885) (r1890) (r1900) (r1907) (r1908) (r1909) (r1911) (r1912) (r1917) (r1945) (r2005)
* Do not expose ZNC's version number through the web interface unless there's an active user session. (r1877)
* Updated AUTHORS file. (r1902) (r1910) (r1999)
* Moved some modules into/out of extra. (r1919) (r1922) (r1923)
* Added ./configure --enable-run-from-script, without it ZNC will no longer look for modules in ./modules/. (r1927) (r1928) (r2001)
* Made a dedicated page to confirm user deletion in webadmin. (r1937) (r1939) (r1941) (r1943)
* Use spaces for separating ip addresses from ports. (r1955)
* ZNC's built-in MOTD now goes through ExpandString. (r1956)
* Check for root before generating a new config file. (r1988)
* Added a flag for adding irc-only / http-only ports via /znc addport. (r1990) (r1992)

## Internal stuff
* Minor cleanup to various places. (r1757) (r1759) (r1846) (r1847) (r1863) (r1865) (r1920) (r1921) (r2004)
* Changes in configure. (r1893) (r1894) (r1895) (r1896) (r1897)
* Flakes messed with the version number. (r1768)
* CString::Split() now Trim()s values before pushing them if bTrimWhiteSpace is true. (r1798)
* Added new module hooks for config entries. (r1803) (r1848) (r1849) (r1850)
* New module hook OnAddUser(). (r1820) (r1821)
* Cleanup to ISUPPORT parser. (r1807)
* Use Split() instead of Token() where possible. (r1808)
* Modularize CIRCSock::ForwardRaw353(). (r1810)
* Use a better seed for srand(). (r1813)
* Changes to debug output. (r1815) (r1836) (r1837) (r1855) (r1882)
* Support for delayed HTTP request processing. (r1830) (r1833) (r1834) (r1835) (r1838) (r1841) (r1842)
* Fixed CSmartPtr's operator==. (r1818)
* Better port/listener management exposed through CZNC. (r1866) (r1867)
* Move CListener and CRealListener into their own files. (r1924)
* Move the HTTP/IRC switching to CIncomingConnection. (r1925)
* Add IsIRCAway() to CUser. (r1903)
* Move some common pid file code into new InitPidFile(). (r1929)
* Templates can now sort loops based on a key. (r1948) (r1949) (r1951) (r1953) (r1954)
* A lot of work went into this release, we would like to thank everyone who contributed code, helped testing or provided feedback.

# ZNC 0.080 (2010-02-18)

## New stuff
* Update to latest Csocket. (r1682) (r1727) (r1735) (r1751) (r1752) (r1753)
* Only allow admins to load modtcl unless -DMOD_MODTCL_ALLOW_EVERYONE is used. (r1684)
* Include /me's in the query buffer. (r1687)
* Some tweaks to savebuff to differentiate it more from buffextras. (r1691) (r1692)
* send_raw can now also send to clients. (r1697)
* Move the "Another client authenticated as you"-message into new module clientnotify. (r1698)
* Imported block_motd module into extra. (r1700)
* Imported flooddetach into extra. (r1701) (r1717)
* Added new setting ServerThrottle which sets a timeout between connections to the same server. (r1702) (r1705)
* Only ask for the more common modules in --makeconf. (r1706)
* Use UTF-8 as default charset for webadmin. (r1716)
* Revamped default webadmin skin. It's very grayish, but looks way more like 2010 than the old default skin does. (r1720)
* New font style for the "ice" webadmin skin. (r1721)
* Added a summary line to /znc listchans. (r1729)
* The admin module can now handle more settings and got some missing permission checks added. (r1743) (r1744) (r1745) (r1746) (r1747)

## Fixes
* Apply new ConnectDelay settings immediately after a rehash. (r1679)
* Do a clean shutdown just before a restart. (r1681)
* Fix a theoretical crash in modtcl. (r1685)
* Users should use the correct save and download path after Clone(). (r1686)
* Several improvements to znc-buildmod. (r1688) (r1689) (r1690) (r1695)
* Fix a crash with modperl by loading modules differently. (r1714)
* Fix HTTP Cache-Control headers for static files served by webadmin. (r1719)
* Send the nicklist to a user who is being force-rejoined in partyline. (r1731)
* Set the issuer name in CUtils::GenerateCert(). (r1732)
* Fixed some inconsistency with /znc reloadmod. (r1749)
* Added a work-around for SSL connections which incorrectly errored out during handshake. (r1750)

## Minor stuff
* Don't try to catch SIGILL, SIGBUS or SIGSEGV, the default action will do fine. (r1680)
* Added IP-address to messages from notify_connect. (r1699)
* Switched to Csocket's own c-ares code. (r1704) (r1707) (r1708) (r1709) (r1710) (r1712) (r1713)
* Add more doxygen comments. (r1715) (r1718) (r1737)
* Removed useless "add your current ip" checkbox from webadmin's edit user page. (r1722)
* Don't try to request a MOTD if there is none. (r1728)

## Internal stuff
* It's 2010, where's my hoverboard? (r1693)
* Got rid of Timers.h. (r1696)
* Added a Clone() method to CNick. (r1711)
* Call OnChanAction() after OnChanCTCP(). (r1730)
* Random cleanups to CFile::Delete(). (r1694)
* Other random cleanups. (r1723) (r1724) (r1725) (r1726) (r1736)
* Move the implementation of CSocket to Socket.cpp/h. (r1733)

# ZNC 0.078 (2009-12-18)

## New stuff
* Add a DCCVHost config option which specifies the VHost (IP only!) for DCC bouncing. (r1647)
* Users cloned via the admin module no longer automatically connect to IRC. (r1653)
* Inform new clients about their /away status. (r1655)
* The "BUG" messages from route_replies can now be turned off via /msg *route_replies silent yes. (r1660)
* Rewrite znc.conf on SIGUSR1. (r1666)
* ISpoofFormat now supports ExpandString. (r1670)

## Fixes
* Allow specifing port and password for delserver. (r1640)
* Write the config file on restart and shutdown. (r1641)
* Disable c-ares if it is not found unless --enable-c-ares was used. (r1644) (r1645)
* blockuser was missing an admin check. (r1648)
* Sometimes, removing a server caused znc to lose track of which server it is connected to. (r1659)
* Include a more portable header for uint32_t in SHA256.h. (r1665)
* Fixed cases where ZNC didn't properly block PONG replies to its own PINGs. (r1668)
* Fixed a possible crash if a client disconnected before an auth module was able to verify the login. (r1669)
* Away allowed to accidentally execute IRC commands. (r1672)
* Correctly bind to named hosts if c-ares is enabled. (r1673)
* Don't accept only spaces as QuitMsg because this would cause an invalid config to be written out. (r1675)

## Minor stuff
* Comment out some weird code in Client.cpp. (r1646)
* Remove connect_throttle since it's obsoleted by fail2ban. (r1649)
* Remove outdated sample znc.conf. (r1654)
* route_replies now got a higher timeout before it generates a "BUG" message. (r1657)
* Documented the signals on which znc reacts better. (r1667)

## Internal stuff
* New module hook OnIRCConnecting(). (r1638)
* Remove obsolete CUtils::GetHashPass(). (r1642)
* A module's GetDescription() now returns a C-String. (r1661) (r1662)
* When opening a module, check the version number first and don't do anything on a mismatch. (r1663)

# ZNC 0.076 (2009-09-24)

## New stuff
* Add a make uninstall Makefile target. (r1580)
* Imported modules from znc-extra: fixfreenode, buffextras, autoreply, route_replies, adminlog. (r1591) (r1592) (r1593) (r1594) (r1595)
* Imported the rest of znc-extra under modules/extra hidden behind configure's --enable-extra. (r1605) (r1606) (r1608) (r1609) (r1610)
* ZNC now uses SHA-256 instead of MD5 for hashing passwords. MD5 hashes still work correctly. (r1618)

## Fixes
* Don't cache duplicate raw 005 (e.g. due to /version). (r1579)
* Send a MODE removing all user modes to clients when we lose the irc connection. (r1583)
* Use a nickmask instead of a nick as the source for ZNC-generated MODE commands. (r1584)
* Use the right error codes if startup fails. (r1585)
* Fix a NULL pointer dereference in some of the ares-specific code. (r1586)
* VHost and Motd input boxes in graphiX and dark-clouds in webadmin didn't insert newlines. (r1588)
* Generate proper error messages when loading modules. This was broken since znc 0.070. (r1596)
* Allow unloading of removed modules. This was broken since znc 0.070. (r1597)
* Fix savebuff with KeepBuffer = false. (r1616)
* Fix accidental low buffer size for webadmin sockets. (r1617)
* AltNicks are no longer truncated to 9 characters. (r1620)
* Webadmin can now successfully add new admin users and have them load the shell module. (r1625)
* Webadmin no longer includes the znc version in the auth realm. (r1627)
* CUser::Clone now handles modules after all other settings, making it work with shell. (r1628)
* Some CSS selectors in webadmin's dark-clouds and graphiX skins were wrong. (r1631)
* The help of admin was improved. (r1632) (r1633)

## Minor stuff
* make distclean now also removes the pkg-config files. (r1581)
* Add the autoconf check for large file support. (r1587)
* Generic "not enough arguments" support for route_replies and some fix for /lusers. (r1598) (r1600)
* ZNC now tries to join channels in random order. (r1601) (r1602) (r1603)
* route_replies now handles "No such channel" for /names. (r1614)
* Fixes a theoretical crash on shutdown. (r1624)
* saslauth was moved to znc-extra. (r1626)

## Internal stuff
* Now using autoconf 2.64. (r1604)
* Removed unused classes CNoCopy and CSafePtr. (r1607)
* Moved CZNC::FindModPath() to CModules. (r1611)
* Added CModules::GetModDirs() as a central place for finding module dirs. (r1612) (r1629)
* Added CModules::GetModPathInfo() which works like GetModInfo() but which takes the full path to the module. (r1613)
* Updated to latest Csocket which adds openssl 1.0 compatibility and fixes some minor bug. (r1615) (r1621)
* Merged the internal join and ping timers. (r1622) (r1623)

# ZNC 0.074 (2009-07-23)

## Fixes
* Fix a regression due to (r1569): Webadmin was broken if the skins were accessed through an absolute path (=almost always). (r1574)
* Fix a possible crash if users are deleted while they have active DCC sockets. (r1575)

Sorry for breaking your webadmin experience guys. :(

# ZNC 0.072 (2009-07-21)

All webadmin skins are broken in this release due to a bug in webadmin itself. This is fixed in the next release.

High-impact security bugs
There was a path traversal bug in ZNC which allowed attackers write access to any place to which ZNC has write access. The attacker only needed a user account (with BounceDCCs enabled). Details are in the commit message. (r1570)

This is CVE-2009-2658.

Affected versions
All ZNC versions since ZNC 0.022 (Initial import in SVN) are affected.

## New stuff
* /msg *status uptime is now accessible to everyone. (r1526)
* ZNC can now optionally use c-ares for asynchronous DNS resolving. (r1548) (r1549) (r1550) (r1551) (r1552) (r1553) (r1556) (r1565) (r1566)
* The new config option AnonIPLimit limits the number of unidentified connections per IP. (r1561) (r1563) (r1567)

## Fixes
* znc --no-color --makeconf still used some color codes. (r1519)
* Webadmin favicons were broken since (r1481). (r1524)
* znc.pc was installed to the wrong directory in multilib systems. (r1530)
* Handle flags like e.g. --allow-root for /msg *status restart. (r1531) (r1533)
* Fix channel user mode tracking. (r1574)
* Fix a possible crash if users are deleted while they are connecting to IRC. (r1557)
* Limit HTTP POST data to 1 MiB. (r1559)
* OnStatusCommand() wasn't called for commands executed via /znc. (r1562)
* On systems where sizeof(off_t) is 4, all ZNC-originated DCCs failed with "File too large (>4 GiB)". (r1568)
* ZNC didn't properly verify paths when checking for directory traversal attacks (Low impact). (r1569)

## Minor stuff
* Minor speed optimizations. (r1527) (r1532)
* stickychan now accepts a channel list as module arguments. (r1534)
* Added a clear command to nickserv. (r1554)
* Added an execute command to perform. (r1558)
* Added a swap command to perform. (r1560)
* fail2ban clears all bans on rehash. (r1564)

## Internal stuff
* The API for traffic stats changed. (r1521) (r1523)
* Some optimizations to CSmartPtr. (r1522)
* CString now accepts an optional precision for converting floating point numbers. (r1525)
* Made home dir optional in CDir::ChangeDir(). (r1536)
* Stuff. (r1537) (r1550)
* EMFILE in CSockets is handled by closing the socket. (r1544)
* Special thanks to cnu and flakes!

# ZNC 0.070 (2009-05-23)

## New stuff
* Add a CloneUser command to admin. (r1477)
* Make webadmin work better with browser caches in conjunction with changing skins. (r1481) (r1482)
* Better error messages if binding a listening port fails. (r1483)
* admin module now supports per-channel settings. (r1484)
* Fix the KICK that partyline generates when a user is deleted. (r1486)
* fail2ban now allows a couple of login attempts before an IP is banned. (r1489)
* Fixed a crash bug in stickychan. (r1500)
* Install a pkg-config .pc file. (r1503)
* Auto-detect globalness in re/un/loadmod commands. (r1505)

## Fixes
* Fix a bug where ZNC lost its lock on the config file. (r1457)
* Limit DCC transfers to files smaller than 4 GiB. (r1461)
* Make znc -D actually work. (r1466)
* Make znc --datadir ./meh --makeconf work. The restart used to fail. (r1468)
* Fix a crash bug if CNick::GetPermStr() was called on CNick objects from module calls. (r1491)
* Some fixes for solaris. (r1496) (r1497) (r1498)
* nickserv module now also works on OFTC. (r1502)
* Make sure the "Invalid password" message is sent before a client socket is closed. (r1506)
* Fix a bug where ZNC would reply with stale cached MODEs for a "MODE #chan" request. (r1507)

## Minor stuff
* Man page updates. (r1467)
* Make CFile::Close() check close()'s return values if --debug is used. (r1476)
* Update to latest Csocket. (r1490)
* Improve the error messages generated by /msg *status loadmod. (r1493)
* Remove broken znc --encrypt-pem. (r1495)

## Internal stuff
* cout and endl are included in Utils.h, not main.h. (r1449)
* CFile::Get*Time() now return a time_t. (r1453) (r1479)
* Switched some more CFile members to more appropriate return types. (r1454) (r1471)
* CFile::Seek() now takes an off_t as its argument. (r1458)
* Turn TCacheMap into more of a map. (r1487) (r1488)
* Updates to latest Csocket. (r1509)
* API breakage: CAuthBase now wants a Csock* instead of just the remote ip. (r1511) (r1512)
* New Module hooks (r1494)
    * OnChanBufferStarting()
    * OnChanBufferPlayLine()
    * OnChanBufferEnding()
    * OnPrivBufferPlayLine()

# ZNC 0.068 (2009-03-29)

## New stuff
* watch now uses ExpandString on the patterns. (r1402)
* A user is now always notified for failed logins to his account. This now also works with auth modules like imapauth. (r1415) (r1416)
* Added /msg *status UpdateModule <mod> which reloads an user module on all users. (r1418) (r1419)
* A module whose version doesn't match the current ZNC version is now marked as such in ListAvailModules and friends. (r1420)
* Added a Set password command to admin. (r1423) (r1424)
* ZNC no longer uses znc.conf-backup. (r1432)
* Two new command line options were added to ZNC:
* ZNC --foreground and znc -f stop ZNC from forking into the background. (r1441)
* ZNC --debug and znc -D produce output as if ZNC was compiled with --enable-debug. (r1442) (r1443)

## Fixes
* cd in shell works again. (r1401)
* Make WALLOPS properly honour KeepBuffer. Before this, they were always added to the replay buffer. (r1405)
* ZNC now handles raw 432 Illegal Nickname when trying to login to IRC and sends its AltNick. (r1425)
* Fix a crash with recursion in module calls. (r1438)
* Fixed some compiler warnings with -Wmissing-declarations. (r1439)

## Minor stuff
* Allow a leading colon on client's PASS commands. (r1403)
* CFile::IsDir() failed on "/". (r1404)
* CZNC::AddUser() now always returns a useful error description. (r1406)
* Some micro-optimizations. (r1408) (r1409)
* The new default for JoinTries is 10. This should help some excess flood problems. (r1411)
* All webadmin skins must now reside inside the webadmin skins dir or they are rejected. (r1412)
* Watch now saves its module settings as soon as possible, to prevent data loss on unclean shutdown. (r1413) (r1414)
* Regenerated configure with autoconf 2.63. (r1426)
* Some dead code elimination. (r1435)
* Clean up znc -n output a little. (r1437)

## Internal stuff
* CString::Base64Decode() now strips newlines. (r1410)
* Remove CModInfo::IsSystem() since it was almost unused and quite useless. (r1417)
* Some minor changes to CSmartPtr. (r1421) (r1422)
* Added CFile::Sync(), a fsync() wrapper. (r1431)

# ZNC 0.066 (2009-02-24)

There was a privilege escalation bug in webadmin which could allow all ZNC users to write to znc.conf. They could gain shell access through this. (r1395) (r1396)

This is CVE-2009-0759.

## Affected versions
This bug affects all versions of ZNC which include the webadmin module. Let's just say this affects every ZNC version, ok? ;)

## Who can use this bug?
First, ZNC must have the webadmin module loaded and accessible to the outside. Now any user who already has a valid login can exploit this bug.

An admin must help (unknowingly) to trigger this bug by reloading the config.

## Impact
Through this bug users can write arbitrary strings to the znc.conf file.

Unprivileged ZNC users can make themselves admin and load the shell module to gain shell access.
Unprivileged ZNC users can temporarily overwrite any file ZNC has write access to via ISpoof. This can be used to overwrite ~/.ssh/authorized_keys and gain shell access.
Unprivileged ZNC users can permanently truncate any file to which ZNC has write access via ISpoof. ZNC never saves more than 1kB for restoring the ISpoofFile.

## How can I protect myself?
Upgrade to ZNC 0.066 or newer or unload webadmin.

## What happens?
Webadmin doesn't properly validate user input. If you send a manipulated POST request to webadmin's edit user page which includes newlines in e.g. the QuitMessage field, this field will be written unmodified to the config. This way you can add new lines to znc.conf. The new lines will not be parsed until the next rehash or restart.

This can be done with nearly all input fields in webadmin. Because every user can modify himself via webadmin, every user can exploit this bug.

## Thanks
Thanks to cnu for finding and reporting this bug.

## New stuff
* Added the admin module. (r1379) (r1386)
* savebuff and away no longer ask for a password on startup. (r1388)
* Added the fail2ban module. (r1390)

## Fixes
* savebuff now also works with KeepBuffer turned off. (r1384)
* webadmin did not properly escape module description which could allow XSS attacks. (r1391)
* Fix some "use of uninitialized variable" warnings. (r1392)
* Check the return value of strftime(). This allowed reading stack memory. (r1394)

## Minor stuff
* Some dead code elimination. (r1381)
* Don't have two places where the version number is defined. (r1382)

## Internal stuff
* Removed some useless and unused CFile members. (r1383)
* Removed the DEBUG_ONLY define. (r1385)
* OnFailedLogin() is now called for all failed logins, not only failed IRC ones. This changes CAuthBase API. (r1389)

# ZNC 0.064 (2009-02-16)

## New stuff
* schat now prints a message if a client connects and there are still some active schats. (r1282)
* awaynick: Set awaynick on connect, not after some time. (r1291)
* Allow adding new servers through /msg *status addserver even if a server with the same name but e.g. a different port is already added. (r1295) (r1296)
* Show the current server in /msg *status listservers with a star. (r1308)
* /msg *status listmods now displays the module's arguments instead of its description. Use listavailmods for the description. (r1310)
* ZNC now updates the channel buffers for detached channels and thus gives a buffer replay when you reattach. (r1325)
* watch now adds timestamps to messages it adds to the playback buffers. (r1333)
* ZNC should now work on cygwin out of the box (use --disable-ipv6). (r1351)
* Webadmin will handle all HTTP requests on the irc ports. (r1368) (r1375)

## Fixes
* Handle read errors in CFile::Copy() instead of going into an endless loop. (r1280) (r1287)
* Make schat work properly again and clean it up a little. (r1281) (r1303)
* Removed all calls to getcwd(). We now no longer depend on PATH_MAX. (r1286)
* stickychan: Don't try to join channels if we are not connected to IRC. (r1298)
* watch now saved its settings. (r1304)
* Don't forward PONG replies that we requested to the user. (r1309)
* awaynick evaluated the awaynick multiple times and thus didn't set the nick back. (r1313)
* znc-config --version said '@VERSION@' instead of the actual version number. (r1319)
* Handle JOIN redirects due to +L. (r1327)
* Remove the length restrictions on webadmin's password fields which led to silent password truncation. (r1330)
* Webadmin now reloads global modules if you change their arguments. (r1331)
* The main binary is no longer built with position independent code. (r1338)
* ZNC failed to bounce DCCs if its own ip started with a value above 127. (r1340)
* Savebuff no longer reloads old channel buffers if you did /msg *status clearbuffer. (r1345)
* Some work has been done to make ZNC work with mingw (It doesn't work out of the box yet). (r1339) (r1341) (r1342) (r1343) (r1344) (r1354) (r1355) (r1356) (r1358) (r1359)
* modperl used huge amounts of memory after some time. This is now fixed. (r1357)
* shell now generates error messages if e.g. fork() fails. (r1369)
* If the allowed buffer size is lowered, the buffer is now automatically shrunk. (r1371)
* webadmin now refuses to transfer files bigger than 16 MiB, because it would block ZNC. (r1374)

## Minor stuff
* Only reply to /mode requests if we actually know the answer. (r1290)
* Lowered some timeouts. (r1297)
* Memory usage optimizations. (r1300) (r1301) (r1302)
* Allow custom compiler flags in znc-buildmod via the $CXXFLAGS and $LIBS environment flags. (r1312)
* Show the client's IP in debug output if no username is available yet. (r1315)
* Allow /msg *status setbuffer for channels we are currently not on. (r1323)
* Updated the README. (r1326)
* Use @includedir@ instead of @prefix@/include in the Makefile. (r1328)
* Use RTLD_NOW for loading modules instead of RTLD_LAZY which could take down the bouncer. (r1332)
* Use stat() instead of lstat() if the later one isn't available. (r1339)
* CExecSock now generates an error message if execvp() fails. (r1362)
* Improved some error messages. (r1367)

## Internal stuff
* Add traffic tracking support to CSocket. Every module that uses CSocket now automatically gets the traffic it causes tracked. (r1283)
* Add VERSION_MINOR and VERSION_MAJOR defines. (r1284) (r1285)
* Rework CZNC::Get*Path() a little. (r1289) (r1292) (r1299)
* Remove the IPv6 stuff from CServer. It wasn't used at all. (r1294)
* Make email use CSocket instead of Csock. (r1305)
* Cleaned up and improved CFile::ReadLine() and CChan::AddNicks() a little. (r1306) (r1307)
* Replaced most calls to strtoul() and atoi() with calls to the appropriate CString members. (r1320)
* Moved the SetArgs() call before the OnLoad() call so that modules can overwrite there arguments in OnLoad(). (r1329)
* Let CZNC::AddUser() check if the user name is still free. (r1346)
* API stuff
    * Added CModule::IsGlobal(). (r1283)
    * Added CModule::BeginTimers(), EndTimers(), BeginSockets() and EndSockets(). (r1293)
    * Added CModule::ClearNV(). (r1304)
    * Removed ReadFile(), WriteFile(), ReadLine() (Use CFile instead), Lower(), Upper() (Use CString::AsUpper(), ::ToUpper(), ::*Lower() instead) and added CFile::ReadFile() (r1311)
    * Added CModules::OnUnknownUserRaw(). (r1314)
    * Added CUtils::SaltedHash() for computing the salted MD5 hashes ZNC uses. (r1324)
    * Removed CLockFile and made CFile take over its job. (r1337) (r1352) (r1353)
    * Change the return type to CUtils::GetPass() to CString. (r1343)
    * Added a DEBUG(f) define that expands to DEBUG_ONLY(cout << f << endl). (r1348) (r1349)
    * Removed some unused functions and definitions. (r1360) (r1361)

# ZNC 0.062 (2008-12-06)

## New stuff
* Add --disable-optimization to configure. (r1206)
* New webadmin skin dark-clouds by bigpresh. (r1210)
* Added the q module as a replacement for QAuth. (r1217) (r1218)
* Added an enhanced /znc command: (r1228)
* /znc jump is equal to /msg *status jump
* /znc *shell pwd is equal to /msg *shell pwd
* Webadmin should generate less traffic, because it now uses client-side caching for static data (images, style sheets, ...). (r1248)
* Changes to the vhost interface from *status: (r1256)
* New commands: AddVHost, RemVHost and ListVHosts.
* SetVHost now only accepts vhosts from the ListVHosts list, if it is non-empty.
* ZNC now should compile and work fine on Mac OS. (r1258)
* IPv6 is now enabled by default unless you disable it with --disable-ipv6. (r1270)

## Fixes
* Make keepnick usable. (r1203) (r1209)
* Don't display 'Your message to .. got lost' for our own nick. (r1211)
* Fix compile error with GCC 4.3.1 if ssl is disabled. Thanks to sebastinas. (r1212)
* Limit the maximum buffer space each socket may use to prevent DoS attacks. (r1233)
* Properly clean the cached perms when you are kicked from a channel. (r1236)
* Due to changes in rev 1155-1158, modperl crashed on load on some machines. (r1237) (r1239)
* Stickychan didn't work with disabled channels. (r1238)
* Catch a throw UNLOAD in the OnBoot module hook. (r1249)
* Webadmin now accepts symlinks in the skin dir. (r1252)
* Fix for partyline if a force-joined user is deleted. (r1263) (r1264)
* Revert change from (r1125) so that we compile on fbsd 4 again. (r1273)

## Minor stuff
* Recompile everything if configure is run again. (r1205)
* Improved the readability of ListMods und ListAvailMods. (r1216)
* Accept "y" and "n" as answers to yes/no questions in --makeconf. (r1244)
* --makeconf now also generates a ssl certificate if a ssl listening port is configured. (r1246)
* Improved and cleaned up the simple_away module. (r1247)
* The nickserv module automatically saves the password and never displays it anymore. (r1250)
* Use relative instead of absolute URLs in all webadmin skins. (r1253) (r1254)
* Add znc-config --cxx and use it in znc-buildmod. (r1255)
* Support out-of-tree-builds. (r1257)
* Make schat's showsocks command admin-only. (r1260)
* Fix compilation with GCC 4.4. (r1269)
* Use AC_PATH_PROG instead of which to find the perl binary. (r1274)
* New AUTHORS file format. (r1275)

## Internal stuff
* Removed redundant checks for NULL pointers (r1220) (r1227)
* Renamed String.h and String.cpp to ZNCString. (r1202)
* Print a warning in CTable if an unknown column is SetCell()'d (r1223)
* Update to latest Csocket (r1225)
* Remove CSocket::m_sLabel and its accessor functions. Use the socket name Csocket provides instead. (r1229)
* modules Makefile: Small cleanup, one defines less and no compiler flags passed multiple times. (r1235)
* Webadmin now uses CSocket instead of using Csock and keeping a list of sockets itself. (r1240)
* Mark some global and static variables as const. (r1241)
* Cleanup perform, no feature changes. (r1242)
* Some tweaking to configure.in. Among other things, we now honour CPPFLAGS and don't check for a C compiler anymore. (r1251)
* On rare occasions webadmin generated weird error messages. (r1259)
* OnStatusCommand now doesn't have the const attribute on its argument. (r1262)
* Some new functions:
    * some CString constructors (e.g. CString(true) results in "true") (r1243) (r1245)
    * CString::TrimPrefix() and CString::TrimSuffix() (r1224) (r1226)
    * CString::Equals() (r1232) (r1234)
    * CTable::Clear() (r1230)
    * CClient::PutStatus(const CTable&) (r1222)
    * CGlobalModule::OnClientConnect() (r1266) (r1268)
    * CModule::OnIRCRegistration() (r1271)
    * CModule::OnTimerAutoJoin() (r1272)
* Renames:
    * CModule::OnUserAttached() is now known as CModules::OnClientLogin(). (r1266)
    * CModule::OnUserDetached() is now known as CModules::OnClientDisconnect(). (r1266)

# ZNC 0.060 (2008-09-13)

* Print a message when SIGHUP is caught. (r1197)
* Moved autocycle into a module. (r1191) (r1192)
* New module call OnMode(). (r1189)
* Added MaxJoins and JoinTries to webadmin. (r1187)
* Fix channel keyword (+k) related mess up on Quakenet (RFC, anyone?). (r1186) (r1190)
* Added new module call OnUserTopicRequest(). (r1185)
* Also add traffic generated by modules to the traffic stats. (r1183)
* Don't use znc.com but znc.in everywhere (hostname of *status etc). (r1181) (r1195)
* Close the listening port if we ran out of free FDs. (r1180)
* Add a config option MaxJoins which limits the number of joins ZNC sends in one burst. (r1177)
* Bug fix where WriteToDisk() didn't made sure a fail was empty. (r1176)
* Add ShowMOTD and reorder the HELP output of *status. (r1175)
* Add /msg *status restart . Thanks to kroimon. (r1174)
* Make --makeconf more userfriendly. Thanks to kroimon. (r1173)
* Don't start a new znc process after --makeconf. Thanks to kroimon. (r1171)
* Add CModule::PutModule(const CTable&). (r1168) (r1169)
* Unify some preprocessor macros in Modules.cpp. (r1166)
* Catch a throw UNLOAD from CModule::OnLoad(). (r1164)
* A couple of bugs with OnModCTCP(), OnModCommand() and OnModNotice() where fixed. (r1162)
* Quicker connects and reconnects to IRC. (r1161)
* Speedup the CTable class. (r1160)
* Update our bundled Csocket. (r1159)
* Some fixes to modperl for hppa.
* Move keepnick into a module. (r1151) (r1152) (r1153)
* Split up some big functions and files. (r1148) (r1149) (r1150)
* modperl now fails to load if it can't find modperl.pm. (r1147)
* Handle nick prefixes and such stuff from clients correctly. (r1144) (r1145)
* Simplify the code connecting users a little. (r1143)
* Fix partyline for users who are not connected to IRC. (r1141)
* We are in a channel when we receive a join for it, not an 'end of /names'. (r1140)
* Enable some more debug flags with --enable-debug. (r1138)
* Don't ever throw exceptions in CModules::LoadModule(). (r1137)
* Don't give any stdin to commands executed from the shell module. (r1136)
* Fix some over-the-end iterator dereference on parting empty channels. (r1133)
* Replace usage of getresuid() with getuid() and geteuid(). (r1132)
* Use salted hashes for increased security. (r1127) (r1139)
* Don't mention any libraries in znc-config. (r1126)
* Don't define __GNU_LIBRARY__ for FreeBSD. (r1125)

# ZNC 0.058 (2008-07-10)

* Fix a crash with NAMESX-enabled IRC servers. (r1118)
* Fix a privilege escalation bug in webadmin if auth modules are used. (r1113)
* Remove -D_GNU_SOURCE from our CXXFLAGS. (r1110)
* CUtils::GetInput() now kills the process if reading from stdin fails. (r1109)
* Properly include limits.h for PATH_MAX. (r1108)
* Don't allow running ZNC as root unless --allow-root is given. (r1102)
* Add more possibilities for ExpandString(). (r1101)
* Autoattach doesn't allow you adding an entry twice now. (r1100)
* Print a warning if PATH_MAX is undefined. (r1099)
* Use ExpandString() for CTCPReply. (r1096)
* Add Uptime command to *status. (r1095) (r1107)
* Make --makeconf clearer. (r1093)
* Add man pages for znc, znc-buildmod and znc-config. (r1091)
* Perl modules are no longer installed with executable bit set. (r1090)
* Crypt now forwards messages to other connected clients. (r1088)
* Fix a theoretical crash bug in the DNS resolving code. (r1087)
* Modules now get their module name as ident, not 'znc'. (r1084)
* Handle channel CTCPs the same way private CTCPs are handled. (r1082)
* Webadmin: Add support for timezone offset. (r1079)
* Webadmin: Remove the *.de webadmin skins. (r1078)
* Webadmin: Don't reset all channel settings when a user page is saved. (r1074)
* Fix a possible crash when rehashing failed to open the config file. (r1073)
* The instructions at the end of makeconf showed a wrong port. (r1072)
* Throttle DCC transfers to the speed of the sending side. (r1069)
* De-bashify znc-buildmod. (r1068)
* Time out unauthed clients after 60 secs. (r1067)
* Don't fail with conspire as IRC client. (r1066)
* Replace CString::Token() with a much faster version. (r1065)
* DenyLoadMod users no longer can use ListAvailMods. (r1063)
* Add a VERSION_EXTRA define which can be influenced via CXXFLAGS and which is appended to ZNC's version number. (r1062)

# ZNC 0.056 (2008-05-24)

* Rehashing also handles channel settings. (r1058)
* Make znc-buildmod work with prefixes. (r1054)
* Greatly speed up CUser::GetIRCSock(). Thanks to w00t. (r1053)
* Don't link the ZNC binary against libsasl2. (r1050)
* Make CString::RandomString() produce a more random string (this is used for autoop and increases its security). (r1047)
* Remove OnRehashDone() and add OnPreRehash() and OnPostRehash(). (r1046)
* Show traffic stats in a readable unit instead of lots of bytes. (r1038)
* Fixed a bug were nick changes where silently dropped if we are in no channels. (r1037)
* Remove the /watch command from watch. (r1035)
* znc-buildmod now reports errors via exit code. (r1034)
* Display a better error message if znc.conf cannot be opened. (r1033)
* Print a warning from *status if some message or notice is lost because we are not connected to IRC. (r1032)
* Make ./configure --bindir=DIR work. (r1031)
* Always track header dependencies. This means we require a compile supporting -MMD and -MF. (r1026)
* Improve some error messages if we can't connect to IRC. (r1023)
* Use \n instead of \r\n for znc.conf. (r1022)
* Fix some invalid replies from the shell module. (r1021)
* Support chans other than #chan and &chan. (r1019)
* Make chansaver add all channels to the config on load. (r1018)
* Reply to PINGs if we are not connected to a server. (r1016)
* Fix some bugs with Csocket, one caused segfaults when connecting to special SSL hosts. (r1015)
* Use MODFLAGS instead of CXXFLAGS for modules. Do MODFLAGS=something ./configure if you want a flag that is used only by modules. (r1012)
* Add OnTopic() module call. (r1011)
* Don't create empty .registry files for modules. See find ~/.znc -iname ".registry" -size 0 for a list of files you can delete. (r1010)
* Only allow admins to load the shell module. (r1007)
* Fix CModule::DelNV()'s return value. (r1006)
* Fix CUser::Clone() to handle all the settings. (r1005)
* Mark all our FDs as close-on-exec. (r1004)

# ZNC 0.054 (2008-04-01)

* Forward /names replies for unknown channels.
* Global modules can no longer hook into every config line, but only those prefixed with 'GM:'.
* Don't forward topic changes for detached channels.
* Remove ~/.znc/configs/backups and instead only keep one backup under znc.conf-backup.
* Update /msg *status help.
* Add --datadir to znc-config.
* Update bundled Csocket to the latest version. This fixes some bugs (e.g. not closing SSL connections properly).
* Use $HOME if possible to get the user's home (No need to read /etc/passwd anymore).
* Use -Wshadow and fix all those warnings.
* Add /msg *status ListAvailMods. Thanks to SilverLeo.
* Add OnRehashDone() module call.
* Add rehashing (SIGHUP and /msg *status rehash).
* Also write a pid file if we are compiled with --enable-debug. Thanks to SilverLeo.
* Add ClearVHost and 'fix' SetVHost. Thanks to SilverLeo.
* Increase the connect timeout for IRC connections to 2 mins.
* Add a user's vhost to the list on the user page in webadmin.
* Add --no-color switch and only use colors if we are on a terminal.
* Add DenySetVHost config option. Thanks to Veit Wahlich aka cru.
* Change --makeconf's default for KeepNick and KeepBuffer to false.
* Add simple_away module. This sets you away some time after you disconnect from ZNC.
* Don't write unneeded settings to the <Chan> section. Thanks to SilverLeo.
* Remove OnFinishedConfig() module call. Use OnBoot() instead.
* Fix some GCC 4.3 warnings. Thanks to darix again.
* Move the static data (webadmin's skins) to /usr/share/znc per default. Thanks to Marcus Rueckert aka darix.
* New znc-buildmod which works on shells other than bash.
* Add ClearAllChannelBuffers to *status.
* Handle CTCPs to *status.
* autoattach now saves and reloads its settings.
* Let webadmin use the user's defaults for new chans. Thanks to SilverLeo.

# ZNC 0.052 (2007-12-02)

* Added saslauth module.
* Add del command to autoattach.
* Make awaynick save its settings and restore them when it is loaded again.
* Added disconnect and connect commands to *status.
* CTCPReply = VERSION now ignores ctcp version requests (as long as no client is attached). This works for every CTCP request.
* Add -W to our default CXXFLAGS.
* Remove save command from perform, it wasn't needed.
* Add list command to stickychan.
* --with-module-prefix=x now really uses x and not x/znc (Inspired by CNU :) ).
* Use a dynamic select timeout (sleep until next cron runs). This should save some CPU time.
* Fix NAMESX / UHNAMES, round two (multi-client breakage).
* Module API change (without any breakage): OnLoad gets sMessage instead of sErrorMsg.
* Fix a mem-leak.
* Disable auto-rejoin on kick and add module kickrejoin.
* Respect $CXXFLAGS env var in configure.
* Removed some executable bits on graphiX' images.
* Added README file and removed docs/.
* Removed the antiidle module.
* Fixes for GCC 4.3 (Debian bug #417793).
* Some dead code / code duplications removed.
* Rewrote Makefile.ins and don't strip binaries anymore by default.

# ZNC 0.050 (2007-08-11)

* fixed UHNAMES bug (ident was messed up, wrong joins were sent)
* fixed /lusers bug (line was cached more than once)
* added disabled chans to the core
* send out a notice asking for the server password if client doesn't send one
* added ConnectDelay config option
* added timestamps on the backlog
* added some module calls
* added basic traffic stats
* added usermodes support
* API breakage (CModule::OnLoad got an extra param)
* added fixed channels to the partyline module
* fixed partyline bugs introduced by last item
* fixed a NULL pointer dereference if /nick command was received from a client while not connected to IRC
* added a JoinTries per-user config option which specifies how often we try to rejoin a channel (default: 0 -> unlimited)
* make configure fail if it can't find openssl (or perl, ...)
* new modules: antiidle, nickserv

# ZNC 0.047 (2007-05-15)

* NULL pointer dereference when a user uses webadmin while not on irc
* A logged in user could access any file with /msg *status send/get
* znc --makeconf now restarts znc correctly
* New webadmin skin (+ german translations)
* Updated to new Csocket version
* Allow @ and . in user names which now can also be longer
* Added crox and psychon to AUTHORS
* Relay messages to other clients of the current user (for the crypt module)
* Added chansaver Module
* Moved awaynick functionality into a module
* Added perform module from psychon
* fixed bug when compiling without module support
* Added a configurable Timer to the away module
* Added support for Topics in the partyline module
* Added support for reloading global modules
* Added a timer to ping inactive clients
* Migrated away from CString::ToString() in favor of explicit constructors
* IMAP Authentication Module added
* Fixed issues with gcc 4.1
* Added concept of default channels that a user is automatically joined to every time they attach
* Added SetVHost command
* Added error reporting and quit msgs as *status output
* Added a server ping for idle connections - Thanks zparta
* added -ldl fix for openssl crypto package. fixes static lib link requirement
* Explicitly set RTLD_LOCAL, some systems require it - thanks x-x
* Added SendBuffer and ClearBuffer client commands
* Added support for to talk unencrypted
* added with-modules-prefix and moved modules by default to PREFIX/libexec
* Added license and contact info
* remove compression initialization until standard has normalized a bit

# ZNC 0.045 (2006-02-20)

* Added +o/v -o/v for when users attach/detach - partyline module
* Changed internal naming of CUserSock to CClient for consistency
* Fixed some issues with older bsd boxes
* Added ListenHost for binding to a specific ip instead of inaddr_any
* Allow - and _ as valid username chars
* respect compiler, we don't force you to use g++ anymore, don't include system includes for deps
* Added Replace_n() and fixed internal loop bug in Replace() (thanks psycho for finding it)
* Don't allow .. in GET
* Added autoop module
* Added support for buffering of /me actions
* Added Template support in webadmin now you can write your own skins easily :)
* Added ipv6 support
* Support for multiple Listen Ports (note the config option "ListenPort" changed to "Listen")

# ZNC 0.044 (2005-10-14)

* Fixed issue where pipe between client and irc sockets would get out of sync, this was introduced in 0.043
* Added *status commands to list users and clients connected

# ZNC 0.043 (2005-10-13)

* Added Multi-Client support
* Added Global partyline module
* Added MOTD config option
* Added Admin permission
* Added SaveConfig admin-only *status command
* Added Broadcast admin-only *status command

# ZNC 0.041 (2005-09-08)

* This release fixes some issues with 64bit systems.

# ZNC 0.040 (2005-09-07)

This release contains a lot of features/bugfixes and a great new global module called admin.cpp which will allow you to add/remove/edit users and settings on the fly via a web browser.

# ZNC 0.039 (2005-09-07)

This release contains a lot of features/bugfixes and a great new global module called admin.cpp which will allow you to add/remove/edit users and settings on the fly via a web browser.

# ZNC 0.038 (2005-09-07)

This release contains a lot of bugfixes and a great new global module called admin.cpp which will allow you to add/remove/edit users and settings on the fly via a web browser.

# ZNC 0.037 (2005-05-22)

# ZNC 0.036 (2005-05-15)

# ZNC 0.035 (2005-05-14)

# ZNC 0.034 (2005-05-01)

# ZNC 0.033 (2005-04-26)

# ZNC 0.030 (2005-04-21)

# ZNC 0.029 (2005-04-12)

# ZNC 0.028 (2005-04-04)

# ZNC 0.027 (2005-04-04)

# ZNC 0.025 (2005-04-03)

# ZNC 0.023 (2005-03-10)
