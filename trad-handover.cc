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


double stopTime = 40.0;  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nHost       = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 20;   //使AP2过载
uint32_t nAp3Station = 1;


double nSamplingPeriod = 0.2;   // 抽样间隔，根据总的Simulation时间做相应的调整


/* for udp-server-client application. */
uint32_t nMaxPackets = 20000;    // The maximum packets to be sent.
double nInterval  = 0.1;  // The interval between two packet sent.
uint32_t nUdpPacketSize = 1024;

/* for tcp-bulk-send application. */   
//uint32_t nMaxBytes = 1000000000;  //Zero is unlimited. 100M
uint32_t nMaxBytes = 0;


// 1500字节以下的帧不需要RTS/CTS
uint32_t rtslimit = 1500;

uint32_t MaxRange = 100;

/* 恒定速度移动节点的
初始位置 x = 0.0, y = 40.0
和
移动速度 x = 10.0,  y=  0.0
*/
Vector3D mPosition = Vector3D(160.0, 120.0, 0.0);
Vector3D mVelocity = Vector3D(0.0, -5.0 , 0.0);

// 设置各个AP的传输信号强度(dBm为单位)，必须得为正值，否则不能发送。而且越大表示信号越强。
double ap1TxPwr = 90;
double ap2TxPwr = 100;
double ap3TxPwr = 90;



Ipv4Address serverIp;   // UDP/TCP的server IP
Ipv4Address clientIp;   // UDP/TCP的client IP



////////////////////// 函数声明 ///////////////////
bool CommandSetup(int argc, char **argv);

void ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset);
void LostPacketsMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset2);
void DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset1);
void JitterMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset3);
void PrintParams (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor);

// 监听服务器端网卡丢的包
// 增强版
/*
static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p);
*/

// 简单版
static void
RxDrop (Ptr<const Packet> p);
//  trace传输的包
/*static void PhyTxTrace (std::string path, Ptr<const Packet> packet, 
  uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, 
  bool isShortPreamble, uint8_t txPower);
*/
// trace接收的包
/*static void PhyRxTrace (std::string path, Ptr<const Packet> packet, 
  uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, 
  bool isShortPreamble, double signalDbm, double noiseDbm);
*/
////////////////////// 函数声明 ///////////////////



