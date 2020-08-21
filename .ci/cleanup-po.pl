#!/usr/bin/env perl

use strict;
use warnings;
use v5.10;

local $/;
my $text = <>;

if ($ENV{MSGFILTER_MSGID}) {
	print $text;
} else {
	for (split(/^/, $text)) {
		next if /^PO-Revision-Date:/;
		s/^Last-Translator: \K.*/Various people/;
		print;
	}
}
