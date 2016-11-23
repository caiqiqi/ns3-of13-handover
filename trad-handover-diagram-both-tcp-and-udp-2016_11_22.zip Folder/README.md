## Parameters

### Sampling period
Since if I set the sampling period too long, there will be not enough points in the diagram for us to
observe the handover process. If I set the sampling period too short. There will be more points dropping to zero in the diagram,
making us confused about the actual handover process and other irrelavant time. So I choose the `0.8s` version,
which is more clear to observe.

### Moving station and AP
The maximum range that the AP's signal is able to cover is 100m.

- AP1 transmit power : 90dbm
- AP2 transmit power : 100dbm
- AP3 transmit power : 90dbm

The moving station is initially associated to AP3, and as it moves, when it steps into the area where AP1 and AP2 are both 
able to cover the signal, it chooses the AP2, which has a higher RSSI than AP1 does, though more loaded.
The moving station does not do active probing, which means it does not actively scan for available APs for it to associate.
Instead, the three APs issue beacons to inform the stations within their reach for later association.

### SSID and DHCP
From the very beginning, we set a fixd SSID "ssid-default", for all the APs and stations;
and a fixed IP address for all the stations and hosts, so there is no DHCP.

### AP network interface card mechanism
Each AP does not have an IP address, like a layer 2 device. It just forwards the frames received from the wireless interface card
to the csma ethernet interface card and vice versa.

### link capacity
- The link bandwidth between the switch1 and switch2 is `100Mbps`.
- The link bandwidth between each AP and switch1 is respectively `30Mbps`.
- The link bandwidth between each host and switch2 is respectively `30Mbps`.


### Parameters for TCP 
- Packet size : 1024 Bytes
- Data rate:    5Mbps

### Parameters for UDP
- Packet size : 1024 Bytes
- Interval between two packets to be sent: 0.2s 
If this interval value is set too high, which means the time to wait between two packets to be sent is too long, there will be little packets trasmmited,
and the throughput will be very low.
If this value is set too low, which means the time to wait between two packets to be sent is too short, there will be huge packet loss,
and the throughput will still be low.

## The handover process
The handover process time for udp and tcp are not the same. 
- For TCP, handover process happens at time around `19.0s~20.5s`.
- For UDP, handover process happens at time around `16.0s~17.0s`.

I make two arrows for TCP handover and UDP handover in the "Throughput VS Time" diagram to make the diagram more understanderable.
I dont know if it is necessary to mark the handover process for the "Delay VS Time" diagram and "Packet Loss Ratio VS Time" diagram.

## Experiment Time
9:30PM on 11/22/2016
