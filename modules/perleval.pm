use strict;
use warnings;

package perleval;
use base 'ZNC::Module';

sub description {
	'Evaluates perl code'
}

sub wiki_page {
	'perleval'
}

sub OnLoad {
	my $self = shift;
	if (!$self->GetUser->IsAdmin) {
		$_[1] = 'Only admin can load this module';
		return 0
	}
	return 1
}

sub OnModCommand {
	my $self = shift;
	my $cmd = shift;
	my $x = eval $cmd;
	if ($@) {
		$self->PutModule("Error: $@")
	} else {
		$self->PutModule("Result: $x")
	}
}

1
