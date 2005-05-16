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
	my ( $Username, $Module, $ModPath ) = @_;

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

	if ( !$Modules{"$Username|$Module"} )
	{
		ZNC::PutModule( "$Module Isn't Loaded" );
		return( HALTMODS() );
	}

	undef $Modules{"$Username|$Module"};
	ZNC::PutModule( "UnLoaded $Module" );
}

1;








