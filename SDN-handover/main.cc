// Network topology
//
//           ----Controller---
//            |             |
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
//#include "ns3/constant-velocity-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
//#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
//#include "ns3/openflow-module.h"
#include "ns3/ofswitch13-module.h"
#include "ns3/log.h"
// 用来构造 BridgeNetDevice 的
#include "ns3/bridge-module.h"

// 定制的Controller
//#include "qos-controller.h"

//包含 `gnuplot`和`Gnuplot2Ddatabase`
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/netanim-module.h"

#include "ns3/ipv4-click-routing.h"
//#include "ns3/click-internet-stack-helper.h"
#include "my-learning-controller.h"

#include <iostream>
#include <fstream>
#include <vector>



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SDNHandoverScript");

uint32_t nApplicationType = 0;

const uint32_t APP_TCP_ONOFF = 1;
const uint32_t APP_TCP_BULK  = 2;
const uint32_t APP_UDP       = 3;


uint32_t lastRxPacketsum = 0;
double lastThrou   = 0.0;

bool tracing  = true;
bool enableRtsCts = false;

double serverStartTime = 1.0;
double startTime = 1.001;
double stopTime = 40.0;  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nHost       = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 20;    // 使AP2过载
uint32_t nAp3Station = 1;


double nSamplingPeriod = 0.8;   // 抽样间隔，根据总的Simulation时间做相应的调整


/* for udp-server-client application. */
uint32_t nUdpMaxPackets = 20000;    // The maximum packets to be sent.随着Node数增加之前的20000个packets不够了
double nUdpInterval  = 0.2;  // The interval between two packet sent.
uint32_t nUdpPacketSize = 1024;

/* for tcp-bulk-send application. */   
//uint32_t nMaxBytes = 1000000000;  //Zero is unlimited. 100M
uint32_t nMaxBytes = 0;

// 1500字节以下的帧不需要RTS/CTS
uint32_t rtslimit = 1500;

uint32_t MaxRange = 100;

/* 恒定速度移动节点的
初始位置 ( x, y, z)
和
移动速度 ( x, y, z)
*/
Vector3D mPosition = Vector3D(160.0, 120.0, 0.0);
Vector3D mVelocity = Vector3D(0.0, -3.0 , 0.0);  //将速度改小并没有用，该不能握手还是不能握手


// 设置各个AP的传输信号强度(dBm为单位)，必须得为正值，否则不能发送。而且越大表示信号越强。
double ap1TxPwr = 90;
double ap2TxPwr = 100;
double ap3TxPwr = 90;



Ipv4Address serverIp;   // UDP/TCP的server IP
Ipv4Address clientIp;   // UDP/TCP的client IP

std::string filePrefix = "SDN-handover__";
// Throughput
std::string throu = filePrefix + "ThroughputVSTime";
std::string graphicsFileName        = throu + ".eps";
std::string plotFileName;
//  Delay
std::string delay = filePrefix + "DelayVSTime";
std::string graphicsFileName1        = delay + ".eps";
std::string plotFileName1;
// Lost packet
std::string lost = filePrefix + "LostPacketsVSTime";
std::string graphicsFileName2        = lost + ".eps";
std::string plotFileName2;

Gnuplot gnuplot (graphicsFileName);
Gnuplot2dDataset dataset;
Gnuplot gnuplot1 (graphicsFileName1);
Gnuplot2dDataset dataset1;
Gnuplot gnuplot2 (graphicsFileName2);
Gnuplot2dDataset dataset2;

////////////////////// 函数声明 ///////////////////
bool CommandSetup(int argc, char **argv);
// 设置文件输出后缀名
void
SetOutput (uint32_t application_type);


void Assoc (std::string context, Mac48Address maddr);  // Associate时回调
void DeAssoc (std::string context, Mac48Address maddr);  // DeAssociate时回调


void ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset);
void LostPacketsMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset2);
void DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset1);
void JitterMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset3);
void PrintParams (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor);

