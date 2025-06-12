#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {

  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);

  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodes;
  staNodes.Create(4);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid-1");
  Ssid ssid2 = Ssid("ns-3-ssid-2");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));
  NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices3 = wifi.Install(phy, mac, staNodes.Get(2));
  NetDeviceContainer staDevices4 = wifi.Install(phy, mac, staNodes.Get(3));


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

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(1.0),
                                "MinY", DoubleValue(1.0),
                                "DeltaX", DoubleValue(3.0),
                                "DeltaY", DoubleValue(3.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(staNodes);



  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
  address.NewNetwork();

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices3);
  address.NewNetwork();

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces3 = address.Assign(staDevices2);
  address.NewNetwork();

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces4 = address.Assign(staDevices4);


  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(apNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(apInterfaces1.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(staNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));


  UdpServerHelper server2(9);
  ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpClientHelper client2(apInterfaces2.GetAddress(0), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(1000));
  client2.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = client2.Install(staNodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));


  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("wifi-animation.xml");
  anim.SetConstantPosition(apNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(apNodes.Get(1), 0.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(staNodes.Get(1), 4.0, 1.0);
  anim.SetConstantPosition(staNodes.Get(2), 1.0, 4.0);
  anim.SetConstantPosition(staNodes.Get(3), 4.0, 4.0);


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


  Simulator::Run();


  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}