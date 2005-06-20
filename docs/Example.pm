package Example;
use strict;

#
# Create a constructor for the module
sub new
{
	my ( $classname ) = @_;
	my $self = {};

	bless( $self, $classname );

	return( $self );
}

#
# we're going to hook this call back
sub OnChanMsg
{
	my ( $me, $nick, $chan, $msg ) = @_;
	ZNC::PutTarget( $chan, "WTF!?, you said [$msg]\n" );
	return( ZNC::CONTINUE );
}

sub OnShutdown
{
	my ( $me ) = @_;
}

1;

