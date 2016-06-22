#
# Copyright (c) 2016 David Lamparter for NetDEF, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

@0xcde3b6bf3a9eb937;

using Codegen = import "/codegen.capnp";
using Codegen.Cflag;
using Codegen.IPv4;
using Codegen.IPv6;
using Codegen.ctype;
using Codegen.cflag;
using Codegen.csetter;
using Codegen.caltname;
using Codegen.csetwrite;
using Codegen.cunion;
using Codegen.cunionval;
using Codegen.clen;
using Codegen.cexpr;

enum Af {
	ipv4			@0 $cexpr("AF_INET");
	ipv6			@1 $cexpr("AF_INET6");
}

enum TransPref {
	ipv4			@0 $cexpr("DUAL_STACK_LDPOV4");
	ipv6			@1 $cexpr("DUAL_STACK_LDPOV6");
}

enum PwType {
	ethernet		@0 $cexpr("PW_TYPE_ETHERNET");
	ethernetTagged		@1 $cexpr("PW_TYPE_ETHERNET_TAGGED");
}

struct LDP $csetwrite
# LDP global configuration.
# Some settings can be overruled if also configured within a more restricted scope, like per interface or per neighbor.
{
	cfFLdpdEnabled		@0 :Bool $cflag(field = "flags", value = "F_LDPD_ENABLED");
	# Specify whether LDP is globally enabled or not.
	# Default value: False.

	rtrId			@1 :IPv4;
	# Set the router ID.
	# If not specified, then ldpd will obtain a router-ID automatically from zebra.

	transPref		@2 :TransPref $ctype("uint16_t");
	# Specify the preferred address-family for TCP transport connections.
	# If two dual-stack LSRs preferences does not match, no LDP session will be established.
	# Default value: ipv4.
	# Valid options: ipv4, ipv6.

	cfFLdpdDsCiscoInterop	@3 :Bool $cflag(field = "flags", value = "F_LDPD_DS_CISCO_INTEROP");
	# If set to True, Cisco non-compliant format will be used to send and interpret the Dual-Stack capability TLV.
	# Default value: False.

	lhelloHoldtime	 	@4 :UInt16;
	# Set the link hello holdtime in seconds.
	# Default value: 15.
	# Valid range: 3-65535.

	lhelloInterval	 	@5 :UInt16;
	# Set the link hello interval in seconds.
	# Default value: 5.
	# Valid range: 1-65535.

	thelloHoldtime	 	@6 :UInt16;
	# Set the targeted hello holdtime in seconds.
	# Default value: 45.
	# Valid range: 3-65535.

	thelloInterval		@7 :UInt16;
	# Set the targeted hello interval in seconds.
	# Default value: 5.
	# Valid range: 1-65535.

	ipv4 :group {
	# IPv4 address-family

		cfFLdpdAfEnabled	@8 :Bool $cflag(field = "flags", value = "F_LDPD_AF_ENABLED");
		# Specify whether LDP is enabled for IPv4.
		# Default value: False.

		cfFLdpdAfThelloAccept	@9 :Bool $cflag(field = "flags", value = "F_LDPD_AF_THELLO_ACCEPT");
		# If set to True, allow LDP sessions to be established with remote neighbors that have not been specifically configured.
		# Default value: False.

		cfFLdpdAfExpNull	@10 :Bool $cflag(field = "flags", value = "F_LDPD_AF_EXPNULL");
		# If set to True, advertise explicit-null labels in place of implicit-null labels for directly connected prefixes.
		# Default value: False.

		cfFLdpdAfNoGTSM		@11 :Bool $cflag(field = "flags", value = "F_LDPD_AF_NO_GTSM");
		# If set to False, ldpd will use the GTSM procedures described in RFC 6720.
		# If GTSM is enabled, multi-hop neighbors should have either GTSM disabled individually or configured with an appropriate gtsmHops distance.
		# Default value: False.

		transAddr :group {
		# Set the local address to be used in the TCP sessions.
		# The router-id will be used if this option is not specified.
			v4		@12 :IPv4;
		}

		keepalive		@13 :UInt16;
		# Set the keepalive timeout in seconds.
		# Default value: 180.
		# Valid range: 3-65535.

		lhelloHoldtime		@14 :UInt16;
		# Set the link hello holdtime in seconds.
		# Default value: 15.
		# Valid range: 3-65535.

		lhelloInterval		@15 :UInt16;
		# Set the link hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.

		thelloHoldtime	 	@16 :UInt16;
		# Set the targeted hello holdtime in seconds.
		# Default value: 45.
		# Valid range: 3-65535.

		thelloInterval		@17 :UInt16;
		# Set the targeted hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.
	}

	ipv6 :group {
	# IPv6 address-family

		cfFLdpdAfEnabled	@18 :Bool $cflag(field = "flags", value = "F_LDPD_AF_ENABLED");
		# Specify whether LDP is enabled for IPv6.
		# Default value: False.

		cfFLdpdAfThelloAccept	@19 :Bool $cflag(field = "flags", value = "F_LDPD_AF_THELLO_ACCEPT");
		# If set to True, allow LDP sessions to be established with remote neighbors that have not been specifically configured.
		# Default value: False.

		cfFLdpdAfExpNull	@20 :Bool $cflag(field = "flags", value = "F_LDPD_AF_EXPNULL");
		# If set to True, advertise explicit-null labels in place of implicit-null labels for directly connected prefixes.
		# Default value: False.

		cfFLdpdAfNoGTSM		@21 :Bool $cflag(field = "flags", value = "F_LDPD_AF_NO_GTSM");
		# If set to False, ldpd will use the GTSM procedures described in RFC 7552.
		# Since GTSM is mandatory for LDPv6, the only effect of disabling GTSM for the IPv6 address-family is that ldpd will not check the
		# incoming packets' hop limit. Outgoing packets will still be sent using a hop limit of 255 to guarantee interoperability.
		# If GTSM is enabled, multi-hop neighbors should have either GTSM disabled individually or configured with an appropriate gtsmHops distance.
		# Default value: False.

		transAddr :group {
		# Set the local address to be used in the TCP sessions.
			v6		@22 :IPv6;
		}

		keepalive		@23 :UInt16;
		# Set the keepalive timeout in seconds.
		# Default value: 180.
		# Valid range: 3-65535.

		lhelloHoldtime		@24 :UInt16;
		# Set the link hello holdtime in seconds.
		# Default value: 15.
		# Valid range: 3-65535.

		lhelloInterval		@25 :UInt16;
		# Set the link hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.

		thelloHoldtime	 	@26 :UInt16;
		# Set the targeted hello holdtime in seconds.
		# Default value: 45.
		# Valid range: 3-65535.

		thelloInterval		@27 :UInt16;
		# Set the targeted hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.
	}
}

