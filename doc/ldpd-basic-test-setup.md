## Topology

The goal of this test is to verify that the all the basic functionality
of ldpd is working as expected, be it running on Linux or OpenBSD. In
addition to that, more advanced features are also tested, like LDP
sessions over IPv6, MD5 authentication and pseudowire signaling.

In the topology below there are 3 PE routers, 3 CE routers and one P
router (not attached to any consumer site).

All routers have IPv4 addresses and OSPF is used as the IGP. The
three routers from the bottom of the picture, P, PE2 and PE3, are also
configured for IPv6 (dual-stack) and static IPv6 routes are used to
provide connectivity among them.

The three CEs share the same VPLS membership. LDP is used to set up the
LSPs among the PEs and to signal the pseudowires. MD5 authentication is
used to protect all LDP sessions.

```
                          CE1 172.16.1.1/24
                           +
                           |
                       +---+---+
                       |  PE1  |
                       | IOS XE|
                       |       |
                       +---+---+
                           |
                           | 10.0.1.0/24
                           |
                       +---+---+
                       |   P   |
                +------+ IOS XR+------+
                |      |       |      |
                |      +-------+      |
    10.0.2.0/24 |                     | 10.0.3.0/24
2001:db8:2::/64 |                     | 2001:db8:3::/64
                |                     |
            +---+---+             +---+---+
            |  PE2  |             |  PE3  |
            |OpenBSD+-------------+ Linux |
            |       |             |       |
            +---+---+ 10.0.4.0/24 +---+---+
                |   2001:db8:4::/64   |
                +                     +
 172.16.1.2/24 CE2                   CE3 172.16.1.3/24
```


## Configuration

#### Linux
1 - Enable IPv4/v6 forwarding:
```
# sysctl -w net.ipv4.ip_forward=1
# sysctl -w net.ipv6.conf.all.forwarding=1
```

2 - Set up the interfaces:
```
# ip link add name lo1 type dummy
# ip link set dev lo1 up
# ip addr add 4.4.4.4/32 dev lo1
# ip -6 addr add 4:4:4::4/128 dev lo1
# ip link set dev eth1 up
# ip addr add 10.0.4.4/24 dev eth1
# ip -6 addr add 2001:db8:4::4/64 dev eth1
# ip link set dev eth2 up
# ip addr add 10.0.3.4/24 dev eth2
# ip -6 addr add 2001:db8:3::4/64 dev eth2
```

3 - Set up the bridge and pseudowire interfaces:
```
# ip link add type bridge
# ip link set dev bridge0 up
# ip link set dev eth0 up
# ip link set dev eth0 master bridge0
# ip link add name mpw0 type dummy
# ip link set dev mpw0 up
# ip link set dev mpw0 master bridge0
# ip link add name mpw1 type dummy
# ip link set dev mpw1 up
# ip link set dev mpw1 master bridge0
```

> NOTE: MPLS support in the Linux kernel is very recent and it still
doesn't support pseudowire interfaces. We are using here dummy interfaces
just to show how the VPLS configuration should looks like in the future.

4 - Add static IPv6 routes for the remote loopbacks:
```
# ip -6 route add 2:2:2::2/128 via 2001:db8:3::2
# ip -6 route add 3:3:3::3/128 via 2001:db8:4::3
```

5 - Edit /etc/quagga/ospfd.conf:
```
router ospf
 network 4.4.4.4/32 area 0.0.0.0
 network 10.0.3.4/24 area 0.0.0.0
 network 10.0.4.4/24 area 0.0.0.0
!
```

