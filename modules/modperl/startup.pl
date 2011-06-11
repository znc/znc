#
# Copyright (C) 2004-2011  See the AUTHORS file for details.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#

use 5.010;
use strict;
use warnings;
use ZNC;
use IO::File;
use feature 'switch', 'say';

package ZNC::Core;

my $uuidtype;
my $uuidgen;
our %pmods;
my %modrefcount;

sub Init {
	if (eval { require Data::UUID }) {
		$uuidtype = 'Data::UUID';
		$uuidgen = new Data::UUID;
	} elsif (eval { require UUID }) {
		$uuidtype = 'UUID';
	} else {
		$uuidtype = 'int';
		$uuidgen = 0;
	}
}

sub CreateUUID {
	my $res;
	given ($uuidtype) {
		when ('Data::UUID') {
			$res = $uuidgen->create_str;
		}
		when ('UUID') {
			my ($uuid, $str);
			UUID::generate($uuid);
			UUID::unparse($uuid, $res);
		}
		when ('int') {
			$uuidgen++;
			$res = "$uuidgen";
		}
	}
	say "Created new UUID for modperl with '$uuidtype': $res";
	return $res;
}

sub unloadByIDUser {
	my ($id, $user) = @_;
	my $modpath = $pmods{$id}{_cmod}->GetModPath;
	my $modname = $pmods{$id}{_cmod}->GetModName;
	$pmods{$id}->OnShutdown;
	$user->GetModules->removeModule($pmods{$id}{_cmod});
	delete $pmods{$id}{_cmod};# Just for the case
	delete $pmods{$id}{_nv};
	delete $pmods{$id}{_ptimers};
	delete $pmods{$id}{_sockets};
	delete $pmods{$id};
	unless (--$modrefcount{$modname}) {
		say "Unloading $modpath from perl";
		ZNC::_CleanupStash($modname);
		delete $INC{$modpath};
	}
}

sub UnloadModule {
	my ($cmod) = @_;
	unloadByIDUser($cmod->GetPerlID, $cmod->GetUser);
}

sub UnloadAll {
	while (my ($id, $pmod) = each %pmods) {
		unloadByIDUser($id, $pmod->{_cmod}->GetUser);
	}
}

sub IsModule {
	my $path = shift;
	my $modname = shift;
	my $f = IO::File->new($path);
	grep {/package\s+$modname\s*;/} <$f>;
}

sub LoadModule {
	my ($modname, $args, $user) = @_;
	$modname =~ /^\w+$/ or return ($ZNC::Perl_LoadError, "Module names can only contain letters, numbers and underscores, [$modname] is invalid.");
	return ($ZNC::Perl_LoadError, "Module [$modname] already loaded.") if defined $user->GetModules->FindModule($modname);
	my $modpath = ZNC::String->new;
	my $datapath = ZNC::String->new;
	ZNC::CModules::FindModPath("$modname.pm", $modpath, $datapath) or return ($ZNC::Perl_NotFound);
	$modpath = $modpath->GetPerlStr;
	return ($ZNC::Perl_LoadError, "Incorrect perl module [$modpath]") unless IsModule $modpath, $modname;
	eval {
		require $modpath;
	};
	if ($@) {
		# modrefcount was 0 before this, otherwise it couldn't die.
		# so can safely remove module from %INC
		delete $INC{$modpath};
		die $@;
	}
	$modrefcount{$modname}++;
	my $id = CreateUUID;
	$datapath = $datapath->GetPerlStr;
	$datapath =~ s/\.pm$//;
	my $cmod = ZNC::CPerlModule->new($user, $modname, $datapath, $id);
	my %nv;
	tie %nv, 'ZNC::ModuleNV', $cmod;
	my $pmod = bless {
		_cmod=>$cmod,
		_nv=>\%nv
	}, $modname;
	$cmod->SetDescription($pmod->description);
	$cmod->SetArgs($args);
	$cmod->SetModPath($modpath);
	$pmods{$id} = $pmod;
	$user->GetModules->push_back($cmod);
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
		unloadByIDUser($id, $user);
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
	my ($modname) = @_;
	$modname =~ /^\w+$/ or return ($ZNC::Perl_LoadError, "Module names can only contain letters, numbers and underscores, [$modname] is invalid.");
	my $modpath = ZNC::String->new;
	my $datapath = ZNC::String->new;
	ZNC::CModules::FindModPath("$modname.pm", $modpath, $datapath) or return ($ZNC::Perl_NotFound, "Unable to find module [$modname]");
	$modpath = $modpath->GetPerlStr;
	return ($ZNC::Perl_LoadError, "Incorrect perl module.") unless IsModule $modpath, $modname;
	require $modpath;
	my $pmod = bless {}, $modname;
	return ($ZNC::Perl_Loaded, $modpath, $pmod->description, $pmod->wiki_page)
}

