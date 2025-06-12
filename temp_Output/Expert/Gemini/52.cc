#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  double simulationTime = 10.0;
  std::string phyMode = "OfdmRate6Mbps";
  double distance = 10.0;
  double slotTime = 9e-6;
  double sifs = 16e-6;
  double pifs = 25e-6;

  CommandLine cmd;
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("slotTime", "Slot time in seconds", slotTime);
  cmd.AddValue("sifs", "SIFS in seconds", sifs);
  cmd.AddValue("pifs", "PIFS in seconds", pifs);
  cmd.Parse(argc, argv);

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());
  phyHelper.Set("ShortGuardEnabled", BooleanValue(true));

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("ns-3-ssid");

  NetDeviceContainer apDevice;
  macHelper.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true),
                    "BeaconInterval", TimeValue(Seconds(0.1)));

  apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

  NetDeviceContainer staDevice;
  macHelper.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

  staDevice = wifiHelper.Install(phyHelper, macHelper, staNode);

  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue(phyMode),
                                      "ControlMode", StringValue(phyMode));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(apDevice);
  ipv4.Assign(staDevice);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(staNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(i.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
  echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(apNode.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  Config::Set("/NodeList/*/$ns3::WifiNetDevice/*/$ns3::WifiMac/Slot", TimeValue(Seconds(slotTime)));
  Config::Set("/NodeList/*/$ns3::WifiNetDevice/*/$ns3::WifiMac/Sifs", TimeValue(Seconds(sifs)));
  Config::Set("/NodeList/*/$ns3::WifiNetDevice/*/$ns3::WifiMac/Pifs", TimeValue(Seconds(pifs)));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.destinationAddress == i.GetAddress(1)) {
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (simulationTime * 1000000.0) << " Mbps\n";
    }
  }

  Simulator::Destroy();
  return 0;
}