6 - Edit /etc/quagga/ldpd.conf:
```
mpls ldp
 dual-stack cisco-interop
 neighbor 1.1.1.1 password opensourcerouting
 neighbor 2.2.2.2 password opensourcerouting
 neighbor 3.3.3.3 password opensourcerouting
 !
 address-family ipv4
  discovery transport-address 4.4.4.4
  !
  interface eth2
  !
  interface eth1
  !
 !
 address-family ipv6
  discovery transport-address 4:4:4::4
  !
  interface eth2
  !
  interface eth1
  !
 !
!
l2vpn ENG type vpls
 bridge br0
 member interface eth0
 !
 member pseudowire mpw0
  neighbor lsr-id 1.1.1.1
  pw-id 100
 !
 member pseudowire mpw1
  neighbor lsr-id 3.3.3.3
  neighbor address 3:3:3::3
  pw-id 100
 !
!
```

> NOTE: Care should be taken when using the **label local advertise
explicit-null** command under Linux. If the MPLS kernel module is not
loaded, all packets arriving with the explicit-null label will be dropped,
which can cause the LDP sessions to be torn down.

7 - Run zebra, ospfd and ldpd.

#### OpenBSD
1 - Enable IPv4/v6 forwarding:
```
# sysctl net.inet.ip.forwarding=1
# sysctl net.inet6.ip6.forwarding=1
```

2 - Set up the interfaces:
```
# ifconfig lo1 alias 3.3.3.3 netmask 255.255.255.255
# ifconfig lo1 inet6 3:3:3::3/128
# ifconfig em2 10.0.2.3/24 mpls
# ifconfig em2 inet6 2001:db8:2::3/64
# ifconfig em3 10.0.4.3/24 mpls
# ifconfig em3 inet6 2001:db8:4::3/64
```

3 - Set up the bridge and pseudowire interfaces:
```
# ifconfig bridge0 create
# ifconfig bridge0 up
# ifconfig em1 up
# ifconfig bridge0 add em1
# ifconfig mpw0 create
# ifconfig mpw0 up
# ifconfig bridge0 add mpw0
# ifconfig mpw1 create
# ifconfig mpw1 up
# ifconfig bridge0 add mpw1
```

4 - Add static IPv6 routes for the remote loopbacks:
```
# route -n add 4:4:4::4/128 2001:db8:4::4
# route -n add 2:2:2::2/128 2001:db8:2::2
```

5 - Edit /etc/quagga/ospfd.conf:
```
router ospf
 network 10.0.2.3/24 area 0
 network 10.0.4.3/24 area 0
 network 3.3.3.3/32 area 0
!
```

6 - Edit /etc/quagga/ldpd.conf:
```
mpls ldp
 dual-stack cisco-interop
 neighbor 1.1.1.1 password opensourcerouting
 neighbor 2.2.2.2 password opensourcerouting
 neighbor 4.4.4.4 password opensourcerouting
 !
 address-family ipv4
  discovery transport-address 3.3.3.3
  !
  interface em3
  !
  interface em2
  !
 !
 address-family ipv6
  discovery transport-address 3:3:3::3
  !
  interface em3
  !
  interface em2
  !
 !
!
l2vpn ENG type vpls
 bridge br0
 member interface em1
 !
 member pseudowire mpw0
  neighbor lsr-id 1.1.1.1
  pw-id 100
 !
 member pseudowire mpw1
  neighbor lsr-id 4.4.4.4
  neighbor address 4:4:4::4
  pw-id 100
 !
!
```

7 - Run zebra, ospfd and ldpd.

#### Cisco routers
CE1 (IOS):
```
interface FastEthernet0/0
 ip address 172.16.1.1 255.255.255.0
 !
!
```

CE2 (IOS):
```
interface FastEthernet0/0
 ip address 172.16.1.2 255.255.255.0
 !
!
```

CE3 (IOS):
```
interface FastEthernet0/0
 ip address 172.16.1.3 255.255.255.0
 !
!
```

