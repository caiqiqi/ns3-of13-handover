# Description

## link bandwidth
The link bandwidth between the switch1 and switch2 is 100Mbps.
The link bandwidth between each AP and switch1 is respectively 30Mbps
The link bandwidth between each host and switch2 is respectively 30Mbps

## throughput measuring improved
Actually the throughput should drop to zero when doing handover. But due to the drawback of my method of measuring the throughput, 
when it should drop to zero, it remains the previous value. So I have improved it.


### previous vulnerable measuring method
Since the previous measuring method is as follows:
```
tmpThrou   = receivedBytes * 8.0 / 
              (timeLastReceivedPacket.GetSeconds() - 
                timeFirstTrasmittedPacket.GetSeconds())
                 /1024 ;
dataset.Add(tmpThrou);
```
- timeFirstTrasmittedPacket
The value is always 1.0(because we start trasmitting traffic at time 1.0)
- timeLastReceivedPacket
The last time the receiver(host) receives packets.
This means that if at time 19.0, the receiver receives some packets, but at time 19.0~21.0, it doesnt receive any packets,
this value will remain.
- receivedBytes
The total bytes that the receiver receives.
This means if during some period of time, the receiver doesnt receive any packets, this value remains.
And at last, the value is added to the dataset to be drawn into the diagram.


So due to the drawback of my previous method, when the actual bite rate drops to zero, it remains in the diagram.

### new improved measuring method
My improved method is like this:
```
tmpThrou   = receivedBytes * 8.0 / 
              (timeLastReceivedPacket.GetSeconds() - 
                timeFirstTrasmittedPacket.GetSeconds())
                 /1024 ;
// lastThrou is a variable that I set to remember the last throughput value
// at the first time, lastThrou is 0.0, we should add tmpThrou to the dataset
double lastThrou = 0.0;
if (lastThrou == 0)
{
  dataset.Add  (Simulator::Now().GetSeconds(), tmpThrou);
  lastThrou = tmpThrou;
}
else // later on, as long as lastThrou is not equal to tmpThrou, I add the tmpThrou to the dataset
if ( tmpThrou - lastThrou)
{
dataset.Add  (Simulator::Now().GetSeconds(), tmpThrou);
lastThrou = tmpThrou ;
}
// else add 0.0 to the dataset
else
{
dataset.Add  (Simulator::Now().GetSeconds(), 0.0);
} 

```

## the measuring cycle
As the measuring method indicates, the measuring cycle affect the output diagram.
Since the handover process is lasts about 1.5~2.0 seconds, 
so I choose 4 cycle values, 0.2s, 0.5s, 0.7s, 1.0s to make the output diagram more clear.

## experiment time
3PM on 2016-11-20