struct LDPIface $csetwrite
# Represents an LDP interface.
{
	name			@0 :Text;
	# Interface name

	ipv4 :group {
	# IPv4 address-family

		enabled		@1 :Bool;
		# Specify whether this interface is enabled for LDPv4.
		# Default value: False.

		helloHoldtime	@2 :UInt16;
		# Set the link hello holdtime in seconds.
		# Default value: 15.
		# Valid range: 3-65535.

		helloInterval	@3 :UInt16;
		# Set the link hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.
	}

	ipv6 :group {
	# IPv6 address-family

		enabled		@4 :Bool;
		# Specify whether this interface is enabled for LDPv6.
		# Default value: False.

		helloHoldtime	@5 :UInt16;
		# Set the link hello holdtime in seconds.
		# Default value: 15.
		# Valid range: 3-65535.

		helloInterval	@6 :UInt16;
		# Set the link hello interval in seconds.
		# Default value: 5.
		# Valid range: 1-65535.
	}
}

struct LDPTnbr $csetwrite
# Represents an LDP targeted neighbor.
{
	af			@0 :Af $ctype("int");
	# Address-family of the targeted neighbor.
	# Valid options: ipv4, ipv6.

	addr :union $cunion("af") {
	# Address (IPv4 or IPv6) of the targeted neighbor.
		v4 @1 :IPv4 $cunionval("AF_INET");
		v6 @2 :IPv6 $cunionval("AF_INET6");
	}
}

