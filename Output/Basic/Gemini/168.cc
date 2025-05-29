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

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNodes;
  staNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);
  mobility.Install(staNodes);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevices);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(staNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(staInterfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(staNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  phy.EnablePcap("wifi-ap", apDevices);
  phy.EnablePcap("wifi-sta", staDevices);

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.tr"));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  AnimationInterface anim("wifi-animation.xml");
  anim.SetConstantPosition(apNode.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(staNodes.Get(0), 1.0, 1.0);
  anim.SetConstantPosition(staNodes.Get(1), 1.0, 20.0);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.destinationAddress == staInterfaces.GetAddress(0)) {
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
      }
  }

  Simulator::Destroy();

  return 0;
}