/*
要使在handover的时候throughput降低到0，可对AP进行调整，使得AP与STA到某个距离的时候信号降到0，使得连接断开
*/
int
main (int argc, char *argv[])
{
  
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mbps"));
  //设置默认拥塞控制算法
  // ns-3.24   ///////
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpTahoe"));
  // ns-3.25后 ////////
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpHybla"));
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpHighSpeed"));
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
  /* 
  x^2 = 20^2 + 50^ => 50 < x < 60
  设置最大WIFI覆盖距离为50m(这样一个STA在与某个AP断开连接到与下一个AP连接上的时间之间会有一个间隔时间), 超出这个距离之后将无法传输WIFI信号 
  */
  Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (MaxRange));
  
  /* 设置命令行参数 */
  CommandSetup (argc, argv) ;
  
  // Enabling Checksum computations
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  //LogComponentEnable ("TradHandoverScript", LOG_LEVEL_INFO);

  /*----- init Helpers ----- */
  CsmaHelper csma;
  
  /* 调用YansWifiChannelHelper::Default() 已经添加了默认的传播损耗模型, 下面不要再手动添加 */
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> yansWifiChannel = wifiChannel.Create();
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
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel");
  // 一个给AP，一个给STA
  YansWifiPhyHelper wifiPhyAP = YansWifiPhyHelper::Default();
  YansWifiPhyHelper wifiPhySTA = YansWifiPhyHelper::Default();

  wifiPhyAP.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhyAP.SetChannel (yansWifiChannel);
  wifiPhySTA.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhySTA.SetChannel (yansWifiChannel);

  WifiHelper wifi;
  /* The SetRemoteStationManager method tells the helper the type of `rate control algorithm` to use. 
   * Here, it is asking the helper to use the AARF algorithm
   */
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ); // 貌似只能在ns-3.25支持
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  //QosWifiMacHelper wifiMac;
  // 一个给AP，一个给STA
  NqosWifiMacHelper wifiMacAP = NqosWifiMacHelper::Default ();
  NqosWifiMacHelper wifiMacSTA = NqosWifiMacHelper::Default ();

 
  NS_LOG_UNCOND ("------------Creating Nodes------------");
  NodeContainer switchesNode, apsNode, hostsNode;
  NodeContainer staWifiNodes[nAp]; // 数组，包含nAp个NodeContainer,而每个NodeContainer里面包含各自WIFI网络中的节点
  for (size_t i = 0; i < nAp; i++)
  {
    staWifiNodes[i] = NodeContainer();
  }

  switchesNode. Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  apsNode.      Create (nAp);        //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  hostsNode.    Create (nHost);      //2 Nodes(terminal1 and terminal2)-----node 5,6

  staWifiNodes[0].Create(nAp1Station);    // node 7,8,9
  staWifiNodes[1].Create(nAp2Station);    // node 10,11,12,13
  staWifiNodes[2].Create(nAp3Station);    //  node 14
  
  Ptr<Node> ap1WifiNode = apsNode.Get (0);
  Ptr<Node> ap2WifiNode = apsNode.Get (1);
  Ptr<Node> ap3WifiNode = apsNode.Get (2);
  
  
  

  NS_LOG_UNCOND ("------------Creating Devices------------");
  /* CSMA Devices */
  NetDeviceContainer apCsmaDevices[nAp];
  for (size_t i = 0; i < nAp; i++)
  {
    apCsmaDevices[i] = NetDeviceContainer();
  }

  NetDeviceContainer hostDevices;
  
  NetDeviceContainer switchDevices[nSwitch];  //两个switch的网卡们(ports)
  for (size_t i = 0; i < nSwitch; i++)
  {
    switchDevices[i] = NetDeviceContainer();
  }


  
  /* WIFI Devices */
  NetDeviceContainer stasWifiDevices[nAp]; // 各个WIFI网络的STA的数组，每个NetDeviceContainer里包含各自WIFI网络的网卡
  for (size_t i = 0; i < nAp; i++)
  {
    stasWifiDevices[i] = NetDeviceContainer();
  }
  NetDeviceContainer apWifiDevices[nAp]; // 各个WIFI网络的AP数组，每个NetDeviceContainer里包含各种WIFI网络的网卡
  for (size_t i = 0 ; i <nAp; i++)
  {
    apWifiDevices[i] = NetDeviceContainer();
  }

  
  NS_LOG_UNCOND ("------------Building Topology-------------");

  NetDeviceContainer link;


  /* #1 Connect traditional switch1 to traditional switch2 */
  // switch1 和 switch2 之间的线路作为主干线路, 应该是100M的
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  link = csma.Install (NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switchDevices[0]. Add(link.Get(0));
  switchDevices[1]. Add(link.Get(1));
  
  /* #2 Connect AP1, AP2 and AP3 to traditional switch1 ！！！*/
  // 各个AP到switch1的线路做一下限制, 每个AP到switch1只能有30M
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("30Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  for (uint32_t i = 0; i < nAp; i++)
  {
    link = csma.Install (NodeContainer(apsNode.Get(i), switchesNode.Get(0)));   // Get(0) for switch1
    if (i == 0)
    {
      apCsmaDevices[0]. Add(link.Get(0));
    }
    else if (i == 1)
    {
      apCsmaDevices[1]. Add(link.Get(0));
    }
    else if (i == 2)
    {
      apCsmaDevices[2]. Add(link.Get(0));
    }
    switchDevices[0]. Add(link.Get(1));
  }


  /* #3 Connect host1 and host2 to traditonal switch2  */
  for (uint32_t i = 0; i < nHost; i++)
  {
    link = csma.Install (NodeContainer(hostsNode.Get(i), switchesNode.Get(1)));   // Get(1) for switch2
    hostDevices. Add(link.Get(0));
    switchDevices[1]. Add(link.Get(1));
  }

  

  NS_LOG_UNCOND ("----------Configuring WIFI networks----------");
  Ssid ssid = Ssid ("ssid-default");
  //Ssid ssid1 = Ssid ("ssid-1");
  //Ssid ssid2 = Ssid ("ssid-2");
  //Ssid ssid3 = Ssid ("ssid-3");

  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (0 % 3) * 5));   // 1
  
  // ------------------- 配置STA --------------------
  wifiMacSTA.SetType ("ns3::StaWifiMac", 
                   "Ssid", SsidValue (ssid), 
                   "ActiveProbing", BooleanValue (false));//
  stasWifiDevices[0] = wifi.Install(wifiPhySTA, wifiMacSTA, staWifiNodes[0] );

  wifiMacSTA.SetType ("ns3::StaWifiMac", 
                   "Ssid", SsidValue (ssid), 
                   "ActiveProbing", BooleanValue (false));//
  stasWifiDevices[1] = wifi.Install(wifiPhySTA, wifiMacSTA, staWifiNodes[1] );

  wifiMacSTA.SetType ("ns3::StaWifiMac", 
                   "Ssid", SsidValue (ssid), 
                   "ActiveProbing", BooleanValue (false));//变成false似乎还快一些
  stasWifiDevices[2] = wifi.Install(wifiPhySTA, wifiMacSTA, staWifiNodes[2] );
  // ------------------- 配置STA --------------------

  // ------------------- 配置AP --------------------
  wifiMacAP.SetType ("ns3::ApWifiMac", 
                   "Ssid", SsidValue (ssid));
  wifiPhyAP.Set("TxPowerStart", DoubleValue(ap1TxPwr));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(ap1TxPwr));
  apWifiDevices[0]   = wifi.Install(wifiPhyAP, wifiMacAP, ap1WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (1 % 3) * 5));    // 6  

  wifiMacAP.SetType ("ns3::ApWifiMac", 
                   "Ssid", SsidValue (ssid));
  wifiPhyAP.Set("TxPowerStart", DoubleValue(ap2TxPwr));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(ap2TxPwr));

  apWifiDevices[1]   = wifi.Install(wifiPhyAP, wifiMacAP, ap2WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (2 % 3) * 5));    // 11



  wifiMacAP.SetType ("ns3::ApWifiMac", 
                   "Ssid", SsidValue (ssid));
  wifiPhyAP.Set("TxPowerStart", DoubleValue(ap3TxPwr));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(ap3TxPwr));
  apWifiDevices[2]   = wifi.Install(wifiPhyAP, wifiMacAP, ap3WifiNode);
  // ------------------- 配置AP --------------------


  MobilityHelper mobility1;
  /* for staWifi--1--Nodes */
  mobility1.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (80),
    "MinY",      DoubleValue (40),
    "DeltaX",    DoubleValue (20),
    "DeltaY",    DoubleValue (20),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility1.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (50, 150, 40, 120)));
  mobility1.Install (staWifiNodes[0]);

  /* for staWifi--2--Nodes */
  MobilityHelper mobility2;
  mobility2.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (150), // 指这一组节点的起始节点的x轴坐标
    "MinY",      DoubleValue (5),  // 指这一组节点的起始节点的y轴坐标
    "DeltaX",    DoubleValue (20),  // x轴方向间隔
    "DeltaY",    DoubleValue (10),  // y轴方向间隔
    "GridWidth", UintegerValue(5), // 指网格的宽度为几个节点
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility2.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (100, 250, 0, 40))); // 活动范围矩形框
  mobility2.Install (staWifiNodes[1]);
  
  /* for sta-1-Wifi-3-Node 要让Wifi3网络中的Sta1以恒定速度移动  */
  MobilityHelper mobConstantSpeed;
  mobConstantSpeed.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobConstantSpeed.Install (staWifiNodes[2].Get(0));  // Wifi-3中的第一个节点(即Node14)安装
  
  Ptr <ConstantVelocityMobilityModel> velocityModel = staWifiNodes[2].Get(0)->GetObject<ConstantVelocityMobilityModel>();
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
  //mobConstantPosition.Install (staWifiNodes[2]);
  mobConstantPosition.Install (switchesNode);
  
  NS_LOG_UNCOND ("----------Installing Bridge NetDevice----------");
  /*!!!!!!!!!!!! 关键的 BridgeHelper !!!!!!!!!!!*/
  BridgeHelper bridge1, bridge2;
  // 每个Switch中，与AP相连的CSMA网卡，和 与另一个Switch相连的CSMA网卡 构成一个`BridgeNetDevice`
  // 下面表示在某Node上安装 ns3::BridgeNetDevice
  bridge1. Install(switchesNode.Get(0), switchDevices[0]);
  bridge2. Install(switchesNode.Get(1), switchDevices[1]);

  // 每个AP中，CSMA网卡与WIFI网卡构成一个`BridgeNetDevice`
  BridgeHelper bridgeForAP1, bridgeForAP2, bridgeForAP3;
  bridgeForAP1. Install(apsNode.Get (0), NetDeviceContainer(apWifiDevices[0], apCsmaDevices[0]));
  bridgeForAP2. Install(apsNode.Get (1), NetDeviceContainer(apWifiDevices[1], apCsmaDevices[1]));
  bridgeForAP3. Install(apsNode.Get (2), NetDeviceContainer(apWifiDevices[2], apCsmaDevices[2]));


  NS_LOG_UNCOND ("----------Installing Internet stack----------");


  /* Add internet stack to all the nodes, expect switches(交换机不用) */
  InternetStackHelper internet;

  internet.Install (apsNode);
  internet.Install (hostsNode);
  internet.Install (switchesNode); // 给交换机撞上Internet之后，交换机有了回环网卡
  internet.Install (staWifiNodes[0]);
  internet.Install (staWifiNodes[1]);
  internet.Install (staWifiNodes[2]);

  
  NS_LOG_UNCOND ("-----------Assigning IP Addresses.-----------");


  Ipv4AddressHelper ip;
  ip.SetBase ("10.0.0.0",    "255.255.255.0");

  Ipv4InterfaceContainer h1h2Interface;
  Ipv4InterfaceContainer stasWifi1Interface;
  Ipv4InterfaceContainer stasWifi2Interface;
  Ipv4InterfaceContainer stasWifi3Interface;




  h1h2Interface      = ip.Assign (hostDevices);   // 10.0.0.1~2

  // 供三组STA
  stasWifi1Interface = ip.Assign (stasWifiDevices[0]); // 10.0.0.0.3~5
  stasWifi2Interface = ip.Assign (stasWifiDevices[1]); 
  stasWifi3Interface = ip.Assign (stasWifiDevices[2]); 



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
  client.SetAttribute ("PacketSize", UintegerValue (nUdpPacketSize));
  // for node 14
  ApplicationContainer clientApps = client.Install(staWifiNodes[2].Get(0));
  // for node 10
  //ApplicationContainer clientApps = client.Install(staWifiNodes[1].Get(0));
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
  /*
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
  ApplicationContainer sourceApps = source.Install (staWifiNodes[2].Get(0));    // AP3内的第 0 个STA
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (stopTime));
  

  // 给3 个AP1 的stations 加上 BulkSender
  for (uint32_t i = 0; i < nAp1Station; i++)
  {
    BulkSendHelper ap1Source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port)); // 服务器的IP
    // Set the amount of data to send in bytes.  Zero is unlimited.
    ap1Source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
    ApplicationContainer ap1sourceApps = ap1Source.Install (staWifiNodes[0].Get(i));  // AP1内的第 i 个STA
    ap1sourceApps.Start (Seconds (1.0));
    ap1sourceApps.Stop (Seconds (stopTime));
  }


  // 给20 个AP2 的stations 加上 BulkSender
  for (uint32_t i = 0; i < nAp2Station; i++)
  {
    BulkSendHelper ap2Source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));  // 服务器的IP
    // Set the amount of data to send in bytes.  Zero is unlimited.
    ap2Source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
    ApplicationContainer ap2sourceApps = ap2Source.Install (staWifiNodes[1].Get(i));  // AP2内的第 i 个STA
    ap2sourceApps.Start (Seconds (1.0));
    ap2sourceApps.Stop (Seconds (stopTime));
  }
  */

  ApplicationContainer clientApps;
  // 给3 个AP1 的stations 加上 OnOffApplication
  for (uint32_t i = 0; i < nAp1Station; i++)
  {
      OnOffHelper ap1OnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (h1h2Interface.GetAddress(1), port));
      ap1OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      ap1OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      ap1OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));
      
      clientApps.Add( ap1OnOffHelper.Install (staWifiNodes[0].Get(i)) );
  }

  // 给20 个AP2 的stations 加上 OnOffApplication
  for (uint32_t i = 0; i < nAp2Station; i++)
  {
      OnOffHelper ap2OnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (h1h2Interface.GetAddress(1), port));
      ap2OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      ap2OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      ap2OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));
      
      clientApps.Add( ap2OnOffHelper.Install (staWifiNodes[1].Get(i)) );
  }

  // 给移动的STA加上 OnOffApplication
  OnOffHelper staOnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (h1h2Interface.GetAddress(1), port));
  staOnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  staOnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  staOnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.001)));

  clientApps.Add( staOnOffHelper.Install (staWifiNodes[2].Get(0)) );



  // 设置全局变量server IP 和client IP的值，供下面的测延时、吞吐量、抖动、丢包等使用
  serverIp = h1h2Interface.GetAddress(1);
  clientIp = stasWifi3Interface.GetAddress(0);

  // Create a source to send packets.  Instead of a full Application
  // and the helper APIs you might see in other example files, this example
  // will use sockets directly and register some socket callbacks as a sending
  // "Application".

  // Create and bind the socket...
  //Ptr<Socket> localSocket =
    //Socket::CreateSocket (staWifiNodes[2].Get(0), TcpSocketFactory::GetTypeId ());
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
      //csma.EnableAsciiAll (ascii.CreateFileStream ("trad-handover/trad-handover.tr"));
      wifiPhyAP.EnablePcap ("trad-handover/trad-handover-ap1-wifi", apWifiDevices[0]);
      wifiPhyAP.EnablePcap ("trad-handover/trad-handover-ap2-wifi", apWifiDevices[1]);
      //wifiPhy.EnablePcap ("trad-handover/trad-handover-ap2-sta1-wifi", stasWifiDevices[1]);
      wifiPhyAP.EnablePcap ("trad-handover/trad-handover-ap3-wifi", apWifiDevices[2]);
      wifiPhySTA.EnablePcap ("trad-handover/trad-handover-sta-wifi", stasWifiDevices[2].Get(0));  //只有一个
      // WifiMacHelper doesnot have `EnablePcap()` method
      //csma.EnablePcap ("trad-handover/trad-switch1-csma", switchDevices[0]);
      //csma.EnablePcap ("trad-handover/trad-switch2-csma", switchDevices[1]);
      //csma.EnablePcap ("trad-handover/trad-ap1-csma", apCsmaDevices[0]);
      //csma.EnablePcap ("trad-handover/trad-ap2-csma", apCsmaDevices[1]);
      //csma.EnablePcap ("trad-handover/trad-ap3-csma", apCsmaDevices[2]);
      //csma.EnablePcap ("trad-handover/trad-H1-csma", hostDevices.Get(0));
      csma.EnablePcap ("trad-handover/trad-H2-csma", hostDevices.Get(1));
    }


  AnimationInterface anim ("trad-handover/trad-handover.xml");
  anim.SetConstantPosition(switchesNode.Get(0),200,0);             // s1-----node 0
  anim.SetConstantPosition(switchesNode.Get(1),400,0);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),100,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),200,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),180,100);      // Ap3----node 4
  anim.SetConstantPosition(hostsNode.Get(0),350,60);    // H1-----node 5
  anim.SetConstantPosition(hostsNode.Get(1),400,60);    // H2-----node 6
  //anim.SetConstantPosition(staWifiNodes[2].Get(0),55,40);  //   -----node 14

  anim.EnablePacketMetadata();   // to see the details of each packet

  NS_LOG_UNCOND ("------------Configuring Global variables------------");
  /* `MonitorSnifferTx` : Trace source simulating the capability of a wifi device 
  in monitor mode to sniff all frames being transmitted
  */
  //Config::Connect ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&PhyRxTrace));

  /* `MonitorSnifferRx` : Trace source simulating a wifi device in monitor mode 
  sniffing all received frames
  */
  // Node2 为AP1，Node3 为AP2， Node4 为AP3
  

  //for (uint32_t i = 2; i < 5; i++)
  //{
    //Config::Connect ("/NodeList/" + i + "/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
  //}
  
  // 对接收TCP流量的服务器的网卡(hostsDevices的第二张网卡)监听其丢的包
  /*增强版
  PcapHelper pcapHelper;
  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("sixth.pcap", std::ios::out, PcapHelper::DLT_PPP);
  devices.Get (1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback (&RxDrop, file));
  */

  // 简单版
  hostDevices.Get (1)->TraceConnectWithoutContext("PhyRxDrop", MakeCallback (&RxDrop));


  NS_LOG_UNCOND ("------------Preparing for Checking all the params.------------");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds(stopTime));