struct LDPNbrp $csetwrite
# Allows for the configuration of neighbor-specific parameters.
# Note, however, that ldpd uses the hello discovery mechanism to discover its neighbors.
# Without an underlying adjacency these commands have no effect.
# The neighbor-specific parameters apply for both LDPoIPv4 and LDPoIPv6 sessions.
{
	lsrId			@0 :IPv4;
	# Neighbor's LSR-ID

	keepalive		@1 :UInt16;
	# Set the keepalive timeout in seconds.
	# Valid range: 3-65535.

	cfFNbrpKeepalive	@2 :Bool $cflag(field = "flags", value = "F_NBRP_KEEPALIVE");
	# Must be set to True when 'keepalive' is set.
	# Default value: False.

	gtsmEnabled		@3 :Int32;
	# Override the inherited configuration and enable/disable GTSM for this neighbor.
	# Valid options: 0, 1.

	cfFNbrpGtsm		@4 :Bool $cflag(field = "flags", value = "F_NBRP_GTSM");
	# Must be set to True when 'gtsmEnabled' is set.
	# Default value: False.

	gtsmHops		@5 :UInt8;
	# Set the maximum number of hops the neighbor may be away.
	# When GTSM is enabled for this neighbor, incoming packets are required to have
	# a TTL/hop limit of 256 minus this value, ensuring they have not passed through
	# more than the expected number of hops.
	# Default value: 1.
	# Valid range: 1-255.

	cfFNbrpGtsmHops		@6 :Bool $cflag(field = "flags", value = "F_NBRP_GTSM_HOPS");
	# Must be set to True when 'gtsmHops' is set.
	# Default value: False.

	auth :group {
		passwd :union $cunion("method") {
			none		@7 :Void $cunionval("AUTH_NONE") ;
			md5key		@8 :Text $cunionval("AUTH_MD5SIG") $clen("md5key_len");
			# Enable TCP MD5 signatures per RFC 5036.
		}
	}
}

struct LDPL2VPN $csetwrite
# Represents an LDP L2VPN instance.
{
	name			@0 : Text;
	# L2VPN name

	pwType			@1 : PwType $ctype("int");
	# Specify the type of the configured pseudowires. The type must be the same at both endpoints.
	# Default value: ethernet.
	# Valid options: ethernet, ethernetTagged.

	mtu			@2 : Int32;
	# Set the MTU advertised in the pseudowires. Local and remote MTUs must match for a pseudowire to be set up.
	# Default value: 1500.
	# Valid range: 512-65535.

	brIfname		@3 : Text;
	# Set the bridge interface the VPLS is associated with.
	# This setting is optional and is only used to remove MAC addresses received from MAC address withdrawal messages.
}

struct LDPL2VPNIface $csetwrite
# Represents a L2VPN non pseudowire interface.
# When configured, ldpd will send MAC address withdrawal messages when this interface is shut down.
{
	ifname			@0 : Text;
	# Interface name
}

struct LDPL2VPNPseudowire $csetwrite
# Represents a L2VPN pseudowire interface.
{
	ifname			@0 :Text;
	# Name of the pseudowire interface.

	pwid			@1 :UInt32;
	# Set the PW ID used to identify the pseudowire.
	# The PW ID must be the same at both endpoints.
	# Valid range: 1-4294967295.

	lsrId			@2 :IPv4;
	# Specify the LSR-ID of the remote endpoint of the pseudowire.

	af			@3 :Af $ctype("int");
	# Address-family of the remote endpoint of the pseudowire.
	# Valid options: ipv4, ipv6.
	# Default value: ipv4.

	addr :union $cunion("af") {
	# Address of the remote endpoint of the pseudowire.
	# A targeted neighbor will automatically be created for this address.
	# Default value: 'lsrId'.
		v4 @4 :IPv4 $cunionval("AF_INET");
		v6 @5 :IPv6 $cunionval("AF_INET6");
	}

	cfFPwStaticNbrAddr	@6 :Bool $cflag(field = "flags", value = "F_PW_STATIC_NBR_ADDR");
	# Must be set to True when 'addr' is set.
	# Default value: False.

	cfFPwStatusTlvConf	@7 :Bool $cflag(field = "flags", value = "F_PW_STATUSTLV_CONF");
	# Specify whether the use of the Status TLV is preferred or not preferred.
	# Default value: True.

	cfFPwCwordConf		@8 :Bool $cflag(field = "flags", value = "F_PW_CWORD_CONF");
	# Specify whether the use of the control word is preferred or not preferred.
	# Default value: True.
}

# vim: set noet ts=8 nowrap tw=0:
