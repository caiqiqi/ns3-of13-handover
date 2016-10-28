// Network topology
//         ----------     ---------- 
//  AP3 -- | Switch1 | --| Switch2 | -- H1
//         ----------     ----------
//   |      |       |          |
//   X     AP1     AP2         H2
//        | | |   | |||
//        X X |   X X|X
//            |      |
//            m2     m1  
//


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
//#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ofswitch13-module.h"
#include "ns3/log.h"
// 给传统交换机(BridgeNetDevice)
#include "ns3/bridge-module.h"

#include "ns3/global-route-manager.h"

//包含 `gnuplot`和`Gnuplot2Ddatabase`
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"

#include "ns3/netanim-module.h"

#include <iostream>
#include <stdint.h>
#include <sstream>
#include <fstream>
#include <vector>


using namespace ns3;

//NS_LOG_COMPONENT_DEFINE ("TradHandoverScript");



bool tracing  = true;
ns3::Time timeout = ns3::Seconds (0);


double stopTime = 60.0;  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nHost       = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 4;
uint32_t nAp3Station = 1;


double nSamplingPeriod = 0.5;   // 抽样间隔，根据总的Simulation时间做相应的调整


/* for udp-server-client application. */
uint32_t nMaxPackets = 20000;    // The maximum packets to be sent.
double nInterval  = 0.1;  // The interval between two packet sent.
uint32_t nPacketSize = 1024;

/* for tcp-bulk-send application. */   
//uint32_t nMaxBytes = 1000000000;  //Zero is unlimited. 100M
uint32_t nMaxBytes = 0;

// 1500字节以下的帧不需要RTS/CTS
uint32_t rtslimit = 1500;

/* 恒定速度移动节点的
初始位置 x = 0.0, y = 25.0
和
移动速度 x = 1.0,  y=  0.0
*/
Vector3D mPosition = Vector3D(0.0, 40.0, 0.0);
Vector3D mVelocity = Vector3D(10.0, 0.0 , 0.0);

bool
SetTimeout (std::string value)
{
  try {
      timeout = ns3::Seconds (atof (value.c_str ()));
      return true;
    }
  catch (...) { return false; }
  return false;
}


bool
CommandSetup (int argc, char **argv)
{

  CommandLine cmd;
  //cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  //cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  cmd.AddValue ("SamplingPeriod", "Sampling period", nSamplingPeriod);
  cmd.AddValue ("stopTime", "The time to stop", stopTime);
  
  /* for udp-server-client application */
  // cmd.AddValue ("MaxPackets", "The total packets available to be scheduled by the UDP application.", nMaxPackets);
  // cmd.AddValue ("Interval", "The interval between two packet sent", nInterval);
  // cmd.AddValue ("PacketSize", "The size in byte of each packet", nPacketSize);


  cmd.AddValue ("rtslimit", "The size of packets under which there should be RST/CST", rtslimit);

  cmd.Parse (argc, argv);
  return true;
}




/*
 * Calculate Throughput using Flowmonitor
 * 每个探针(probe)会根据四点来对包进行分类
 * -- when packet is `sent`;
 * -- when packet is `forwarded`;
 * -- when packet is `received`;
 * -- when packet is `dropped`;
 * 由于包是在IP层进行track的，所以任何的四层(TCP)重传的包，都会被认为是一个新的包
 */
void
ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset)
{
  
  double throu   = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  /* since fmhelper is a pointer, we should use it as a pointer.
   * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
   */
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {
    /* 
     * `Ipv4FlowClassifier`
     * Classifies packets by looking at their IP and TCP/UDP headers. 
     * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
    */

    /* 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的 */
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    if (t.sourceAddress==Ipv4Address("10.0.0.10") && t.destinationAddress == Ipv4Address("10.0.0.2"))
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            throu   = i->second.rxBytes * 8.0 / 
              (i->second.timeLastRxPacket.GetSeconds() - 
                i->second.timeFirstTxPacket.GetSeconds())/1024 ;
            dataset.Add  (Simulator::Now().GetSeconds(), throu);
          }
          else
          {
            std::cout << "This is not UDP/TCP traffic" << std::endl;
          }
      }

    }
  /* check throughput every nSamplingPeriod second(每隔nSamplingPeriod调用依次Simulation)
   * 表示每隔nSamplingPeriod时间
   */
  Simulator::Schedule (Seconds(nSamplingPeriod), &ThroughputMonitor, fmhelper, monitor, 
    dataset);

}

