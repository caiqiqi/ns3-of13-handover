```cpp
// 创建Nodes
NodeContainer of13SwitchesNodes; // 交换机Node(两个)
NodeContainer apSwitchNodes      // 集成AP功能的Openflow交换机节点


Ptr<Node> of13ControllerNode = CreateObject<Node> ();   // Node 0 Controller节点
of13SwitchesNodes.   Create (nSwitch);                   // Node 1
apSwitchNodes.       Create (nApSwitch);                 //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4


Ptr<Node> ap1WifiNode = apSwitchNodes.Get (0);
Ptr<Node> ap2WifiNode = apSwitchNodes.Get (1);
Ptr<Node> ap3WifiNode = apSwitchNodes.Get (2);

// 创建Devices

  /* WIFI Devices */
  // 各个WIFI网络的AP数组，每个NetDeviceContainer里包含各种WIFI网络的网卡
  NetDeviceContainer apWifiDevices[nAp];
  for (size_t i = 0 ; i <nAp; i++)
  {
    apWifiDevices[i] = NetDeviceContainer();
  }

  // ------------------- 配置AP --------------------
  wifiMacAP.SetType ("ns3::ApWifiMac", 
                   "Ssid", SsidValue (ssid)
                   // ,"BeaconGeneration", BooleanValue (true)  // 应该默认是true吧
                   //,"BeaconInterval", TimeValue (NanoSeconds (102400000)) // 即124ms
                   );
  wifiPhyAP.Set("TxPowerStart", DoubleValue(apTxPwr[i]));
  wifiPhyAP.Set("TxPowerEnd",   DoubleValue(apTxPwr[i]));
  apWifiDevices[0]   = wifi.Install(wifiPhyAP, wifiMacAP, apSwitchNodes.Get (0));
  apWifiDevices[1]   = wifi.Install(wifiPhyAP, wifiMacAP, apSwitchNodes.Get (1));
  apWifiDevices[2]   = wifi.Install(wifiPhyAP, wifiMacAP, apSwitchNodes.Get (2));

  // 创建各个ofswitch的ports
  NetDeviceContainer of13SwitchPorts[nOf13Switch];  //两个switch的网卡们(ports)
  for (uint32_t i = 0; i < nOf13Switch; i++)
  {
    of13SwitchPorts[i] = NetDeviceContainer();
  }

  // Connect the switches/APs in chain
  for (uint32_t i = 1; i < nOf13Switch; i++)
    {
      NodeContainer nc (of13SwitchNodes.Get (i - 1), of13SwitchNodes.Get (i));
      NetDeviceContainer link = csmaHelper.Install (nc);
      of13SwitchPorts [i - 1].Add (link.Get (0));
      of13SwitchPorts [i].Add (link.Get (1));
    }



  // Install OpenFlow device in every switch
  OFSwitch13DeviceContainer of13SwitchDevices;
  for (uint32_t i = 0; i < nOf13Switch; i++)
    {
      // 在第i个of13SwitchNodes 上装上 of13SwitchPorts(NetDeviceContainer)
      of13SwitchDevices = of13Helper->InstallSwitch (of13SwitchNodes.Get (i), of13SwitchPorts [i]);
    }
```