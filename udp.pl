#!/usr/bin/perl -w

use strict;
use IO::Socket;
use bytes;

BEGIN {
  $|++
}

my($sock, $recvd, $client, $MAXLEN, $PORTNO);

$MAXLEN = 4096;
$PORTNO = 54321;

$sock = IO::Socket::INET->new(LocalPort => $PORTNO, Proto => 'udp')
  or die "socket: $@";

print "Awaiting UDP messages on port $PORTNO\n";

# for a real long time
while ($sock->recv($recvd, $MAXLEN)) {
  my($port, $ipaddr) = sockaddr_in($sock->peername);
  $client = gethostbyaddr($ipaddr, AF_INET);
  print "Recvd from ($client) $recvd Size (",bytes::length($recvd),") bytes \n";
  
  # convert to integer and send it back
  $sock->send(pack("i",bytes::length($recvd)));
}

die "recv: $!";
