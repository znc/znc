package SampleSock;
use base 'ZNCSocket';

sub OnConnect
{
	my ( $me ) = @_;
	ZNC::PutTarget( $me->{SamChan}, "I have FD [$me->{fd}]" );
}

sub OnReadLine
{
	my ( $me, $line ) = @_;
	$line =~ s/\r?\n$//;

	ZNC::PutTarget( $me->{SamChan}, "I have a line [$line]" );
	$me->Write( "WHAAAAA!?\n" );
}

sub OnSockDestroy
{
	my ( $me ) = @_;
	ZNC::PutTarget( $me->{SamChan}, "Ahhhh we be closed :(" );
}

sub OnSockDestroy
{
	ZNC::PutTarget( $me->{SamChan}, "destructor called, he wants his money back" );
}

sub OnNewSock
{
	my ( $me, $newfd ) = @_;
	ZNC::PutTarget( $me->{SamChan}, "new created fd $newfd" );
	my $obj = new SampleSock( $me->{modobj}, $newfd );
	$obj->{SamChan} = $me->{SamChan};
	$obj->AddSock();
}

1;

package ExampleWithSock;
use strict;

sub new
{
	my ( $classname ) = @_;
	my $self = {};

	bless( $self, $classname );

	print STDERR "WOOOF!\n";

	return( $self );
}

sub OnChanMsg
{
	my ( $me, $nick, $chan, $msg ) = @_;
	$nick =~ s/^(.+?)!.*/$1/;

	ZNC::PutTarget( $chan, "$nick, you said [$msg]?" );

	if ( $msg =~ /^go\s+(.+?)\s+(.+)/i )
	{
		my $obj = new SampleSock( $me );
		$obj->{SamChan} = $chan;
		if ( $obj->Connect( $1, $2, 10, 1 ) )
		{
			$obj->AddSock();
		}
	}
	elsif ( $msg =~ /^listen\s+([0-9]+)/ )
	{
		my $obj = new SampleSock( $me );
		$obj->{SamChan} = $chan;
		if ( $obj->Listen( $1, "", 1 ) )
		{
			$obj->AddSock();
		}
		else
		{
			ZNC::PutTarget( $chan, "Failed to setup listener!" );
		}
	}

	return( ZNC::CONTINUE );
}

sub OnShutdown
{
	print STDERR "HERE I AM!!!!!\n";
}

1;