sub ModInfoByPath {
	my ($modpath, $modname) = @_;
	die "Incorrect perl module." unless IsModule $modpath, $modname;
	require $modpath;
	my $pmod = bless {}, $modname;
	return ($pmod->description, $pmod->wiki_page)
}

sub CallModFunc {
	my $id = shift;
	my $func = shift;
	my $default = shift;
	my @arg = @_;
	my $res = $pmods{$id}->$func(@arg);
#	print "Returned from $func(@_): $res, (@arg)\n";
	unless (defined $res) {
		$res = $default if defined $default;
	}
	($res, @arg)
}

sub CallTimer {
	my $modid = shift;
	my $timerid = shift;
	$pmods{$modid}->_CallTimer($timerid)
}

sub CallSocket {
	my $modid = shift;
	$pmods{$modid}->_CallSocket(@_)
}

sub RemoveTimer {
	my $modid = shift;
	my $timerid = shift;
	$pmods{$modid}->_RemoveTimer($timerid)
}

sub RemoveSocket {
	my $modid = shift;
	my $sockid = shift;
	$pmods{$modid}->_RemoveSocket($sockid)
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
	my $id = ZNC::Core::CreateUUID;
	my %a = @_;
	my $ctimer = ZNC::CreatePerlTimer(
			$self->{_cmod},
			$a{interval}//10,
			$a{cycles}//1,
			"perl-timer-$id",
			$a{description}//'Just Another Perl Timer',
			$id);
	my $ptimer = {
		_ctimer=>$ctimer,
		_modid=>$self->GetPerlID
	};
	$self->{_ptimers}{$id} = $ptimer;
	if (ref($a{task}) eq 'CODE') {
		bless $ptimer, 'ZNC::Timer';
		$ptimer->{job} = $a{task};
		$ptimer->{context} = $a{context};
	} else {
		bless $ptimer, $a{task};
	}
	$ptimer;
}

sub _CallTimer {
	my $self = shift;
	my $id = shift;
	my $t = $self->{_ptimers}{$id};
	$t->RunJob;
}

sub _RemoveTimer {
	my $self = shift;
	my $id = shift;
	say "Removing perl timer $id";
	$self->{_ptimers}{$id}->OnShutdown;
	delete $self->{_ptimers}{$id}
}

sub CreateSocket {
	my $self = shift;
	my $class = shift;
	my $id = ZNC::Core::CreateUUID;
	my $csock = ZNC::CreatePerlSocket($self->{_cmod}, $id);
	my $psock = bless {
		_csock=>$csock,
		_modid=>$self->GetPerlID
	}, $class;
	$self->{_sockets}{$id} = $psock;
	$psock->Init(@_);
	$psock;
}

sub _CallSocket {
	my $self = shift;
	my $id = shift;
	my $func = shift;
	$self->{_sockets}{$id}->$func(@_)
}

sub _RemoveSocket {
	my $self = shift;
	my $id = shift;
	say "Removing perl socket $id";
	$self->{_sockets}{$id}->OnShutdown;
	delete $self->{_sockets}{$id}
}

package ZNC::Timer;

sub GetModule {
	my $self = shift;
	$ZNC::Core::pmods{$self->{_modid}};
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
	$ZNC::Core::pmods{$self->{_modid}};
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
			"perl-socket-".$self->GetPerlID,
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
				"perl-socket-".$self->GetPerlID,
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
			"perl-socket-".$self->GetPerlID,
			$arg{bindhost}//'',
			$arg{ssl}//0,
			$arg{maxconns}//ZNC::_GetSOMAXCONN,
			$self->{_csock},
			$arg{timeout}//0,
			$addrtype
			);
}

1
