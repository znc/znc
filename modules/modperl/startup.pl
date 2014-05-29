#
# Copyright (C) 2004-2014 ZNC, see the NOTICE file for details.
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

use 5.010;
use strict;
use warnings;
use ZNC;
use IO::File;
use feature 'switch', 'say';

package ZNC::Core;

my %modrefcount;
my @allmods;

sub UnloadModule {
	my ($pmod) = @_;
	my @newallmods = grep {$pmod != $_} @allmods;
	if ($#allmods == $#newallmods) {
		return 0
	}
	@allmods = @newallmods;
	$pmod->OnShutdown;
	my $cmod = $pmod->{_cmod};
	my $modpath = $cmod->GetModPath;
	my $modname = $cmod->GetModName;
	given ($cmod->GetType()) {
		when ($ZNC::CModInfo::NetworkModule) {
			$cmod->GetNetwork->GetModules->removeModule($cmod);
		}
		when ($ZNC::CModInfo::UserModule) {
			$cmod->GetUser->GetModules->removeModule($cmod);
		}
		when ($ZNC::CModInfo::GlobalModule) {
			ZNC::CZNC::Get()->GetModules->removeModule($cmod);
		}
	}
	delete $pmod->{_cmod};
	delete $pmod->{_nv};
	unless (--$modrefcount{$modname}) {
		say "Unloading $modpath from perl";
		ZNC::_CleanupStash($modname);
		delete $INC{$modpath};
	}
	return 1
	# here $cmod is deleted by perl (using DESTROY)
}

sub UnloadAll {
	while (@allmods) {
		UnloadModule($allmods[0]);
	}
}

sub IsModule {
	my $path = shift;
	my $modname = shift;
	my $f = IO::File->new($path);
	grep {/package\s+$modname\s*;/} <$f>;
}

sub LoadModule {
	my ($modname, $args, $type, $user, $network) = @_;
	$modname =~ /^\w+$/ or return ($ZNC::Perl_LoadError, "Module names can only contain letters, numbers and underscores, [$modname] is invalid.");
	my $container;
	given ($type) {
		when ($ZNC::CModInfo::NetworkModule) {
			$container = $network;
		}
		when ($ZNC::CModInfo::UserModule) {
			$container = $user;
		}
		when ($ZNC::CModInfo::GlobalModule) {
			$container = ZNC::CZNC::Get();
		}
	}
	return ($ZNC::Perl_LoadError, "Uhm? No container for the module? Wtf?") unless $container;
	$container = $container->GetModules;
	return ($ZNC::Perl_LoadError, "Module [$modname] already loaded.") if defined $container->FindModule($modname);
	my $modpath = ZNC::String->new;
	my $datapath = ZNC::String->new;
	ZNC::CModules::FindModPath("$modname.pm", $modpath, $datapath) or return ($ZNC::Perl_NotFound);
	$modpath = $modpath->GetPerlStr;
	return ($ZNC::Perl_LoadError, "Incorrect perl module [$modpath]") unless IsModule $modpath, $modname;
	my $pmod;
	my @types = eval {
		require $modpath;
		$pmod = bless {}, $modname;
		$pmod->module_types();
	};
	if ($@) {
		# modrefcount was 0 before this, otherwise it couldn't die in the previous time.
		# so can safely remove module from %INC
		delete $INC{$modpath};
		die $@;
	}
	return ($ZNC::Perl_LoadError, "Module [$modname] doesn't support the specified type.") unless $type ~~ @types;
	$modrefcount{$modname}++;
	$datapath = $datapath->GetPerlStr;
	$datapath =~ s/\.pm$//;
	my $cmod = ZNC::CPerlModule->new($user, $network, $modname, $datapath, $pmod);
	my %nv;
	tie %nv, 'ZNC::ModuleNV', $cmod;
	$pmod->{_cmod} = $cmod;
	$pmod->{_nv} = \%nv;
	$cmod->SetDescription($pmod->description);
	$cmod->SetArgs($args);
	$cmod->SetModPath($modpath);
	$cmod->SetType($type);
	push @allmods, $pmod;
	$container->push_back($cmod);
	my $x = '';
	my $loaded = 0;
	eval {
		$loaded = $pmod->OnLoad($args, $x);
	};
	if ($@) {
		$x .= ' ' if '' ne $x;
		$x .= $@;
	}
	if (!$loaded) {
		UnloadModule $pmod;
		if ($x) {
			return ($ZNC::Perl_LoadError, "Module [$modname] aborted: $x");
		}
		return ($ZNC::Perl_LoadError, "Module [$modname] aborted.");
	}
	if ($x) {
		return ($ZNC::Perl_Loaded, "[$x] [$modpath]");
	}
	return ($ZNC::Perl_Loaded, "[$modpath]")
}