void
LostPacketsMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset2)
{
  
  uint32_t LostPacketsum = 0;

  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    LostPacketsum += i->second.lostPackets;

    if (t.sourceAddress==Ipv4Address("10.0.0.10") && t.destinationAddress == Ipv4Address("10.0.0.2"))
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            dataset2.Add (Simulator::Now().GetSeconds(), LostPacketsum);
          }
          else
          {
            std::cout << "This is not UDP/TCP traffic" << std::endl;
          }
      }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &LostPacketsMonitor, fmhelper, monitor, 
    dataset2);

}

void
DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset1)
{
  
  uint32_t RxPacketsum = 0;
  double Delaysum  = 0;
  double delay     = 0;

  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      
      RxPacketsum  += i->second.rxPackets;
      Delaysum     += i->second.delaySum.GetSeconds();
      if (t.sourceAddress==Ipv4Address("10.0.0.10") && t.destinationAddress == Ipv4Address("10.0.0.2"))
        {
          // UDP_PROT_NUMBER = 17
          // TCP_PORT_NUMBER = 6
            if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
            {
              delay = Delaysum/ RxPacketsum;
              dataset1.Add (Simulator::Now().GetSeconds(), delay);
            }
            else
            {
              std::cout << "This is not UDP/TCP traffic" << std::endl;
            }
        }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &DelayMonitor, fmhelper, monitor, 
    dataset1);

}




void
JitterMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset3)
{
  
  uint32_t RxPacketsum = 0;
  double JitterSum = 0;
  double jitter    = 0;

  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    RxPacketsum   += i->second.rxPackets;
    JitterSum     += i->second.jitterSum.GetSeconds();

    if (t.sourceAddress==Ipv4Address("10.0.0.10") && t.destinationAddress == Ipv4Address("10.0.0.2"))
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            jitter  = RxPacketsum/ (JitterSum -1);
            dataset3.Add (Simulator::Now().GetSeconds(), jitter);
          }
          else
          {
            std::cout << "This is not UDP/TCP traffic" << std::endl;
          }
      }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &JitterMonitor, fmhelper, monitor, 
    dataset3);

}


void PrintParams (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor){
  double tempThroughput = 0.0;
  uint32_t TxPacketsum = 0;
  uint32_t RxPacketsum = 0;
  uint32_t DropPacketsum = 0;
  uint32_t LostPacketsum = 0;
  double Delaysum  = 0;
  double JitterSum = 0;

  monitor->CheckForLostPackets(); 
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); i++){ 
      // A tuple: Source-ip, destination-ip, protocol, source-port, destination-port
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
                    

    TxPacketsum   += i->second.txPackets;
    RxPacketsum   += i->second.rxPackets;
    LostPacketsum += i->second.lostPackets;
    DropPacketsum += i->second.packetsDropped.size();
    Delaysum      += i->second.delaySum.GetSeconds();
    JitterSum     += i->second.jitterSum.GetSeconds();
    
    tempThroughput = (i->second.rxBytes * 8.0 / 
      (i->second.timeLastRxPacket.GetSeconds() - 
        i->second.timeFirstTxPacket.GetSeconds())/1024);

    if (t.sourceAddress==Ipv4Address("10.0.0.10") && t.destinationAddress == Ipv4Address("10.0.0.2"))
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            std::cout<<"Time: " << Simulator::Now ().GetSeconds () << " s," << " Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;

            std::cout<< "Tx Packets = " << TxPacketsum << std::endl;
            std::cout<< "Rx Packets = " << RxPacketsum << std::endl;
            std::cout<< "Throughput: "<< tempThroughput <<" Kbps" << std::endl;
            std::cout<< "Delay: " << Delaysum/ RxPacketsum << std::endl;
            std::cout<< "LostPackets: " << LostPacketsum << std::endl;
            std::cout<< "Jitter: " << JitterSum/ (RxPacketsum - 1) << std::endl;
            std::cout << "Dropped Packets: " << DropPacketsum << std::endl;
            std::cout << "Packets Delivery Ratio: " << ( RxPacketsum * 100/ TxPacketsum) << "%" << std::endl;
            std::cout<<"------------------------------------------" << std::endl;

          }
          else
          {
            std::cout << "This is not UDP/TCP traffic" << std::endl;
          }
      }
    }
  // 每隔一秒打印一次
  Simulator::Schedule(Seconds(1), &PrintParams, fmhelper, monitor);
}