PE1 - IOS-XE (1):
```
mpls ldp neighbor 2.2.2.2 password opensourcerouting
mpls ldp neighbor 3.3.3.3 password opensourcerouting
mpls ldp neighbor 4.4.4.4 password opensourcerouting
!
l2vpn vfi context VFI
 vpn id 1
 member pseudowire2
 member pseudowire1
!
bridge-domain 1
 member GigabitEthernet1 service-instance 1
 member vfi VFI
!
interface Loopback1
 ip address 1.1.1.1 255.255.255.255
!
interface pseudowire1
 encapsulation mpls
 neighbor 3.3.3.3 100
!
interface pseudowire2
 encapsulation mpls
 neighbor 4.4.4.4 100
!
interface GigabitEthernet3
 ip address 10.0.1.1 255.255.255.0
 mpls ip
!
router ospf 1
 network 0.0.0.0 255.255.255.255 area 0
!
```

P - IOS-XR (2):
```
interface Loopback1
 ipv4 address 2.2.2.2 255.255.255.255
 ipv6 address 2:2:2::2/128
!
interface GigabitEthernet0/0/0/0
 ipv4 address 10.0.1.2 255.255.255.0
!
interface GigabitEthernet0/0/0/1
 ipv4 address 10.0.2.2 255.255.255.0
 ipv6 address 2001:db8:2::2/64
 ipv6 enable
!
interface GigabitEthernet0/0/0/2
 ipv4 address 10.0.3.2 255.255.255.0
 ipv6 address 2001:db8:3::2/64
 ipv6 enable
!
router static
 address-family ipv6 unicast
  3:3:3::3/128 2001:db8:2::3
  4:4:4::4/128 2001:db8:3::4
 !
!
router ospf 1
 router-id 2.2.2.2
 address-family ipv4 unicast
 area 0
  interface Loopback1
  !
  interface GigabitEthernet0/0/0/0
  !
  interface GigabitEthernet0/0/0/1
  !
  interface GigabitEthernet0/0/0/2
  !
 !
!
mpls ldp
 router-id 2.2.2.2
 neighbor
  1.1.1.1:0 password clear opensourcerouting
  3.3.3.3:0 password clear opensourcerouting
  4.4.4.4:0 password clear opensourcerouting
 !
 address-family ipv4
 !
 address-family ipv6
  discovery transport-address 2:2:2::2
 !
 interface GigabitEthernet0/0/0/0
  address-family ipv4
  !
 !
 interface GigabitEthernet0/0/0/1
  address-family ipv4
  !
  address-family ipv6
  !
 !
 interface GigabitEthernet0/0/0/2
  address-family ipv4
  !
  address-family ipv6
  !
 !
!
```

## Verification - Control Plane

Using the CLI on the Linux box, the goal is to ensure that everything
is working as expected.

First, verify that all the required adjacencies and neighborships sessions
were established:

```
linux# show mpls ldp discovery
Local LDP Identifier: 4.4.4.4:0
Discovery Sources:
  Interfaces:
    eth1: xmit/recv
      LDP Id: 3.3.3.3:0, Transport address: 3.3.3.3
          Hold time: 15 sec
      LDP Id: 3.3.3.3:0, Transport address: 3:3:3::3
          Hold time: 15 sec
    eth2: xmit/recv
      LDP Id: 2.2.2.2:0, Transport address: 2.2.2.2
          Hold time: 15 sec
      LDP Id: 2.2.2.2:0, Transport address: 2:2:2::2
          Hold time: 15 sec
  Targeted Hellos:
    4.4.4.4 -> 1.1.1.1: xmit/recv
      LDP Id: 1.1.1.1:0, Transport address: 1.1.1.1
          Hold time: 45 sec
    4:4:4::4 -> 3:3:3::3: xmit/recv
      LDP Id: 3.3.3.3:0, Transport address: 3:3:3::3
          Hold time: 45 sec

linux# show mpls ldp neighbor
Peer LDP Identifier: 1.1.1.1:0
  TCP connection: 4.4.4.4:40921 - 1.1.1.1:646
  Session Holdtime: 180 sec
  State: OPERATIONAL; Downstream-Unsolicited
  Up time: 00:06:02
  LDP Discovery Sources:
    IPv4:
      Targeted Hello: 1.1.1.1

Peer LDP Identifier: 2.2.2.2:0
  TCP connection: 4:4:4::4:52286 - 2:2:2::2:646
  Session Holdtime: 180 sec
  State: OPERATIONAL; Downstream-Unsolicited
  Up time: 00:06:02
  LDP Discovery Sources:
    IPv4:
      Interface: eth2
    IPv6:
      Interface: eth2

Peer LDP Identifier: 3.3.3.3:0
  TCP connection: 4:4:4::4:60575 - 3:3:3::3:646
  Session Holdtime: 180 sec
  State: OPERATIONAL; Downstream-Unsolicited
  Up time: 00:05:57
  LDP Discovery Sources:
    IPv4:
      Interface: eth1
    IPv6:
      Targeted Hello: 3:3:3::3
      Interface: eth1
```