/*----------------------------------------------------------------------*/
  
  std::string base = "trad-handover__";
  //Throughput
  std::string throu = base + "ThroughputVSTime";
  std::string graphicsFileName        = throu + ".eps";
  std::string plotFileName            = throu + ".plt";
  std::string plotTitle               = "Throughput vs Time";
  std::string dataTitle               = "Throughput";
  Gnuplot gnuplot (graphicsFileName);
  gnuplot.SetTitle (plotTitle);
  //gnuplot.SetTerminal ("png");
  gnuplot.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
  gnuplot.SetLegend ("Time(seconds)", "Throughput(Kbit/s)");
  //gnuplot.AppendExtra ("set xrange [10:35]");
  Gnuplot2dDataset dataset;
  dataset.SetTitle (dataTitle);
  dataset.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset.SetErrorBars (Gnuplot2dDataset::XY);
  //Delay
  std::string delay = base + "DelayVSTime";
  std::string graphicsFileName1        = delay + ".eps";
  std::string plotFileName1            = delay + ".plt";
  std::string plotTitle1               = "Delay vs Time";
  std::string dataTitle1               = "Delay";
  Gnuplot gnuplot1 (graphicsFileName1);
  gnuplot1.SetTitle (plotTitle1);
  //gnuplot1.SetTerminal ("png");
  gnuplot1.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");  
  gnuplot1.SetLegend ("Time(seconds)", "Delay(seconds)");
  //gnuplot1.AppendExtra ("set xrange [10:35]");
  Gnuplot2dDataset dataset1;
  dataset1.SetTitle (dataTitle1);
  dataset1.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset1.SetErrorBars (Gnuplot2dDataset::XY);
  //LostPackets
  std::string lost = base + "LostPacketsVSTime";
  std::string graphicsFileName2        = lost + ".eps";
  std::string plotFileName2            = lost + ".plt";
  std::string plotTitle2               = "LostPackets vs Time";
  std::string dataTitle2               = "LostPackets";
  Gnuplot gnuplot2 (graphicsFileName2);
  gnuplot2.SetTitle (plotTitle2);
  //gnuplot2.SetTerminal ("png");
  gnuplot2.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
  gnuplot2.SetLegend ("Time(seconds)", "LostPackets");
  //gnuplot2.AppendExtra ("set xrange [10:35]");
  Gnuplot2dDataset dataset2;
  dataset2.SetTitle (dataTitle2);
  dataset2.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset2.SetErrorBars (Gnuplot2dDataset::XY);
  //Jitter
  std::string jitter = base + "JitterVSTime";
  std::string graphicsFileName3        = jitter + ".eps";
  std::string plotFileName3            = jitter + ".plt";
  std::string plotTitle3               = "Jitter vs Time";
  std::string dataTitle3               = "Jitter";
  Gnuplot gnuplot3 (graphicsFileName3);
  gnuplot3.SetTitle (plotTitle3);
  //gnuplot3.SetTerminal ("png");
  gnuplot3.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
  gnuplot3.SetLegend ("Time(seconds)", "Jitter(seconds)");
  //gnuplot3.AppendExtra ("set xrange [10:35]");
  Gnuplot2dDataset dataset3;
  dataset3.SetTitle (dataTitle3);
  dataset3.SetStyle (Gnuplot2dDataset::POINTS);
  //dataset3.SetErrorBars (Gnuplot2dDataset::XY);