sub GetModInfo {
	my ($modname, $modinfo) = @_;
	$modname =~ /^\w+$/ or return ($ZNC::Perl_LoadError, "Module names can only contain letters, numbers and underscores, [$modname] is invalid.");
	my $modpath = ZNC::String->new;
	my $datapath = ZNC::String->new;
	ZNC::CModules::FindModPath("$modname.pm", $modpath, $datapath) or return ($ZNC::Perl_NotFound, "Unable to find module [$modname]");
	$modpath = $modpath->GetPerlStr;
	return ($ZNC::Perl_LoadError, "Incorrect perl module.") unless IsModule $modpath, $modname;
	ModInfoByPath($modpath, $modname, $modinfo);
	return ($ZNC::Perl_Loaded)
}

sub ModInfoByPath {
	my ($modpath, $modname, $modinfo) = @_;
	die "Incorrect perl module." unless IsModule $modpath, $modname;
	require $modpath;
	my $pmod = bless {}, $modname;
	my @types = $pmod->module_types;
	$modinfo->SetDefaultType($types[0]);
	$modinfo->SetDescription($pmod->description);
	$modinfo->SetWikiPage($pmod->wiki_page);
	$modinfo->SetArgsHelpText($pmod->args_help_text);
	$modinfo->SetHasArgs($pmod->has_args);
	$modinfo->SetName($modname);
	$modinfo->SetPath($modpath);
	$modinfo->AddType($_) for @types;
	unless ($modrefcount{$modname}) {
		say "Unloading $modpath from perl, because it's not loaded as a module";
		ZNC::_CleanupStash($modname);
		delete $INC{$modpath};
	}
}

sub CallModFunc {
	my $pmod = shift;
	my $func = shift;
	my $default = shift;
	my @arg = @_;
	my $res = $pmod->$func(@arg);
#	print "Returned from $func(@_): $res, (@arg)\n";
	unless (defined $res) {
		$res = $default if defined $default;
	}
	($res, @arg)
}

sub CallTimer {
	my $timer = shift;
	$timer->RunJob;
}

sub CallSocket {
	my $socket = shift;
	my $func = shift;
	say "Calling socket $func";
	$socket->$func(@_)
}

sub RemoveTimer {
	my $timer = shift;
	$timer->OnShutdown;
}

sub RemoveSocket {
	my $socket = shift;
	$socket->OnShutdown;
}

package ZNC::ModuleNV;

sub TIEHASH {
	my $name = shift;
	my $cmod = shift;
	bless {cmod=>$cmod, last=>-1}, $name
}

sub FETCH {
	my $self = shift;
	my $key = shift;
	return $self->{cmod}->GetNV($key) if $self->{cmod}->ExistsNV($key);
	return undef
}

sub STORE {
	my $self = shift;
	my $key = shift;
	my $value = shift;
	$self->{cmod}->SetNV($key, $value);
}

sub DELETE {
	my $self = shift;
	my $key = shift;
	$self->{cmod}->DelNV($key);
}

sub CLEAR {
	my $self = shift;
	$self->{cmod}->ClearNV;
}

sub EXISTS {
	my $self = shift;
	my $key = shift;
	$self->{cmod}->ExistsNV($key)
}

sub FIRSTKEY {
	my $self = shift;
	my @keys = $self->{cmod}->GetNVKeys;
	$self->{last} = 0;
	return $keys[0];
	return undef;
}

sub NEXTKEY {
	my $self = shift;
	my $last = shift;
	my @keys = $self->{cmod}->GetNVKeys;
	if ($#keys < $self->{last}) {
		$self->{last} = -1;
		return undef
	}
	# Probably caller called delete on last key?
	if ($last eq $keys[$self->{last}]) {
		$self->{last}++
	}
	if ($#keys < $self->{last}) {
		$self->{last} = -1;
		return undef
	}
	return $keys[$self->{last}]
}