Note that the neighborships with the P and PE2 routers were established
over IPv6, since this is the default behavior for dual-stack LSRs, as
specified in RFC 7552. If desired, the **dual-stack transport-connection
prefer ipv4** command can be used to establish these sessions over IPv4
(the command should be applied an all routers).

Now, verify that there's a remote label for each PE address:
```
linux# show mpls ldp binding
1.1.1.1/32
        Local binding: label: 20
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             imp-null
            2.2.2.2             24000
            3.3.3.3             20
2.2.2.2/32
        Local binding: label: 21
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             18
            2.2.2.2             imp-null
            3.3.3.3             21
3.3.3.3/32
        Local binding: label: 22
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             21
            2.2.2.2             24003
            3.3.3.3             imp-null
4.4.4.4/32
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             22
            2.2.2.2             24001
            3.3.3.3             22
10.0.1.0/24
        Local binding: label: 23
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             imp-null
            2.2.2.2             imp-null
            3.3.3.3             23
10.0.2.0/24
        Local binding: label: 24
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             20
            2.2.2.2             imp-null
            3.3.3.3             imp-null
10.0.3.0/24
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             19
            2.2.2.2             imp-null
            3.3.3.3             24
10.0.4.0/24
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            1.1.1.1             23
            2.2.2.2             24002
            3.3.3.3             imp-null
2:2:2::2/128
        Local binding: label: 18
        Remote bindings:
            Peer                Label
            -----------------   ---------
            2.2.2.2             imp-null
            3.3.3.3             18
3:3:3::3/128
        Local binding: label: 19
        Remote bindings:
            Peer                Label
            -----------------   ---------
            2.2.2.2             24007
4:4:4::4/128
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            2.2.2.2             24006
            3.3.3.3             19
2001:db8:2::/64
        Local binding: label: -
        Remote bindings:
            Peer                Label
            -----------------   ---------
            2.2.2.2             imp-null
            3.3.3.3             imp-null
2001:db8:3::/64
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            2.2.2.2             imp-null
2001:db8:4::/64
        Local binding: label: imp-null
        Remote bindings:
            Peer                Label
            -----------------   ---------
            3.3.3.3             imp-null
```

Check if the pseudowires are up:
```
linux# show l2vpn atom vc
Interface Peer ID         VC ID      Name             Status
--------- --------------- ---------- ---------------- ----------
mpw1      3.3.3.3         100        ENG              UP
mpw0      1.1.1.1         100        ENG              UP
```

Check the label bindings of the pseudowires:
```
linux# show l2vpn atom binding
  Destination Address: 1.1.1.1, VC ID: 100
    Local Label:  25
        Cbit: 1,    VC Type: Ethernet,    GroupID: 0
        MTU: 1500
    Remote Label:  16
        Cbit: 1,    VC Type: Ethernet,    GroupID: 0
        MTU: 1500
  Destination Address: 3.3.3.3, VC ID: 100
    Local Label:  26
        Cbit: 1,    VC Type: Ethernet,    GroupID: 0
        MTU: 1500
    Remote Label:  26
        Cbit: 1,    VC Type: Ethernet,    GroupID: 0
        MTU: 1500
```

## Verification - Data Plane
TODO - MPLS support in zebra is not implemented yet.
