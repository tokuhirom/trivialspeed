use strict;
use warnings;
use Test::TCP;
use Test::More;
use LWP::UserAgent;
use File::Temp;

my $tmp = File::Temp->new();
system("kchashmgr create $tmp") == 0 or die;
system("kchashmgr set $tmp / OK") == 0 or die;
system("kchashmgr set $tmp /dankogai kogaidan") == 0 or die;

test_tcp(
    client => sub {
        my $port = shift;

        {
            my $ua = LWP::UserAgent->new();
            my $res = $ua->get("http://127.0.0.1:$port");
            ok $res->is_success;
            is $res->content, "OK";
        }

        {
            my $ua = LWP::UserAgent->new();
            my $res = $ua->get("http://127.0.0.1:$port/dankogai");
            ok $res->is_success;
            is $res->content, "kogaidan";
        }

        done_testing;
    },
    server => sub {
        my $port = shift;
        exec './trivialspeed', '-p', $port, "$tmp";
        die "cannot exec: trivialspeed";
    },
);