sub SCALAR {
	my $self = shift;
	my @keys = $self->{cmod}->GetNVKeys;
	return $#keys + 1
}

package ZNC::Module;

sub description {
	"< Placeholder for a description >"
}

sub wiki_page {
	''
}

sub module_types {
	$ZNC::CModInfo::NetworkModule
}

sub args_help_text { '' }

sub has_args { 0 }


# Default implementations for module hooks. They can be overriden in derived modules.
sub OnLoad {1}
sub OnBoot {}
sub OnShutdown {}
sub WebRequiresLogin {}
sub WebRequiresAdmin {}
sub GetWebMenuTitle {}
sub OnWebPreRequest {}
sub OnWebRequest {}
sub GetSubPages {}
sub _GetSubPages { my $self = shift; $self->GetSubPages }
sub OnPreRehash {}
sub OnPostRehash {}
sub OnIRCDisconnected {}
sub OnIRCConnected {}
sub OnIRCConnecting {}
sub OnIRCConnectionError {}
sub OnIRCRegistration {}
sub OnBroadcast {}
sub OnChanPermission {}
sub OnOp {}
sub OnDeop {}
sub OnVoice {}
sub OnDevoice {}
sub OnMode {}
sub OnRawMode {}
sub OnRaw {}
sub OnStatusCommand {}
sub OnModCommand {}
sub OnModNotice {}
sub OnModCTCP {}
sub OnQuit {}
sub OnNick {}
sub OnKick {}
sub OnJoining {}
sub OnJoin {}
sub OnPart {}
sub OnChanBufferStarting {}
sub OnChanBufferEnding {}
sub OnChanBufferPlayLine {}
sub OnPrivBufferPlayLine {}
sub OnClientLogin {}
sub OnClientDisconnect {}
sub OnUserRaw {}
sub OnUserCTCPReply {}
sub OnUserCTCP {}
sub OnUserAction {}
sub OnUserMsg {}
sub OnUserNotice {}
sub OnUserJoin {}
sub OnUserPart {}
sub OnUserTopic {}
sub OnUserTopicRequest {}
sub OnCTCPReply {}
sub OnPrivCTCP {}
sub OnChanCTCP {}
sub OnPrivAction {}
sub OnChanAction {}
sub OnPrivMsg {}
sub OnChanMsg {}
sub OnPrivNotice {}
sub OnChanNotice {}
sub OnTopic {}
sub OnServerCapAvailable {}
sub OnServerCapResult {}
sub OnTimerAutoJoin {}
sub OnEmbeddedWebRequest {}
sub OnAddNetwork {}
sub OnDeleteNetwork {}
sub OnSendToClient {}
sub OnSendToIRC {}

# In Perl "undefined" is allowed value, so perl modules may continue using OnMode and not OnMode2
sub OnChanPermission2 { my $self = shift; $self->OnChanPermission(@_) }
sub OnOp2 { my $self = shift; $self->OnOp(@_) }
sub OnDeop2 { my $self = shift; $self->OnDeop(@_) }
sub OnVoice2 { my $self = shift; $self->OnVoice(@_) }
sub OnDevoice2 { my $self = shift; $self->OnDevoice(@_) }
sub OnMode2 { my $self = shift; $self->OnMode(@_) }
sub OnRawMode2 { my $self = shift; $self->OnRawMode(@_) }


# Functions of CModule will be usable from perl modules.
our $AUTOLOAD;

sub AUTOLOAD {
	my $name = $AUTOLOAD;
	$name =~ s/^.*:://; # Strip fully-qualified portion.
	my $sub = sub {
		my $self = shift;
		$self->{_cmod}->$name(@_)
	};
	no strict 'refs';
	*{$AUTOLOAD} = $sub;
	use strict 'refs';
	goto &{$sub};
}

sub DESTROY {}

sub BeginNV {
	die "Don't use BeginNV from perl modules, use GetNVKeys or NV instead!";
}
sub EndNV {
	die "Don't use EndNV from perl modules, use GetNVKeys or NV instead!";
}
sub FindNV {
	die "Don't use FindNV from perl modules, use GetNVKeys/ExistsNV or NV instead!";
}

sub NV {
	my $self = shift;
	$self->{_nv}
}