int
main (int argc, char *argv[])
{

  //设置默认拥塞控制算法  
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpHybla"));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpHighSpeed"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpVegas"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpScalable"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpVeno"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpBic"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpWestwood"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpWestwoodPlus"));

  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  /* RTS/CTS 一种半双工的握手协议 
  设置好RTS的阈值之后，如果超过这个阈值就会在发送信息之前先发送RTS，以减少干扰，
  相应的CTS会回应之前的RTS。一般都是AP发送CTS数据，而Station发送RTS数据。
  这里设置为1500，表示1500字节以上的frame要进行RTS/CTS机制
  */
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",UintegerValue (rtslimit));
  /* 设置最大WIFI覆盖距离为5m, 超出这个距离之后将无法传输WIFI信号 */
  //Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (5));
  
  /* 设置命令行参数 */
  CommandSetup (argc, argv) ;
  
  // Enabling Checksum computations
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  //LogComponentEnable ("TradHandoverScript", LOG_LEVEL_INFO);

  /*----- init Helpers ----- */
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));   // 5M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay
  
  /* 调用YansWifiChannelHelper::Default() 已经添加了默认的传播损耗模型, 下面不要再手动添加 */
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  ////////////////////////////////////////
  ////////////// LOSS MODEL //////////////
  ////////////////////////////////////////

  /* 
   * `FixedRssLossModel` will cause the `rss to be fixed` regardless
   * of the distance between the two stations, and the transmit power 
   *
   *
   *
   *
   *
   *
   */
  /* 传播延时速度是恒定的  */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* 很多地方都用这个，不知道什么意思  */
  // wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");  // !!! 加了这句之后AP和STA就无法连接了
  //wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  /* 不管发送功率是多少，都返回一个恒定的接收功率  */
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.SetChannel (wifiChannel.Create());
  WifiHelper wifi;
  /* The SetRemoteStationManager method tells the helper the type of `rate control algorithm` to use. 
   * Here, it is asking the helper to use the AARF algorithm
   */
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  WifiMacHelper wifiMac;

 
  NS_LOG_UNCOND ("------------Creating Nodes------------");
  NodeContainer switchesNode, apsNode, hostsNode;
  NodeContainer staWifi1Nodes, staWifi2Nodes, staWifi3Nodes;

  switchesNode. Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  apsNode.      Create (nAp);        //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  hostsNode.    Create (nHost);      //2 Nodes(terminal1 and terminal2)-----node 5,6

  staWifi1Nodes.Create(nAp1Station);    // node 7,8,9
  staWifi2Nodes.Create(nAp2Station);    // node 10,11,12,13
  staWifi3Nodes.Create(nAp3Station);    //  node 14
  
  Ptr<Node> ap1WifiNode = apsNode.Get (0);
  Ptr<Node> ap2WifiNode = apsNode.Get (1);
  Ptr<Node> ap3WifiNode = apsNode.Get (2);
  
  
  

  NS_LOG_UNCOND ("------------Creating Devices------------");
  /* CSMA Devices */
  NetDeviceContainer ap1CsmaDevice, ap2CsmaDevice, ap3CsmaDevice;
  NetDeviceContainer hostsDevice;
  NetDeviceContainer switch1Devices, switch2Devices;


  
  /* WIFI Devices */
  NetDeviceContainer stasWifi1Device, apWifi1Device;
  NetDeviceContainer stasWifi2Device, apWifi2Device;
  NetDeviceContainer stasWifi3Device, apWifi3Device;

  
  NS_LOG_UNCOND ("------------Building Topology-------------");

  NetDeviceContainer link;


  /* #1 Connect traditional switch1 to traditional switch2 */
  link = csma.Install (NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Devices. Add(link.Get(0));
  switch2Devices. Add(link.Get(1));
  
  /* #2 Connect AP1, AP2 and AP3 to traditional switch1 ！！！*/  
  for (uint32_t i = 0; i < nAp; i++)
  {
    link = csma.Install (NodeContainer(apsNode.Get(i), switchesNode.Get(0)));   // Get(0) for switch1
    if (i == 0)
    {
      ap1CsmaDevice. Add(link.Get(0));
    }
    else if (i == 1)
    {
      ap2CsmaDevice. Add(link.Get(0));
    }
    else if (i == 2)
    {
      ap3CsmaDevice. Add(link.Get(0));
    }
    switch1Devices. Add(link.Get(1));
  }


  /* #3 Connect host1 and host2 to traditonal switch2  */
  for (uint32_t i = 0; i < nHost; i++)
  {
    link = csma.Install (NodeContainer(hostsNode.Get(i), switchesNode.Get(1)));   // Get(1) for switch2
    hostsDevice. Add(link.Get(0));
    switch2Devices. Add(link.Get(1));
  }

  Ptr<Node> switch1Node = switchesNode.Get(0);
  Ptr<Node> switch2Node = switchesNode.Get(1);

  

  NS_LOG_UNCOND ("----------Configuring WIFI networks----------");
  Ssid ssid = Ssid ("ssid-default");
  //----------------------- Network AP1--------------------

  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (0 % 3) * 5));   // 1

  wifiMac.SetType ("ns3::StaWifiMac", 
                   "Ssid", SsidValue (ssid), 
                   "ActiveProbing", BooleanValue (false));
  stasWifi1Device = wifi.Install(wifiPhy, wifiMac, staWifi1Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  apWifi1Device   = wifi.Install(wifiPhy, wifiMac, ap1WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (1 % 3) * 5));    // 6  

  wifiMac.SetType ("ns3::StaWifiMac", 
                   "Ssid", SsidValue (ssid), 
                   "ActiveProbing", BooleanValue (false));
  stasWifi2Device = wifi.Install(wifiPhy, wifiMac, staWifi2Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  apWifi2Device   = wifi.Install(wifiPhy, wifiMac, ap2WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (2 % 3) * 5));    // 11

  wifiMac.SetType ("ns3::StaWifiMac", 
                  "Ssid", SsidValue (ssid), 
                  "ActiveProbing", BooleanValue (false));
  stasWifi3Device = wifi.Install(wifiPhy, wifiMac, staWifi3Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  apWifi3Device   = wifi.Install(wifiPhy, wifiMac, ap3WifiNode);

  MobilityHelper mobility1;
  /* for staWifi--1--Nodes */
  mobility1.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (60),
    "MinY",      DoubleValue (60),
    "DeltaX",    DoubleValue (40),
    "DeltaY",    DoubleValue (20),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility1.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (50, 150, 40, 120)));
  mobility1.Install (staWifi1Nodes);

  /* for staWifi--2--Nodes */
  MobilityHelper mobility2;
  mobility2.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (200),
    "MinY",      DoubleValue (60),
    "DeltaX",    DoubleValue (20),
    "DeltaY",    DoubleValue (20),
    "GridWidth", UintegerValue(2),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility2.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (150, 250, 40, 120)));
  mobility2.Install (staWifi2Nodes);
  
  /* for sta-1-Wifi-3-Node 要让Wifi3网络中的Sta1以恒定速度移动  */
  MobilityHelper mobConstantSpeed;
  mobConstantSpeed.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobConstantSpeed.Install (staWifi3Nodes.Get(0));  // Wifi-3中的第一个节点(即Node14)安装
  
  Ptr <ConstantVelocityMobilityModel> velocityModel = staWifi3Nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  velocityModel->SetPosition(mPosition);
  velocityModel->SetVelocity(mVelocity);


  /* for ConstantPosition Nodes */
  MobilityHelper mobConstantPosition;
  /* We want the AP to remain in a fixed position during the simulation 
   * only stations in AP1 and AP2 is mobile, the only station in AP3 is not mobile.
   */
  mobConstantPosition.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobConstantPosition.Install (apsNode);
  mobConstantPosition.Install (hostsNode);
  //mobConstantPosition.Install (staWifi3Nodes);
  mobConstantPosition.Install (switchesNode);
  
  NS_LOG_UNCOND ("----------Installing Bridge NetDevice----------");
  /*!!!!!!!!!!!! 关键的 BridgeHelper !!!!!!!!!!!*/
  BridgeHelper bridge1, bridge2;
  // 每个Switch中，与AP相连的CSMA网卡，和 与另一个Switch相连的CSMA网卡 构成一个`BridgeNetDevice`
  // 下面表示在某Node上安装 ns3::BridgeNetDevice
  bridge1. Install(switch1Node, switch1Devices);
  bridge2. Install(switch2Node, switch2Devices);

  // 每个AP中，CSMA网卡与WIFI网卡构成一个`BridgeNetDevice`
  BridgeHelper bridgeForAP1, bridgeForAP2, bridgeForAP3;
  bridgeForAP1. Install(apsNode.Get (0), NetDeviceContainer(apWifi1Device, ap1CsmaDevice));
  bridgeForAP2. Install(apsNode.Get (1), NetDeviceContainer(apWifi2Device, ap2CsmaDevice));
  bridgeForAP3. Install(apsNode.Get (2), NetDeviceContainer(apWifi3Device, ap3CsmaDevice));


  NS_LOG_UNCOND ("----------Installing Internet stack----------");


  /* Add internet stack to all the nodes, expect switches(交换机不用) */
  InternetStackHelper internet;

  internet.Install (apsNode);
  internet.Install (hostsNode);
  internet.Install (switchesNode); // 给交换机撞上Internet之后，交换机有了回环网卡
  internet.Install (staWifi1Nodes);
  internet.Install (staWifi2Nodes);
  internet.Install (staWifi3Nodes);

  
  NS_LOG_UNCOND ("-----------Assigning IP Addresses.-----------");


  Ipv4AddressHelper ip;
  ip.SetBase ("10.0.0.0",    "255.255.255.0");

  Ipv4InterfaceContainer h1h2Interface;
  Ipv4InterfaceContainer stasWifi1Interface;
  Ipv4InterfaceContainer stasWifi2Interface;
  Ipv4InterfaceContainer stasWifi3Interface;




  h1h2Interface      = ip.Assign (hostsDevice);   // 10.0.0.1~2

  // 供三组STA
  stasWifi1Interface = ip.Assign (stasWifi1Device); // 10.0.0.0.3~5
  stasWifi2Interface = ip.Assign (stasWifi2Device); // 10.0.0.0.6~9
  stasWifi3Interface = ip.Assign (stasWifi3Device); // 10.0.0.0.10



  NS_LOG_UNCOND ("-----------Creating Applications.-----------");
  uint16_t port = 9;   // Discard port (RFC 863)
  
  

  /*
  // UDP server
  UdpServerHelper server (port);  // for the server side, only one param(port) is specified
  // for node 6
  ApplicationContainer serverApps = server.Install (hostsNode.Get(1));
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (Seconds(stopTime));  
  

  // UDP client
  UdpClientHelper client (h1h2Interface.GetAddress(1) ,port);   // stasWifi2Interface.GetAddress(0)
  client.SetAttribute ("MaxPackets", UintegerValue (nMaxPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds(nInterval)));  
  client.SetAttribute ("PacketSize", UintegerValue (nPacketSize));
  // for node 14
  ApplicationContainer clientApps = client.Install(staWifi3Nodes.Get(0));
  // for node 10
  //ApplicationContainer clientApps = client.Install(staWifi2Nodes.Get(0));
  // for node 5
  //ApplicationContainer clientApps = client.Install(hostsNode.Get(0));
  clientApps.Start (Seconds(1.1));  
  clientApps.Stop (Seconds(stopTime));
  */
  


  
  // TCP server
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds(stopTime));


  
  // TCP client
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
  ApplicationContainer sourceApps = source.Install (staWifi3Nodes.Get(0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));
  



  // Create a source to send packets.  Instead of a full Application
  // and the helper APIs you might see in other example files, this example
  // will use sockets directly and register some socket callbacks as a sending
  // "Application".

  // Create and bind the socket...
  //Ptr<Socket> localSocket =
    //Socket::CreateSocket (staWifi3Nodes.Get(0), TcpSocketFactory::GetTypeId ());
  //localSocket->Bind ();


  // Trace changes to the congestion window
  //Config::ConnectWithoutContext ("/NodeList/14/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

  // ...and schedule the sending "Application"; This is similar to what an 
  // ns3::Application subclass would do internally.
  //Simulator::ScheduleNow (&StartFlow, localSocket,
                          //h1h2Interface.GetAddress (1), port);

  // One can toggle the comment for the following line on or off to see the
  // effects of finite send buffer modelling.  One can also change the size of
  // said buffer.

  //localSocket->SetAttribute("SndBufSize", UintegerValue(1024));

  NS_LOG_UNCOND ("-----------Configuring Tracing.-----------");

  /**
   * Configure tracing of all enqueue, dequeue, and NetDevice receive events.
   * Trace output will be sent to the file as below
   */
  if (tracing)
    {
      //AsciiTraceHelper ascii;
      //csma.EnablePcapAll("goal-topo-trad");
      //csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo-trad-handover/goal-topo-trad-handover.tr"));
      wifiPhy.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-handover-ap1-wifi", apWifi1Device);
      wifiPhy.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-handover-ap2-wifi", apWifi2Device);
      //wifiPhy.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-handover-ap2-sta1-wifi", stasWifi2Device);
      wifiPhy.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-handover-ap3-wifi", apWifi3Device);
      wifiPhy.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-handover-ap3-sta1-wifi", stasWifi3Device);
      // WifiMacHelper doesnot have `EnablePcap()` method
      //csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-switch1-csma", switch1Devices);
      //csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-switch2-csma", switch2Devices);
      //csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-ap1-csma", ap1CsmaDevice);
      //csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-ap2-csma", ap2CsmaDevice);
      //csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-ap3-csma", ap3CsmaDevice);
      csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-H1-csma", hostsDevice.Get(0));
      csma.EnablePcap ("goal-topo-trad-handover/goal-topo-trad-H2-csma", hostsDevice.Get(1));
    }


  AnimationInterface anim ("goal-topo-trad-handover/goal-topo-trad-handover.xml");
  anim.SetConstantPosition(switch1Node,200,0);             // s1-----node 0
  anim.SetConstantPosition(switch2Node,400,0);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),100,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),200,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),300,20);      // Ap3----node 4
  anim.SetConstantPosition(hostsNode.Get(0),350,60);    // H1-----node 5
  anim.SetConstantPosition(hostsNode.Get(1),400,60);    // H2-----node 6
  //anim.SetConstantPosition(staWifi3Nodes.Get(0),55,40);  //   -----node 14

  anim.EnablePacketMetadata();   // to see the details of each packet




  NS_LOG_UNCOND ("------------Preparing for Checking all the params.------------");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds(stopTime));
