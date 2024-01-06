#
# Copyright (C) 2004-2024 ZNC, see the NOTICE file for details.
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
use strict;
use warnings;

package perleval;
use base 'ZNC::Module';

sub description {
	shift->t_s('Evaluates perl code')
}

sub wiki_page {
	'perleval'
}

sub OnLoad {
	my $self = shift;
	if (!$self->GetUser->IsAdmin) {
		$_[1] = $self->t_s('Only admin can load this module');
		return 0
	}
	return 1
}

sub OnModCommand {
	my $self = shift;
	my $cmd = shift;
	my $x = eval $cmd;
	if ($@) {
		$self->PutModule($self->t_f('Error: %s')->($@));
	} else {
		$self->PutModule($self->t_f('Result: %s')->($x));
	}
}

1
