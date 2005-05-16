#
# TODO need to add timer support
# TODO need to add socket support
#

package ZNC;
use strict;

my %Modules;

sub Eval
{
	my $arg = shift;
	eval $arg;
}

sub CallFunc
{
	my ( $Func, @args ) = @_;
	my $FinalRet = CONTINUE();

	foreach( keys( %Modules ) )
	{
		if ( $Modules{$_}->{$Func} )
		{
			my $Ret = $Modules{$_}->$Func( @args );

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

sub LoadMod
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

	if ( $Modules{"$Username|$Module"} )
	{
		ZNC::PutModule( "$Module Already Loaded" );
		return( HALTMODS() );
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
	
	$Modules{"$Username|$Module"} = $obj;	
	ZNC::PutModule( "Loaded $Module" );
}

sub UnLoadMod
{
	my ( $Username, $Module ) = @_;

	$Module =~ s/(.+?)\.pm/$1/;

	if ( !$Module )
	{
		ZNC::PutModule( "Invalid Module requested!" );
		return( HALTMODS() );
	}

	if ( !$Modules{"$Username|$Module"} )
	{
		ZNC::PutModule( "$Module Isn't Loaded" );
		return( HALTMODS() );
	}

	undef $Modules{"$Username|$Module"};
	ZNC::PutModule( "UnLoaded $Module" );
}

1;








