In `trad-handover-20_nods_in_ap2-from-right-to-left.mp4` , </br>
only the moving STA is the TCP source. There are 20 nodes within AP2's reach, but they dont generate
TCP traffic.
### TCP traffic
from the time `1.0211`s to `10.9558`s;
### Handover process
between AP3 and AP2 happens at the time `10.9587`s, at which time, association to AP2 established;
### TCP traffic
from the time `11.9762`s to `20.9869`s;
### Handover process
between AP2 and AP1 happens at the time `21.0960`s, at which time, association to AP1 established.
### TCP traffic
from the time `22.0081`s.

In `trad-handover-20_nods_in_ap2-from-right-to-left-all-nodes-in-ap2-tcp-new.mp4` , </br>
the moving STA and all the 20 nodes within AP2's reach are TCP sources, i.e AP2 is overloaded.
### TCP traffic
from the time `1.0411`s to `10.9119`s;
### Handover process
between AP3 and AP2 happens at the time `11.0608`s, at which time, association to AP2 established;
### TCP traffic
from the time `12.3622`s to `20.7621`s;
### Handover process
between AP2 and AP1 happens at the time `21.0960`s, at which time, association to AP1 established.
### TCP traffic
from the time `22.1431`s.

## Summary
1. TCP handshake(TCP SYN packets) only happened at the begining. Later on, after the assiaction connection between the STA to the AP is lost during 
the handover process, the handshake is not made again. And the TCP data resume being transffered. (I think this is maybe because I dont assign IP 
addresses to the APs. Do I need to assign IP to the APs and then perform the forwarding action in the APs?)
2. When AP2 is not overloaded, the handover process between AP3 to AP2 is very quick. Almost once the STA stops sending TCP traffic via AP3 and
right away(0.002s) makes association connection to AP2. In comparison, after the TCP traffic via AP3 is stopped, when AP2 is overloaded, some time(0.14s) is needed to make the association connection.
