#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationExample");

static void
TxopTrace(Time txopDuration, std::string apName)
{
  std::cout << apName << " TXOP Duration: " << txopDuration << std::endl;
}

int
main(int argc, char* argv[])
{
  bool verbose = false;
  double simulationTime = 10;
  double distance = 10;
  bool rtsCts = false;
  Time txopLimit = MicroSeconds(0);

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("simulationTime", "Simulation runtime in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue("txopLimit", "TXOP Limit in microseconds", txopLimit);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("WifiAggregationExample", LOG_LEVEL_INFO);
  }

  NodeContainer apNodes;
  apNodes.Create(4);
  NodeContainer staNodes;
  staNodes.Create(4);

  YansWifiChannelHelper channel[4];
  YansWifiPhyHelper phy[4];
  WifiHelper wifi[4];
  WifiMacHelper mac[4];

  std::string ssid[4] = { "ns-3-ssid-36", "ns-3-ssid-40", "ns-3-ssid-44", "ns-3-ssid-48" };
  int channelNumber[4] = { 36, 40, 44, 48 };

  for (int i = 0; i < 4; ++i)
  {
    channel[i] = YansWifiChannelHelper::Default();
    phy[i] = YansWifiPhyHelper::Default();
    phy[i].SetChannel(channel[i].Create());

    wifi[i] = WifiHelper::Default();
    wifi[i].SetStandard(WIFI_PHY_STANDARD_80211n);

    mac[i] = WifiMacHelper::Default();

    Ssid ss = Ssid(ssid[i]);
    mac[i].SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ss),
                   "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi[i].Install(phy[i], mac[i], staNodes.Get(i));

    mac[i].SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ss),
                   "BeaconGeneration", BooleanValue(true),
                   "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi[i].Install(phy[i], mac[i], apNodes.Get(i));

    // Configure channel number
    phy[i].Set("ChannelNumber", UintegerValue(channelNumber[i]));
  }

  // Configure aggregation
  Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmpduLength", UintegerValue(65535));
  Config::SetDefault("ns3::WifiRemoteStationManager::MaxAmsduLength", UintegerValue(7935));

  // Station A: Default A-MPDU (65kB)
  // Station B: Disable aggregation
  Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/Station/$ns3::RegularWifiMac/AggregationEnable", BooleanValue(false));
  // Station C: Enable A-MSDU (8kB), disable A-MPDU
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/Station/$ns3::RegularWifiMac/AmsduEnable", BooleanValue(true));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/Station/$ns3::RegularWifiMac/AmpduEnable", BooleanValue(false));
  // Station D: Enable A-MPDU (32kB) and A-MSDU (4kB)
  Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/Station/$ns3::RegularWifiMac/MaxAmpduLength", UintegerValue(32767));
  Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/Station/$ns3::RegularWifiMac/MaxAmsduLength", UintegerValue(4095));

  if (rtsCts)
  {
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
  }

  if (txopLimit > MicroSeconds(0))
  {
    Config::SetDefault("ns3::WifiRemoteStationManager::Txop", TimeValue(txopLimit));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  for (int i = 0; i < 4; ++i)
  {
    Ptr<MobilityModel> apMobility = apNodes.Get(i)->GetObject<MobilityModel>();
    Ptr<MobilityModel> staMobility = staNodes.Get(i)->GetObject<MobilityModel>();

    Vector3d apPosition = apMobility->GetPosition();
    apPosition.x = i * 20; // Spread APs horizontally
    apMobility->SetPosition(apPosition);

    Vector3d staPosition = staMobility->GetPosition();
    staPosition.x = apPosition.x + distance;
    staMobility->SetPosition(staPosition);
  }

  InternetStackHelper stack;
  stack.Install(apNodes);
  stack.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces[4];
  Ipv4InterfaceContainer apInterfaces[4];

  for (int i = 0; i < 4; ++i)
  {
    staInterfaces[i] = address.Assign(NetDeviceContainer(staNodes.Get(i)->GetDevice(0)));
    apInterfaces[i] = address.Assign(NetDeviceContainer(apNodes.Get(i)->GetDevice(0)));
    address.NewNetwork();
  }

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps[4];
  for (int i = 0; i < 4; ++i)
  {
    serverApps[i] = echoServer.Install(apNodes.Get(i));
    serverApps[i].Start(Seconds(1.0));
    serverApps[i].Stop(Seconds(simulationTime));
  }

  UdpEchoClientHelper echoClient(apInterfaces[0].GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  for (int i = 0; i < 4; ++i)
  {
    echoClient.SetRemote(apInterfaces[i].GetAddress(0), 9);
    clientApps[i] = echoClient.Install(staNodes.Get(i));
    clientApps[i].Start(Seconds(2.0));
    clientApps[i].Stop(Seconds(simulationTime));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Trace TXOP duration
  for (int i = 0; i < 4; ++i)
  {
    std::string apName = "AccessPoint" + std::to_string(i);
    Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/DeviceList/*/$ns3::WifiNetDevice/Mac/Ap/$ns3::ApWifiMac/Txop/TxopDuration",
                                 MakeBoundCallback(&TxopTrace, apName));
  }

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalThroughput[4] = { 0.0, 0.0, 0.0, 0.0 };
  Time maxTxopDuration[4];
  maxTxopDuration[0] = Seconds(0);
  maxTxopDuration[1] = Seconds(0);
  maxTxopDuration[2] = Seconds(0);
  maxTxopDuration[3] = Seconds(0);

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
    std::cout << "  Tx Bytes:   " << i->second.txBytes << std::endl;
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << std::endl;
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;

    for (int j = 0; j < 4; ++j)
    {
      if (t.sourceAddress == staInterfaces[j].GetAddress(0) && t.destinationAddress == apInterfaces[j].GetAddress(0))
      {
        totalThroughput[j] = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
        break;
      }
    }
  }

  for (int i = 0; i < 4; ++i)
  {
    std::cout << "Network " << i << " Throughput: " << totalThroughput[i] << " Mbps" << std::endl;
  }

  Simulator::Destroy();

  return 0;
}