/*-----------------------------------------------------*/
  // 测吞吐量, 延时, 丢包, 抖动
  ThroughputMonitor (&flowmon, monitor, dataset);
  DelayMonitor      (&flowmon, monitor, dataset1);
  LostPacketsMonitor(&flowmon, monitor, dataset2);
  JitterMonitor     (&flowmon, monitor, dataset3);
  // 打印出各种参数
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


  //monitor->SerializeToXmlFile("trad-handover/trad-handover.flowmon", true, true);
  
  /* the SerializeToXmlFile () function 2nd and 3rd parameters 
   * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
   */
  Simulator::Destroy ();
}

/////////////////////////////////////////////
///////////////// 函数定义 ///////////////////
bool
CommandSetup (int argc, char **argv)
{

  CommandLine cmd;

  cmd.AddValue ("SamplingPeriod", "Sampling period", nSamplingPeriod);
  cmd.AddValue ("stopTime", "The time to stop", stopTime);
  
  /* for udp-server-client application */
  // cmd.AddValue ("MaxPackets", "The total packets available to be scheduled by the UDP application.", nMaxPackets);
  // cmd.AddValue ("Interval", "The interval between two packet sent", nInterval);
  // cmd.AddValue ("PacketSize", "The size in byte of each packet", nUdpPacketSize);


  cmd.AddValue ("rtslimit", "The size of packets under which there should be RST/CST", rtslimit);
  cmd.AddValue ("MaxRange", "The max range within which the STA could receive signal", MaxRange);
  
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

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
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
      if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
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

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
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

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
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

void PrintParams (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor)
{
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

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
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
            std::cout << "Packets Delivery Ratio: " << ( RxPacketsum * 100 / TxPacketsum) << "%" << std::endl;
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

/*增强版
static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  file->Write(Simulator::Now(), p);
}
*/

//简单版
static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}

//static void PhyRxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, double signalDbm, double noiseDbm)
//{
  /*
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  currentPacket->RemoveHeader (hdr);
  TotRxBytes += currentPacket->GetSize ();

   if (hdr.IsData ())
     TotRxPkts++;
  
  PhyState = signalDbm - noiseDbm;
  */
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
  
  //NS_LOG_UNCOND ("Number of bytes received now is " << TotRxBytes << "\n");
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
//}

//static void PhyTxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, uint8_t txPower)
//{
  /*
  int psize;
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  currentPacket->RemoveHeader (hdr);
  psize = currentPacket->GetSize ();
  TotTxBytes += psize;

  if (hdr.IsData ())
  //if (psize == 20)
    TotTxPkts++;
  */
     
  //channelOccuPercentage = (double) (TotTxBytes * 8) / (double) (6000000 * 10);
  //std::cout << "Channel Occupency is " << channelOccuPercentage * 100 << "%\n";

  //NS_LOG_UNCOND ("A packet has been sent.\n");
  //NS_LOG_UNCOND ("Number of bytes sent now is " << TotTxBytes << "\n");
//}
