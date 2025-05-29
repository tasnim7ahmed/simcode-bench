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
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNodes;
  staNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

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
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client1(apInterface.GetAddress(0), 9);
  client1.SetAttribute("MaxPackets", UintegerValue(1000));
  client1.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client1.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

    UdpClientHelper client2(apInterface.GetAddress(0), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(1000));
  client2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client2.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  phy.EnablePcapAll("wifi-simulation");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    std::cout << "  Packets Tx:   " << i->second.txPackets << "\n";
  }

  Simulator::Destroy();

  return 0;
}