/*----------------------------------------------------------------------*/
  
  std::string base = "goal-topo-trad-handover__";
  //Throughput
  std::string throu = base + "ThroughputVSTime";
  std::string graphicsFileName        = throu + ".png";
  std::string plotFileName            = throu + ".plt";
  std::string plotTitle               = "Throughput vs Time";
  std::string dataTitle               = "Throughput";
  Gnuplot gnuplot (graphicsFileName);
  gnuplot.SetTitle (plotTitle);
  gnuplot.SetTerminal ("png");
  gnuplot.SetLegend ("Time", "Throughput");
  Gnuplot2dDataset dataset;
  dataset.SetTitle (dataTitle);
  dataset.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset.SetErrorBars (Gnuplot2dDataset::XY);
  //Delay
  std::string delay = base + "DelayVSTime";
  std::string graphicsFileName1        = delay + ".png";
  std::string plotFileName1            = delay + ".plt";
  std::string plotTitle1               = "Delay vs Time";
  std::string dataTitle1               = "Delay";
  Gnuplot gnuplot1 (graphicsFileName1);
  gnuplot1.SetTitle (plotTitle1);
  gnuplot1.SetTerminal ("png");
  gnuplot1.SetLegend ("Time", "Delay");
  Gnuplot2dDataset dataset1;
  dataset1.SetTitle (dataTitle1);
  dataset1.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset1.SetErrorBars (Gnuplot2dDataset::XY);
  //LostPackets
  std::string lost = base + "LostPacketsVSTime";
  std::string graphicsFileName2        = lost + ".png";
  std::string plotFileName2            = lost + ".plt";
  std::string plotTitle2               = "LostPackets vs Time";
  std::string dataTitle2               = "LostPackets";
  Gnuplot gnuplot2 (graphicsFileName2);
  gnuplot2.SetTitle (plotTitle2);
  gnuplot2.SetTerminal ("png");
  gnuplot2.SetLegend ("Time", "LostPackets");
  Gnuplot2dDataset dataset2;
  dataset2.SetTitle (dataTitle2);
  dataset2.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset2.SetErrorBars (Gnuplot2dDataset::XY);
  //Jitter
  std::string jitter = base + "JitterVSTime";
  std::string graphicsFileName3        = jitter + ".png";
  std::string plotFileName3            = jitter + ".plt";
  std::string plotTitle3               = "Jitter vs Time";
  std::string dataTitle3               = "Jitter";
  Gnuplot gnuplot3 (graphicsFileName3);
  gnuplot3.SetTitle (plotTitle3);
  gnuplot3.SetTerminal ("png");
  gnuplot3.SetLegend ("Time", "Jitter");
  Gnuplot2dDataset dataset3;
  dataset3.SetTitle (dataTitle3);
  dataset3.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset3.SetErrorBars (Gnuplot2dDataset::XY);