void ReadArp  (Ptr<Ipv4ClickRouting> clickRouter);
void WriteArp (Ptr<Ipv4ClickRouting> clickRouter);

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
  /* Configure dedicated connections between controller and switches
   * "DEDICATEDCSMA"的意思是，Uses "individual" csma channels, not "shared" csma channel.
   * 这样controller和switch之间的连接才有连线
   */
  Config::SetDefault ("ns3::OFSwitch13Helper::ChannelType",
                      EnumValue (OFSwitch13Helper::DEDICATEDCSMA));
  // Set up some default values for the simulation.
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (1024));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("5Mbps"));
  //设置默认拥塞控制算法
  // ns-3.24   ///////
  //Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpTahoe"));

  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  /* RTS/CTS 一种半双工的握手协议 
  设置好RTS的阈值之后，如果超过这个阈值就会在发送信息之前先发送RTS，以减少干扰，
  相应的CTS会回应之前的RTS。一般都是AP发送CTS数据，而Station发送RTS数据。
  这里设置为1500，表示1500字节以上的frame要进行RTS/CTS机制
  */
  // Turn RTS/CTS on/off for all frames
  if (! enableRtsCts) 
  {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  }
  else
  {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
  }
  /* 
  x^2 = 20^2 + 50^ => 50 < x < 60
  设置最大WIFI覆盖距离为50m(这样一个STA在与某个AP断开连接到与下一个AP连接上的时间之间会有一个间隔时间), 超出这个距离之后将无法传输WIFI信号 
  */
  // Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (MaxRange));
  /* 设置命令行参数 */
  CommandSetup (argc, argv) ;

  // Enabling Checksum computations
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
  
  ns3::PacketMetadata::Enable ();
  
  LogComponentEnable ("SDNHandoverScript", LOG_LEVEL_INFO);

  /*----- init Helpers ----- */
  CsmaHelper  csma;
  
  /* 调用YansWifiChannelHelper::Default() 已经添加了默认的传播损耗模型, 下面不要再手动添加 
  By default, we create a channel model with a propagation delay equal to a constant, 
  the speed of light, and a propagation loss based on a log distance model 
  with a reference loss of 46.6777 dB at reference distance of 1m. 
  */
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  //这里将YansWifiChannel直接弄出来，而不是到时候再create
  Ptr<YansWifiChannel> yansWifiChannel = wifiChannel.Create();
  /* 传播延时速度是恒定的  */
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* 很多地方都用这个，不知道什么意思  */
  // wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");  // !!! 加了这句之后AP和STA就无法连接了
  //wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  /* 不管发送功率是多少，都返回一个恒定的接收功率  */
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (MaxRange));
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
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate18Mbps"));
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);  
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);   // 貌似只能在ns-3.25支持
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  //QosWifiMacHelper wifiMac;
  // 一个给AP，一个给STA
  // NqosWifiMacHelper wifiMacAP = NqosWifiMacHelper::Default ();
  // NqosWifiMacHelper wifiMacSTA = NqosWifiMacHelper::Default ();
  QosWifiMacHelper wifiMacAP = QosWifiMacHelper::Default ();
  QosWifiMacHelper wifiMacSTA = QosWifiMacHelper::Default ();

  NS_LOG_UNCOND ("------------Creating Nodes------------");
  NodeContainer of13SwitchesNode; // 交换机Node(两个)
  NodeContainer apsNode, hostsNode;
  NodeContainer staWifiNodes[nAp]; // 数组，包含nAp个NodeContainer,而每个NodeContainer里面包含各自WIFI网络中的节点
  for (size_t i = 0; i < nAp; i++)
  {
    staWifiNodes[i] = NodeContainer();
  }

  of13SwitchesNode.   Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  apsNode.            Create (nAp);        //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  hostsNode.          Create (nHost);      //2 Nodes(terminal1 and terminal2)-----node 5,6

  staWifiNodes[0].Create(nAp1Station);    // node 7,8,9
  staWifiNodes[1].Create(nAp2Station);    // node 10,11,12,13
  staWifiNodes[2].Create(nAp3Station);    //  node 14
  
  Ptr<Node> ap1WifiNode = apsNode.Get (0);
  Ptr<Node> ap2WifiNode = apsNode.Get (1);
  Ptr<Node> ap3WifiNode = apsNode.Get (2);
  
  // Controller节点
  Ptr<Node> of13ControllerNode = CreateObject<Node> ();   // 传说中Node 15
  

  NS_LOG_UNCOND ("------------Creating Devices------------");
  /* CSMA Devices */
  NetDeviceContainer apCsmaDevices[nAp];
  for (size_t i = 0; i < nAp; i++)
  {
    apCsmaDevices[i] = NetDeviceContainer();
  }

  NetDeviceContainer hostDevices;
  
  NetDeviceContainer of13SwitchPorts[nSwitch];  //两个switch的网卡们(ports)
  for (size_t i = 0; i < nSwitch; i++)
  {
    of13SwitchPorts[i] = NetDeviceContainer();
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

  /* #1 Connect  switch1 to  switch2 */
  // switch1 和 switch2 之间的线路作为主干线路, 应该是100M的
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  link = csma.Install (NodeContainer(of13SwitchesNode.Get(0),of13SwitchesNode.Get(1)));  
  of13SwitchPorts[0]. Add(link.Get(0));
  of13SwitchPorts[1]. Add(link.Get(1));

  
  /* #2 Connect AP1, AP2 and AP3 to switch1 ！！！*/ 
  // 各个AP到switch1的线路做一下限制, 每个AP到switch1只能有30M
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("30Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  for (uint32_t i = 0; i < nAp; i++)
  {
    link = csma.Install (NodeContainer(apsNode.Get(i), of13SwitchesNode.Get(0)));   // Get(0) for switch1
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
    //switch1Device. Add(link.Get(1));
    of13SwitchPorts[0]. Add(link.Get(1));
  }


  /* #3 Connect host1 and host2 to  switch2  */
  // 别忘了这里也要设置csma的link参数，否则会沿用上一个30Mbps的值
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  for (uint32_t i = 0; i < nHost; i++)
  {
    link = csma.Install (NodeContainer(hostsNode.Get(i), of13SwitchesNode.Get(1)));   // Get(1) for switch2
    hostDevices. Add(link.Get(0));
    of13SwitchPorts[1]. Add(link.Get(1)); // switch2
  }

  NS_LOG_UNCOND ("----------Configuring WIFI networks----------");
  Ssid ssid = Ssid ("ssid-default");
  // Ssid ssid1 = Ssid ("ssid-1");
  // Ssid ssid2 = Ssid ("ssid-2");
  // Ssid ssid3 = Ssid ("ssid-3");

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
                   "Ssid", SsidValue (ssid)
                   // ,"BeaconGeneration", BooleanValue (true)  // 应该默认是true吧
                   //,"BeaconInterval", TimeValue (NanoSeconds (102400000)) // 即124ms
                   );
  wifiPhyAP.Set("TxPowerStart", DoubleValue(ap1TxPwr));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(ap1TxPwr));
  apWifiDevices[0]   = wifi.Install(wifiPhyAP, wifiMacAP, ap1WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (1 % 3) * 5));    // 6  
  wifiMacAP.SetType ("ns3::ApWifiMac",
                   // ,"BeaconGeneration", BooleanValue (true)  // 应该默认是true吧
                   //,"BeaconInterval", TimeValue (NanoSeconds (102400000)) // 即124ms
                   "Ssid", SsidValue (ssid)
                   );
  wifiPhyAP.Set("TxPowerStart", DoubleValue(ap2TxPwr));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(ap2TxPwr));

  apWifiDevices[1]   = wifi.Install(wifiPhyAP, wifiMacAP, ap2WifiNode);


  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (2 % 3) * 5));    // 11
  wifiMacAP.SetType ("ns3::ApWifiMac",
                  // ,"BeaconGeneration", BooleanValue (true)  // 应该默认是true吧
                  //,"BeaconInterval", TimeValue (NanoSeconds (102400000)) // 即124ms
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
  mobConstantPosition.Install (of13SwitchesNode);
  mobConstantPosition.Install (of13ControllerNode);

  
  NS_LOG_UNCOND ("----------Installing Bridge NetDevice----------");

  Ptr<Node> of13Switch1Node = of13SwitchesNode.Get (0);
  Ptr<Node> of13Switch2Node = of13SwitchesNode.Get (1);
  /*!!!!!!!!!!!! 关键的 BridgeHelper !!!!!!!!!!!*/

  // 每个AP中，CSMA网卡与WIFI网卡构成一个`BridgeNetDevice`
  BridgeHelper bridgeForAP1, bridgeForAP2, bridgeForAP3;
  bridgeForAP1. Install(apsNode.Get (0), NetDeviceContainer(apWifiDevices[0], apCsmaDevices[0]));
  bridgeForAP2. Install(apsNode.Get (1), NetDeviceContainer(apWifiDevices[1], apCsmaDevices[1]));
  bridgeForAP3. Install(apsNode.Get (2), NetDeviceContainer(apWifiDevices[2], apCsmaDevices[2]));

  NS_LOG_UNCOND ("------Configuring OpenFlow1.3 Switch && Controller------");
  
  /* 这是一个用来创建和配置一个包含单个controller和多个switch的 OpenFlow 1.3 网络的helper */
  Ptr<OFSwitch13Helper> of13Helper = CreateObject<OFSwitch13Helper> ();
  // 前面已经配置了全局的，所以这里不需要了
  // of13Helper->SetChannelType (OFSwitch13Helper::DEDICATEDCSMA);

  /*
  // LearningController   //// 这样在STA从AP1切换到AP2的时候controller会报错 "Inconsistent L2 switching table"
  Ptr<OFSwitch13Controller>         of13ControllerApp;
  Ptr<OFSwitch13LearningController> learningCtrl;
  // InstallDefaultController() 其实是安装的 ns3::OFSwitch13LearningController
  of13ControllerApp = of13Helper->InstallDefaultController (of13ControllerNode); // 接收一个参数
  learningCtrl = DynamicCast<OFSwitch13LearningController> (of13ControllerApp); //转型
  */

  

  Ptr<MyLearningController>  of13ControllerApp = CreateObject<MyLearningController> (); // 自己的controller
  // InstallControllerApp() 是安装的普通的controller
  /*
  将`ns3::OFSwitch13Controller`应用(参数2)安装到node中(参数1)
  同时也在那个node上安装了TCP/IP协议栈
  最后对所有之前已注册的switch 开始  switch <–> controller 的连接
  */
  of13Helper->InstallControllerApp (of13ControllerNode, of13ControllerApp);
  


  OFSwitch13DeviceContainer of13SwitchDevices[nSwitch];
  // 第i个of13SwitchDevices是通过在第i个of13SwitchesNode上安装第i个of13SwitchPorts得到的
  for (size_t i = 0; i < nSwitch; i++)
  {
    of13SwitchDevices[i] = 
        of13Helper->InstallSwitch (of13SwitchesNode.Get(i), of13SwitchPorts[i]);
  }


  /*
  //
  // Install Click on the nodes
  //
  ClickInternetStackHelper clickinternet;
  // APs are to run in promiscuous mode. This can be verified
  // from the pcap trace xxx.pcap generated after running
  // this script.
  clickinternet.SetClickFile (apsNode.Get (0), "src/click/examples/nsclick-wifi-single-interface-promisc.click");
  clickinternet.SetClickFile (apsNode.Get (1), "src/click/examples/nsclick-wifi-single-interface-promisc.click");
  clickinternet.SetClickFile (apsNode.Get (2), "src/click/examples/nsclick-wifi-single-interface-promisc.click");

  clickinternet.SetRoutingTableElement (apsNode, "rt");

  clickinternet.Install (apsNode);
  */

  NS_LOG_UNCOND ("----------Installing Internet stack----------");

  InternetStackHelper internet;

  //internet.Install (apsNode);
  internet.Install (hostsNode);
  //internet.Install (of13SwitchesNode); 
  // (v3.25)给交换机装上Internet之后，交换机就有了回环网卡
  // (v3.24)不能给交换机装Internet，会报错
  
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

  // 共三组STA
  stasWifi1Interface = ip.Assign (stasWifiDevices[0]); // 10.0.0.0.3~5
  stasWifi2Interface = ip.Assign (stasWifiDevices[1]); // 10.0.0.0.6~9
  stasWifi3Interface = ip.Assign (stasWifiDevices[2]); // 10.0.0.0.10

  // 设置全局变量server IP 和client IP的值，供下面的测延时、吞吐量、抖动、丢包等使用
  serverIp = h1h2Interface.GetAddress(1);
  clientIp = stasWifi3Interface.GetAddress(0);

  NS_LOG_UNCOND ("-----------Creating Applications.-----------");
  uint16_t port = 8080;
  
  
/*  判断应用类型，TCP bulk 还是 TCP onoff , 还是UDP  */
  if (nApplicationType == 0)
  {
    std::cout << "[!] Please choose application to run! '--ApplicationType='  tcp-onoff(1), tcp-bulk(2) or udp(3)" << std::endl;
    return 1;
  }

  else if (nApplicationType == APP_UDP )
  {
      
      // UDP server
      UdpServerHelper server (port);  // for the server side, only one param(port) is specified
      // for node 6
      ApplicationContainer serverApps = server.Install (hostsNode.Get(1));
      serverApps.Start (Seconds(serverStartTime));  
      serverApps.Stop (Seconds(stopTime));  
      
    
      // UDP client

      // 给3 个AP1 的stations 加上 UdpClient
      for (uint32_t i =0; i < nAp1Station; i++)
      {
        UdpClientHelper clientHelper (h1h2Interface.GetAddress(1) ,port);
        clientHelper.SetAttribute ("MaxPackets", UintegerValue (nUdpMaxPackets));
        clientHelper.SetAttribute ("Interval", TimeValue (Seconds(nUdpInterval)));  
        clientHelper.SetAttribute ("PacketSize", UintegerValue (nUdpPacketSize));

        ApplicationContainer clientApps;
        clientApps. Add( clientHelper.Install(staWifiNodes[0].Get(i)) ) ;
        clientApps.Start (Seconds(startTime + 0.01));
        clientApps.Stop (Seconds(stopTime));      
      }
      // 给20 个AP2 的stations 加上 UdpClient
      for (uint32_t i =0; i < nAp2Station; i++)
      {
        UdpClientHelper clientHelper (h1h2Interface.GetAddress(1) ,port);
        clientHelper.SetAttribute ("MaxPackets", UintegerValue (nUdpMaxPackets));
        clientHelper.SetAttribute ("Interval", TimeValue (Seconds(nUdpInterval)));  
        clientHelper.SetAttribute ("PacketSize", UintegerValue (nUdpPacketSize));
        
        ApplicationContainer clientApps;
        clientApps. Add( clientHelper.Install(staWifiNodes[1].Get(i)) ) ;
        clientApps.Start (Seconds(startTime + 0.1));
        clientApps.Stop (Seconds(stopTime));
      }
      
      // 给The moving station加上 UdpClient
      ApplicationContainer clientApps;

      UdpClientHelper clientHelper (h1h2Interface.GetAddress(1) ,port);
      clientHelper.SetAttribute ("MaxPackets", UintegerValue (nUdpMaxPackets));
      clientHelper.SetAttribute ("Interval", TimeValue (Seconds(nUdpInterval)));  
      clientHelper.SetAttribute ("PacketSize", UintegerValue (nUdpPacketSize));
      
      clientApps. Add( clientHelper.Install(staWifiNodes[2].Get(0)) ) ;
      clientApps.Start (Seconds(startTime + 0.3));
      clientApps.Stop (Seconds(stopTime));
      

      /*
      // 吞吐量太低
      // UDP server
      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
      sinkApps.Start (Seconds (1.0));
      sinkApps.Stop (Seconds(stopTime));

      // UDP client OnOffApplication
      ApplicationContainer clientApps;
      // 给3 个AP1 的stations 加上 OnOffApplication
      for (uint32_t i = 0; i < nAp1Station; i++)
      {
          OnOffHelper ap1OnOffHelper = OnOffHelper ("ns3::UdpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
          ap1OnOffHelper.SetConstantRate (DataRate ("50kb/s"));
          ap1OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          ap1OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          ap1OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
          ap1OnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
          
          clientApps.Add( ap1OnOffHelper.Install (staWifiNodes[0].Get(i)) );
      }
    
      // 给20 个AP2 的stations 加上 OnOffApplication
      for (uint32_t i = 0; i < nAp2Station; i++)
      {
          OnOffHelper ap2OnOffHelper = OnOffHelper ("ns3::UdpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
          ap2OnOffHelper.SetConstantRate (DataRate ("50kb/s"));
          ap2OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          ap2OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          ap2OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
          ap2OnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
          
          clientApps.Add( ap2OnOffHelper.Install (staWifiNodes[1].Get(i)) );
      }
    
      // 给移动的STA加上 OnOffApplication
      OnOffHelper staOnOffHelper = OnOffHelper ("ns3::UdpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
      staOnOffHelper.SetConstantRate (DataRate ("50kb/s"));
      staOnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      staOnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      staOnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
      staOnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
    
      clientApps.Add( staOnOffHelper.Install (staWifiNodes[2].Get(0)) );
      */
  }

  else if (nApplicationType == APP_TCP_ONOFF )
  {
      
      // TCP server
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
      sinkApps.Start (Seconds (1.0));
      sinkApps.Stop (Seconds(stopTime));

      // TCP client OnOffApplication
      ApplicationContainer clientApps;
      // 给3 个AP1 的stations 加上 OnOffApplication
      for (uint32_t i = 0; i < nAp1Station; i++)
      {
          OnOffHelper ap1OnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
          ap1OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          ap1OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          ap1OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
          ap1OnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
          
          clientApps.Add( ap1OnOffHelper.Install (staWifiNodes[0].Get(i)) );
      }
    
      // 给20 个AP2 的stations 加上 OnOffApplication
      for (uint32_t i = 0; i < nAp2Station; i++)
      {
          OnOffHelper ap2OnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
          ap2OnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
          ap2OnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
          ap2OnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
          ap2OnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
          
          clientApps.Add( ap2OnOffHelper.Install (staWifiNodes[1].Get(i)) );
      }
    
      // 给移动的STA加上 OnOffApplication
      OnOffHelper staOnOffHelper = OnOffHelper ("ns3::TcpSocketFactory",
                                  InetSocketAddress (h1h2Interface.GetAddress(1), port));
      staOnOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      staOnOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      staOnOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTime)));
      staOnOffHelper.SetAttribute ("StopTime", TimeValue (Seconds(stopTime)));
    
      clientApps.Add( staOnOffHelper.Install (staWifiNodes[2].Get(0)) );
  }

  else if (nApplicationType == APP_TCP_BULK )
  {
      // TCP server
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
      sinkApps.Start (Seconds (serverStartTime));
      sinkApps.Stop (Seconds(stopTime));
    
      
      // TCP client BulkSenderApplication
    
      // 给3 个AP1 的stations 加上 BulkSender
      for (uint32_t i = 0; i < nAp1Station; i++)
      {
        BulkSendHelper ap1Source ("ns3::TcpSocketFactory",
                             InetSocketAddress (h1h2Interface.GetAddress(1), port)); // 服务器的IP
        // Set the amount of data to send in bytes.  Zero is unlimited.
        ap1Source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
        ApplicationContainer ap1sourceApps = ap1Source.Install (staWifiNodes[0].Get(i));  // AP1内的第 i 个STA
        ap1sourceApps.Start (Seconds (startTime));
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
        ap2sourceApps.Start (Seconds (startTime));
        ap2sourceApps.Stop (Seconds (stopTime));
      }
      
      // the moving station 
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (h1h2Interface.GetAddress(1), port));
      // Set the amount of data to send in bytes.  Zero is unlimited.
      source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
      ApplicationContainer sourceApps = source.Install (staWifiNodes[2].Get(0));    // AP3内的第 0 个STA
      sourceApps.Start (Seconds (startTime));
      sourceApps.Stop (Seconds (stopTime));
  }



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
      //csma.EnableAsciiAll (ascii.CreateFileStream ("SDN-handover/SDN-handover.tr"));

      // Enable pcap traces
      wifiPhyAP.EnablePcap ("SDN-handover/SDN-handover-ap1-wifi", apWifiDevices[0]);
      wifiPhyAP.EnablePcap ("SDN-handover/SDN-handover-ap2-wifi", apWifiDevices[1]);
      //wifiPhy.EnablePcap ("SDN-handover/SDN-handover-ap2-sta1-wifi", stasWifiDevices[1]);
      wifiPhyAP.EnablePcap ("SDN-handover/SDN-handover-ap3-wifi", apWifiDevices[2]);
      wifiPhyAP.EnablePcap ("SDN-handover/SDN-handover-sta-wifi", stasWifiDevices[2].Get(0));//只有一个
      // WifiMacHelper doesnot have `EnablePcap()` method
      //csma.EnablePcap ("SDN-handover/SDN-handover-switch1-csma", switch1Device);
      //csma.EnablePcap ("SDN-handover/SDN-handover-switch2-csma", switch2Device);
      csma.EnablePcap ("SDN-handover/SDN-handover-switch1-csma", of13SwitchPorts[0]);
      csma.EnablePcap ("SDN-handover/SDN-handover-switch2-csma", of13SwitchPorts[1]);
      //csma.EnablePcap ("SDN-handover/SDN-handover-ap1-csma", apCsmaDevices[0]);
      //csma.EnablePcap ("SDN-handover/SDN-handover-ap2-csma", apCsmaDevices[1]);
      csma.EnablePcap ("SDN-handover/SDN-handover-ap3-csma", apCsmaDevices[2]);
      //csma.EnablePcap ("SDN-handover/SDN-handover-H1-csma", hostDevices.Get(0));
      csma.EnablePcap ("SDN-handover/SDN-handover-H2-csma", hostDevices.Get(1));

      /* 在controller和switch的 OpenFlow信道中开启pcap追踪  */
      of13Helper->EnableOpenFlowPcap ("SDN-handover/SDN-handover-ofCtrl");

      // Enable datapath logs(这个打印在屏幕上的内容太多了，滚都滚不过来)
      //of13Helper->EnableDatapathLogs ("all");
    }



  AnimationInterface anim ("SDN-handover/SDN-handover.xml");
  anim.SetConstantPosition(of13SwitchesNode.Get (0),200,0);             // s1-----node 0
  anim.SetConstantPosition(of13SwitchesNode.Get (1),400,0);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),100,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),200,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),180,100);      // Ap3----node 4
  anim.SetConstantPosition(hostsNode.Get(0),350,60);    // H1-----node 5
  anim.SetConstantPosition(hostsNode.Get(1),400,60);    // H2-----node 6
  //anim.SetConstantPosition(staWifiNodes[2].Get(0),55,40);  //   -----node 14
  anim.SetConstantPosition(of13ControllerNode,300,-20);       //   -----node 15

  //anim.EnablePacketMetadata();   // to see the details of each packet


  /* 对所有STA监控其 Association 和 DeAssocation 的过程  */
  NS_LOG_UNCOND ("--------Tracing Association and DeAssociation.--------");
  std::ostringstream oss_assoc, oss_deassoc;
  // 只需要监控Node 30(即moving STA的Association和DeAssociation状态)
  oss_assoc <<
      "/NodeList/30" <<
      "/DeviceList/0" <<
      "/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Assoc";
  Config::Connect (oss_assoc.str (), MakeCallback (&Assoc));
  oss_deassoc <<
      "/NodeList/30" <<
      "/DeviceList/0" <<
      "/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/DeAssoc";
  Config::Connect (oss_deassoc.str (), MakeCallback (&DeAssoc));

  NS_LOG_UNCOND ("------------Preparing for Checking all the params.------------");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  Simulator::Stop (Seconds(stopTime));