sub CreateTimer {
	my $self = shift;
	my %a = @_;
	my $ptimer = {};
	my $ctimer = ZNC::CreatePerlTimer(
			$self->{_cmod},
			$a{interval}//10,
			$a{cycles}//1,
			"perl-timer",
			$a{description}//'Just Another Perl Timer',
			$ptimer);
	$ptimer->{_ctimer} = $ctimer;
	if (ref($a{task}) eq 'CODE') {
		bless $ptimer, 'ZNC::Timer';
		$ptimer->{job} = $a{task};
		$ptimer->{context} = $a{context};
	} else {
		bless $ptimer, $a{task};
	}
	$ptimer;
}

sub CreateSocket {
	my $self = shift;
	my $class = shift;
	my $psock = bless {}, $class;
	my $csock = ZNC::CreatePerlSocket($self->{_cmod}, $psock);
	$psock->{_csock} = $csock;
	$psock->Init(@_);
	$psock;
}

package ZNC::Timer;

sub GetModule {
	my $self = shift;
	ZNC::AsPerlModule($self->{_ctimer}->GetModule)->GetPerlObj()
}

sub RunJob {
	my $self = shift;
	if (ref($self->{job}) eq 'CODE') {
		&{$self->{job}}($self->GetModule, context=>$self->{context}, timer=>$self->{_ctimer});
	}
}

sub OnShutdown {}

our $AUTOLOAD;

sub AUTOLOAD {
	my $name = $AUTOLOAD;
	$name =~ s/^.*:://; # Strip fully-qualified portion.
	my $sub = sub {
		my $self = shift;
		$self->{_ctimer}->$name(@_)
	};
	no strict 'refs';
	*{$AUTOLOAD} = $sub;
	use strict 'refs';
	goto &{$sub};
}

sub DESTROY {}


package ZNC::Socket;

sub GetModule {
	my $self = shift;
	ZNC::AsPerlModule($self->{_csock}->GetModule)->GetPerlObj()
}

sub Init {}
sub OnConnected {}
sub OnDisconnected {}
sub OnTimeout {}
sub OnConnectionRefused {}
sub OnReadData {}
sub OnReadLine {}
sub OnAccepted {}
sub OnShutdown {}

sub _Accepted {
	my $self = shift;
	my $psock = $self->OnAccepted(@_);
	return $psock->{_csock} if defined $psock;
	return undef;
}

our $AUTOLOAD;

sub AUTOLOAD {
	my $name = $AUTOLOAD;
	$name =~ s/^.*:://; # Strip fully-qualified portion.
	my $sub = sub {
		my $self = shift;
		$self->{_csock}->$name(@_)
	};
	no strict 'refs';
	*{$AUTOLOAD} = $sub;
	use strict 'refs';
	goto &{$sub};
}

sub DESTROY {}

sub Connect {
	my $self = shift;
	my $host = shift;
	my $port = shift;
	my %arg = @_;
	$self->GetModule->GetManager->Connect(
			$host,
			$port,
			"perl-socket",
			$arg{timeout}//60,
			$arg{ssl}//0,
			$arg{bindhost}//'',
			$self->{_csock}
			);
}

sub Listen {
	my $self = shift;
	my %arg = @_;
	my $addrtype = $ZNC::ADDR_ALL;
	if (defined $arg{addrtype}) {
		given ($arg{addrtype}) {
			when (/^ipv4$/i) { $addrtype = $ZNC::ADDR_IPV4ONLY }
			when (/^ipv6$/i) { $addrtype = $ZNC::ADDR_IPV6ONLY }
			when (/^all$/i)  { }
			default { die "Specified addrtype [$arg{addrtype}] isn't supported" }
		}
	}
	if (defined $arg{port}) {
		return $arg{port} if $self->GetModule->GetManager->ListenHost(
				$arg{port},
				"perl-socket",
				$arg{bindhost}//'',
				$arg{ssl}//0,
				$arg{maxconns}//ZNC::_GetSOMAXCONN,
				$self->{_csock},
				$arg{timeout}//0,
				$addrtype
				);
		return 0;
	}
	$self->GetModule->GetManager->ListenRand(
			"perl-socket",
			$arg{bindhost}//'',
			$arg{ssl}//0,
			$arg{maxconns}//ZNC::_GetSOMAXCONN,
			$self->{_csock},
			$arg{timeout}//0,
			$addrtype
			);
}

1
