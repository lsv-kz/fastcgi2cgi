#!/usr/bin/perl

use strict;
use warnings;
use POSIX;
use File::Basename;
use URI::Escape;

my $time = localtime;

my $meth = $ENV{'REQUEST_METHOD'};
if (!defined($meth))
{
    $meth = '------';
}

my $data = '';
if (($meth =~ /POST/))
{
    $data = uri_unescape(<STDIN>);
#    my $length = $ENV{CONTENT_LENGTH};
#    read STDIN, $data, $length;
#    $data = uri_unescape($data);
}

print "Content-type: text/html; charset=utf-8\n\n";
print "<!DOCTYPE html>
<html>
<head>
<title>Environment Dumper </title>
</head>
<body>
<center>
<table border=1>\n";
foreach (sort keys %ENV)
{
    print "<tr><td>$_</td><td>$ENV{$_}</td></tr>\n";
}
print "</table>
<p>$data</p>
<form action=\"env.pl\" method=\"$meth\">
<input type=\"hidden\" name=\"name\" value=\".-./. .+.!.?.,.~.#.&.>.<.^.\">
<input type=\"submit\" value=\"Get ENV\">
</form>
</body>
</html>";