/*----------------------------------------------------------------------*/


/*-----------------------------------------------------*/

  // 根据应用类型设置输出文件后缀名
  SetOutput(nApplicationType);

  // 测吞吐量, 延时, 丢包, 抖动
  ThroughputMonitor (&flowmon, monitor, dataset);
  DelayMonitor      (&flowmon, monitor, dataset1);
  LostPacketsMonitor(&flowmon, monitor, dataset2);
  // JitterMonitor     (&flowmon, monitor, dataset3);
  // 打印出各种参数
  PrintParams       (&flowmon, monitor);

// 手动指定AP的IP
  //Simulator::Schedule (Seconds (0.3), &ReadArp, n.Get (2)->GetObject<Ipv4ClickRouting> ());
  //Simulator::Schedule (Seconds (0.4), &WriteArp, n.Get (2)->GetObject<Ipv4ClickRouting> ());
  //Simulator::Schedule (Seconds (0.5), &ReadArp, n.Get (2)->GetObject<Ipv4ClickRouting> ());

  NS_LOG_UNCOND ("------------Running Simulation.------------");
  Simulator::Run ();


  NS_LOG_UNCOND ("------------Simulation Done.------------");
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
  /*
  gnuplot3.AddDataset (dataset3);
  std::ofstream plotFile3 (plotFileName3.c_str());
  gnuplot3.GenerateOutput (plotFile3);
  plotFile3.close ();
  */


  //monitor->SerializeToXmlFile("SDN-handover/SDN-handover.flowmon", true, true);
  
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

  cmd.AddValue ("ApplicationType", "Choose application to run, tcp-onoff(1), tcp-bulk(2), or udp(3)", nApplicationType);
  
  /* for udp-server-client application */
  cmd.AddValue ("UdpMaxPackets", "The total packets available to be scheduled by the UDP application.", nUdpMaxPackets);
  cmd.AddValue ("Interval", "The interval between two packet sent", nUdpInterval);
  cmd.AddValue ("PacketSize", "The size in byte of each packet", nUdpPacketSize);


  cmd.AddValue ("rtslimit", "The size of packets under which there should be RST/CST", rtslimit);
  cmd.AddValue ("MaxRange", "The max range within which the STA could receive signal", MaxRange);
  
  cmd.Parse (argc, argv);
  return true;
}



