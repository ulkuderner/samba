#!/bin/sh
# Blackbox tests for kinit and kerberos integration with smbclient etc
# Copyright (C) 2006-2007 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2006-2008 Andrew Bartlett <abartlet@samba.org>

if [ $# -lt 5 ]; then
cat <<EOF
Usage: test_kinit.sh SERVER USERNAME PASSWORD REALM DOMAIN PREFIX ENCTYPE SMBCLINET
EOF
exit 1;
fi

SERVER=$1
USERNAME=$2
PASSWORD=$3
REALM=$4
DOMAIN=$5
PREFIX=$6
ENCTYPE=$7
smbclient=$8
shift 8
failed=0

samba4bindir="$BINDIR"
samba4srcdir="$SRCDIR/source4"
samba4kinit=kinit
if test -x $BINDIR/samba4kinit; then
	samba4kinit=$BINDIR/samba4kinit
fi

samba_tool="$samba4bindir/samba-tool"
samba4kpasswd=kpasswd
if test -x $BINDIR/samba4kpasswd; then
	samba4passwd=$BINDIR/samba4kpasswd
fi

enableaccount="$samba_tool user enable"
machineaccountccache="$samba4srcdir/scripting/bin/machineaccountccache"

ldbmodify="ldbmodify"
if [ -x "$samba4bindir/ldbmodify" ]; then
	ldbmodify="$samba4bindir/ldbmodify"
fi

ldbsearch="ldbsearch"
if [ -x "$samba4bindir/ldbsearch" ]; then
	ldbsearch="$samba4bindir/ldbsearch"
fi

. `dirname $0`/subunit.sh
. `dirname $0`/common_test_fns.inc

enctype="-e $ENCTYPE"
unc="//$SERVER/tmp"

KRB5CCNAME_PATH="$PREFIX/tmpccache"
KRB5CCNAME="FILE:$KRB5CCNAME_PATH"
export KRB5CCNAME
rm -f $KRB5CCNAME_PATH

PKUSER="--pk-user=FILE:$PREFIX/private/tls/admincert.pem,$PREFIX/private/tls/adminkey.pem"

testit "kinit with pkinit (name specified)" $samba4kinit $enctype --request-pac --renewable $PKUSER $USERNAME@$REALM || failed=`expr $failed + 1`
testit "kinit with pkinit (enterprise name specified)" $samba4kinit $enctype --request-pac --renewable $PKUSER --enterprise $USERNAME@$REALM || failed=`expr $failed + 1`
testit "kinit with pkinit (enterprise name in cert)" $samba4kinit $enctype --request-pac --renewable $PKUSER --pk-enterprise || failed=`expr $failed + 1`
testit "kinit renew ticket" $samba4kinit --request-pac -R

test_smbclient "Test login with kerberos ccache" 'ls' "$unc" -k yes || failed=`expr $failed + 1`

rm -f $KRB5CCNAME_PATH
exit $failed