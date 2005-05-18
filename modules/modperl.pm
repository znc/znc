#
# TODO need to add socket support
# OnConnect( $sockhandle, $parentsockhandle )
# in the event this is comming in from a listener, the $sockhandle will be different from the others, so need to
# register it in the socket class like the others
# socket will register as $self = { "$sockhandle" => 1 }
#
# For Listeners
# OnConnectionFrom( $sockhandle, $remotehost, $remoteport )
#
# OnError( $sockhandle, $errno )
# OnConnectionRefused( $sockhandle )
# OnTimeout( $sockhandle )
# OnDisconnect( $sockhandle )
# OnData( $sockhandle, $bytes, $length )
# OnReadLine( $sockhandle, $bytes, $length )
# ZNC::WriteSock( $sockhandle, $bytes, $length )
# my $sockhandle = ZNC::ConnectSock( $host, $port, $timeout, $enablereadline )
# my $sockhandle = ZNC::ConnectSockSSL( $host, $port, $timeout, $enablereadline )
# my $sockhandle = ZNC::ListenSock( $port, $bindhostname, $enablereadline )
# my $sockhandle = ZNC::ListenSockSSL( $port, $bindhostname, $enablereadline )
#
# store the sockhandle in their class, before every action call 'TestSock', if its true then call the event
# otherwise close the socket

package ZNC;
use strict;

my @MODS = ();

sub COREEval
{
	my $arg = shift;
	eval $arg;
}

sub CORECallFunc
{
	my ( $Username, $Func, @args ) = @_;
	my $FinalRet = CONTINUE();

	foreach( @MODS )
	{
		next if ( $_->{ZNC_Username} ne $Username );

		if ( $_->can( $Func ) )
		{
			my $Ret = $_->$Func( @args );

			if ( $Ret == HALT() )
			{
				return( $Ret );
			}
			
			if ( $Ret == HALTMODS() )
			{
				return( $Ret );
			}
			
			if ( $Ret == HALTCORE() )
			{
				$FinalRet = $Ret;
			}
		}
	}

	return( $FinalRet );
}

sub CORELoadMod
{
	my ( $Username, $ModPath ) = @_;

	my $Module;
	if ( $ModPath =~ /\/([^\/.]+)\.pm/ )
	{
		$Module = $1;
	}

	if ( !$Module )
	{
		ZNC::PutModule( "Invalid Module requested!" );
		return( HALTMODS() );
	}

	my $DPath = GetString( "DataPath" );
	my $FileName = $DPath . "/." . $Username . $Module . ".pm";
	
	if ( !open( INMOD, $ModPath ) )
	{
		ZNC::PutModule( "Unable to open module $ModPath!" );
		return( HALTMODS() );
	}
	if ( !open( OUTMOD, ">$FileName" ) )
	{
		ZNC::PutModule( "Unable to write to Module cache $FileName!" );
		close( INMOD );
		return( HALTMODS() );
	}

	foreach( @MODS )
	{
		if ( ( $_->{ZNC_Username} eq $Username ) && ( $_->{ZNC_Name} eq $Module ) )
		{
			ZNC::PutModule( "$Module Already Loaded" );
			return( HALTMODS() );
		}
	}
	
	my $pkgcount = 0;
	while( <INMOD> )
	{
		if ( $_ =~ /^\s*package\s*.+;/ )
		{
			if ( $pkgcount > 0 )
			{
				ZNC::PutModule( "Only 1 package declaration per file!" );
				close( INMOD );
				close( OUTMOD );
				return( HALDMODS() );
			}
			print OUTMOD "package $Username$Module;\n";
			$pkgcount++;
		}
		else
		{
			print OUTMOD $_;
		}
	}
	close( INMOD );
	close( OUTMOD );

	require $FileName;

	my $NewMod = $Username . $Module;

	my $obj = new $NewMod();
	if ( !$obj )
	{
		ZNC::PutModule( "$Module Failed to load" );
		return( HALTMODS() );
	}
	
	$obj->{ZNC_Username} = $Username;
	$obj->{ZNC_Name} = $Module;
	$obj->{ZNC_ModPath} = $FileName;
	
	push( @MODS, $obj );
	ZNC::PutModule( "Loaded $Module" );
}

sub COREUnLoadMod
{
	my ( $Username, $Module ) = @_;

	$Module =~ s/(.+?)\.pm/$1/;

	if ( !$Module )
	{
		ZNC::PutModule( "Invalid Module requested!" );
		return( HALTMODS() );
	}

	for( my $i = 0; $i < @MODS; $i++ )
	{
		if ( ( $MODS[$i]->{ZNC_Username} eq $Username ) && ( $MODS[$i]->{ZNC_Name} eq $Module ) )
		{
			undef $MODS[$i];
			splice( @MODS, $i );
			ZNC::PutModule( "UnLoaded $Module" );
			return( CONTINUE() );
		}
	}
	ZNC::PutModule( "$Module Isn't Loaded" );

}

# the timer is inserted, it just calls this guy and expects it to be here
sub CORECallTimer
{
	my ( $Username, $Func, $ModName ) = @_;

	for( my $i = 0; $i < @MODS; $i++ )
	{
		if ( ( $MODS[$i]->{ZNC_Username} eq $Username ) && ( $MODS[$i]->{ZNC_Name} eq $ModName ) )
		{
			return( $MODS[$i]->$Func() );
		}
	}
	return( HALTMODS() );
}

######################
# Ease Of use USER functions
sub AddTimer
{
	my ( $mod, $funcname, $description, $interval, $cycles ) = @_;

	COREAddTimer( $mod->{ZNC_Name}, $funcname, $description, $interval, $cycles );
}

sub RemTimer
{
	my ( $mod, $funcname ) = @_;
	CORERemTimer( $mod->{ZNC_Name}, $funcname );
}

sub PutModule
{
	my ( $line, $ident, $host ) = @_;

	if ( !$ident )
	{
		$ident = "znc";
	}
	if ( !$host )
	{
		$host = "znc.com";
	}

	COREPutModule( "Module", $line, $ident, $host );
}

sub PutModNotice
{
	my ( $line, $ident, $host ) = @_;

	if ( !$ident )
	{
		$ident = "znc";
	}
	if ( !$host )
	{
		$host = "znc.com";
	}

	COREPutModule( "ModNotice", $line, $ident, $host );
}

sub PutIRC
{
	my ( $line ) = @_;
	COREPuts( "IRC", $line );
}

sub PutStatus
{
	my ( $line ) = @_;
	COREPuts( "Status", $line );
}

sub PutUser
{
	my ( $line ) = @_;
	COREPuts( "User", $line );
}

sub PutTarget
{
	my ( $target, $line ) = @_;
	PutIRC( "PRIVMSG $target :$line" );
}
	
1;








