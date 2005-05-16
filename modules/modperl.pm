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

# TODO need to do module caching to protect name spaces!!!!

	my $DPath = GetString( "DataPath" );
	my $FileName = $DPath . "/" . $Username . $Module . ".pm";
	
	
	if ( $Modules{"$Username|$Module"} )
	{
		ZNC::PutModule( "$Module Already Loaded" );
		return( HALTMODS() );
	}
	
	require $ModPath;

	my $obj = new $Module();
	if ( !$obj )
	{
		ZNC::PutModule( "$Module Failed to load" );
		return( HALTMODS() );
	}
	
	$obj->{ZNC_Username} = $Username;
	$obj->{ZNC_Name} = $Module;
	
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








