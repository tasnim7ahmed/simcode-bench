#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer apNodes;
  apNodes.Create(2);
  NodeContainer staNodes;
  staNodes.Create(6);

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
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice1 = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(0.05)));
  NetDeviceContainer apDevice2 = wifi.Install(phy, mac, apNodes.Get(1));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice1 = wifi.Install(phy, mac, staNodes.Get(0), staNodes.Get(1), staNodes.Get(2));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice2 = wifi.Install(phy, mac, staNodes.Get(3), staNodes.Get(4), staNodes.Get(5));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(20.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface1 = address.Assign(apDevice1);
  Ipv4InterfaceContainer staInterface1 = address.Assign(staDevice1);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface2 = address.Assign(apDevice2);
  Ipv4InterfaceContainer staInterface2 = address.Assign(staDevice2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(apNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client1(apInterface1.GetAddress(0), port);
  client1.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client1.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp1 = client1.Install(staNodes.Get(0), staNodes.Get(1), staNodes.Get(2));
  clientApp1.Start(Seconds(2.0));
  clientApp1.Stop(Seconds(10.0));

   UdpClientHelper client2(apInterface2.GetAddress(0), port);
  client2.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client2.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp2 = client2.Install(staNodes.Get(3), staNodes.Get(4), staNodes.Get(5));
  clientApp2.Start(Seconds(2.0));
  clientApp2.Stop(Seconds(10.0));

  phy.EnablePcapAll("wifi-simple");

  AnimationInterface anim("wifi-simple.xml");
  anim.SetConstantPosition(apNodes.Get(0), 20.0, 0.0);
  anim.SetConstantPosition(apNodes.Get(1), 20.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(staNodes.Get(1), 5.0, 0.0);
  anim.SetConstantPosition(staNodes.Get(2), 10.0, 0.0);
  anim.SetConstantPosition(staNodes.Get(3), 0.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(4), 5.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(5), 10.0, 10.0);

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}