void
SetOutput (uint32_t application_type)
{
  
  // 设置文件名后缀
  switch (application_type)
  {
    case APP_TCP_ONOFF:
    {
      plotFileName           =  throu + "_tcp.dat";
      plotFileName1          = delay +  "_tcp.dat";
      plotFileName2          = lost    +  "_tcp.dat";
      break;
    }
    case APP_TCP_BULK:
    {
      plotFileName           =  throu + "_tcp.dat";
      plotFileName1          = delay +  "_tcp.dat";
      plotFileName2          = lost    +  "_tcp.dat";
      break;
    }
    case APP_UDP:
    {
      plotFileName            =  throu + "_udp.dat";
      plotFileName1          = delay +  "_udp.dat";
      plotFileName2          = lost    +  "_udp.dat";
      break;
    }

  }

}



void Assoc (std::string context, Mac48Address maddr)
{
  NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds () <<
    "s " << context <<
    " Associated with access point " << maddr);
}


void DeAssoc (std::string context, Mac48Address maddr)
{
  NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds () <<
    "s " << context <<
    " Association with access point " << maddr << " lost");
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
  
  uint32_t tmpRxPacketsum = 0;
  double tmpThrou ;

  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  /* since fmhelper is a pointer, we should use it as a pointer.
   * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
   */
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {
      
      tmpRxPacketsum = i->second.rxPackets;
      tmpThrou   = i->second.rxBytes * 8.0 / 
        (i->second.timeLastRxPacket.GetSeconds() - 
          i->second.timeFirstTxPacket.GetSeconds())/1024 ;

    /* 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的 */
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            // 第一次lastRxPacketsum 的值为0
            if (lastRxPacketsum == 0)
            {
              dataset.Add  (Simulator::Now().GetSeconds(), tmpThrou);
              lastRxPacketsum = tmpRxPacketsum;
            }
            else //之后只要 tmpRxPacketsum 不等于 lastRxPacketsum, 即接收到的包数量有变化，说明说收到了新的包，将这时的吞吐量加入dataSet中
              if ( tmpRxPacketsum - lastRxPacketsum)
              {
                dataset.Add  (Simulator::Now().GetSeconds(), tmpThrou);
                lastRxPacketsum = tmpRxPacketsum ;
              }
              // 否则吞吐量为0 
              else
              {
                dataset.Add  (Simulator::Now().GetSeconds(), 0.0);
              } 
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
      
      RxPacketsum  = i->second.rxPackets;
      Delaysum     = i->second.delaySum.GetSeconds();
      if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
        {
          // UDP_PROT_NUMBER = 17
          // TCP_PORT_NUMBER = 6
            if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
            {
              delay = Delaysum / RxPacketsum * 1000;   // 延时单位为ms
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
  
  // uint32_t TxPacketsum = 0;
  uint32_t RxPacketsum = 0;
  uint32_t LostPacketsum = 0;
  double LostPacketRatio = 0.0;

  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    
    // TxPacketsum   = i->second.txPackets;
    RxPacketsum   = i->second.rxPackets;
    LostPacketsum = i->second.lostPackets;

    if (LostPacketsum + RxPacketsum != 0)
    {
      LostPacketRatio = (double)LostPacketsum / (LostPacketsum + RxPacketsum) * 100;
    }

    if (t.sourceAddress==clientIp && t.destinationAddress == serverIp)
      {
        // UDP_PROT_NUMBER = 17
        // TCP_PORT_NUMBER = 6
          if (17 == unsigned(t.protocol) || 6 == unsigned(t.protocol))
          {
            dataset2.Add (Simulator::Now().GetSeconds(), LostPacketRatio);
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

    RxPacketsum   = i->second.rxPackets;
    JitterSum     = i->second.jitterSum.GetSeconds();

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
  // uint32_t TxBytesum   = 0;
  // uint32_t RxBytesum   = 0;
  uint32_t TxPacketsum = 0;
  uint32_t RxPacketsum = 0;
  uint32_t DropPacketsum = 0;
  uint32_t LostPacketsum = 0;
  double LostPacketRatio = 0.0;

  double Delaysum  = 0;
  double JitterSum = 0;

  monitor->CheckForLostPackets(); 
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); i++){ 
      // A tuple: Source-ip, destination-ip, protocol, source-port, destination-port
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    

    TxPacketsum   = i->second.txPackets;
    RxPacketsum   = i->second.rxPackets;
    LostPacketsum = i->second.lostPackets;

    if (LostPacketsum + RxPacketsum)
    {
      LostPacketRatio = (double)LostPacketsum / (LostPacketsum + RxPacketsum) * 100;
    }
    DropPacketsum = i->second.packetsDropped.size();
    Delaysum      = i->second.delaySum.GetSeconds();
    JitterSum     = i->second.jitterSum.GetSeconds();
    
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
            std::cout<< "Delay: " << Delaysum/ RxPacketsum * 1000 << " ms" << std::endl;   // 延时单位为ms
            std::cout<< "Packets loss radio: " << LostPacketRatio  << "%" << std::endl;
            std::cout<< "Jitter: " << JitterSum/ (RxPacketsum - 1) << std::endl;
            std::cout << "Dropped Packets: " << DropPacketsum << std::endl;
            // std::cout << "Packets Delivery Ratio: " << ( RxPacketsum * 100 / TxPacketsum) << "%" << std::endl;
            std::cout<<"------------------------------------------" << std::endl;



          }
          else
          {
            std::cout << "This is not UDP/TCP traffic" << std::endl;
          }
      }
    }
  // 每隔一秒打印一次
  Simulator::Schedule(Seconds(nSamplingPeriod), &PrintParams, fmhelper, monitor);

}

void
ReadArp (Ptr<Ipv4ClickRouting> clickRouter)
{
  // Access the handlers
  NS_LOG_INFO (clickRouter->ReadHandler ("wifi/arpquerier", "table"));
  NS_LOG_INFO (clickRouter->ReadHandler ("wifi/arpquerier", "stats"));
}

void
WriteArp (Ptr<Ipv4ClickRouting> clickRouter)
{
  // Access the handler
  NS_LOG_INFO (clickRouter->WriteHandler ("wifi/arpquerier", "insert", "172.16.1.2 00:00:00:00:00:02"));
}
