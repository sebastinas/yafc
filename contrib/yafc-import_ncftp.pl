#!/usr/bin/perl -w
#
# A simple ncftp -> yafc bookmark converter by Mike Wayne (arc@cts.com)
# Sorry if my code sucks!
#
# Example usage:
# yafc-import_ncftp.pl ~/.ncftp/bookmarks >> ~/.yafc/bookmarks
#
# If a filename argument is not given, ~/.ncftp/bookmarks will be read.

use strict;
my $mime;

# set these to their appropriate settings.
my $anonpass	= 'user@host.com';			# anonymous password
my $ncftpbm	= "$ENV{HOME}/.ncftp/bookmarks";	# ncftp bookmarks

eval 'use MIME::Base64';
if ($@ eq '') { $mime = 1 }

chomp($anonpass = encode_base64($anonpass)) if $mime;
$ncftpbm = $ARGV[0] if $ARGV[0];

open(BOOKMARKS, $ncftpbm) or die "Unable to open ncftp bookmarks file $ncftpbm: $!\n";

print <<NOTICE;
# this is an automagically created file
# so don't bother placing comments here, they will be overwritten
# make sure this file isn't world readable if passwords are stored here

NOTICE

while (<BOOKMARKS>)
{
	if ($_ =~ /,/)
	{
		my @bookmark = split(/,/, $_);

		$bookmark[2] ||= 'anonymous';
		$bookmark[7] ||= 21;
	
		print "machine $bookmark[1]:$bookmark[7] alias '$bookmark[0]'\n  login ";

		if ($bookmark[2] eq 'anonymous')
		{
			print 'anonymous password ';
			print "[base64]" if $mime;
			print $anonpass;
		}

		else
		{
			print $bookmark[2];

			$bookmark[3] =~ s/^\*encoded\*/\[base64\]/ if $bookmark[3];
			print " password $bookmark[3]" if $bookmark[3];
		}

		$bookmark[5] = "\"$bookmark[5]\"" if ($bookmark[5] =~ m/ /);
		print " cwd $bookmark[5]" if ($bookmark[5] && $bookmark[5] ne '/');

		print "\n\n";
	}
}

print "# end of bookmark file\n";

close(BOOKMARKS);