/*-----------------------------------------------------*/
  // 测吞吐量, 延时, 丢包, 抖动, 最后打印出这些参数
  ThroughputMonitor (&flowmon, monitor, dataset);
  DelayMonitor      (&flowmon, monitor, dataset1);
  LostPacketsMonitor(&flowmon, monitor, dataset2);
  JitterMonitor     (&flowmon, monitor, dataset3);
  PrintParams       (&flowmon, monitor);
/*-----------------------------------------------------*/


  NS_LOG_UNCOND ("------------Running Simulation.------------");
  Simulator::Run ();

  //Throughput
  gnuplot.AddDataset (dataset);
  std::ofstream plotFile (plotFileName.c_str());
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();
  //Delay
  gnuplot1.AddDataset (dataset1);
  std::ofstream plotFile1 (plotFileName1.c_str());
  gnuplot1.GenerateOutput (plotFile1);
  plotFile1.close ();
  //LostPackets
  gnuplot2.AddDataset (dataset2);
  std::ofstream plotFile2 (plotFileName2.c_str());
  gnuplot2.GenerateOutput (plotFile2);
  plotFile2.close ();
  //Jitter
  gnuplot3.AddDataset (dataset3);
  std::ofstream plotFile3 (plotFileName3.c_str());
  gnuplot3.GenerateOutput (plotFile3);
  plotFile3.close ();


  //monitor->SerializeToXmlFile("goal-topo-trad-handover/goal-topo-trad-handover.flowmon", true, true);
  
  /* the SerializeToXmlFile () function 2nd and 3rd parameters 
   * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
   */
  Simulator::Destroy ();
}
