#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(1)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(2)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(3)));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(4));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer::Create(staDevices, apDevices));

  UdpClientHelper client0(interfaces.GetAddress(4), 9);
  client0.SetAttribute("MaxPackets", UintegerValue(100));
  client0.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client0.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps0 = client0.Install(nodes.Get(0));
  clientApps0.Start(Seconds(1.0));
  clientApps0.Stop(Seconds(10.0));

  UdpClientHelper client1(interfaces.GetAddress(4), 9);
  client1.SetAttribute("MaxPackets", UintegerValue(100));
  client1.SetAttribute("Interval", TimeValue(Seconds(0.15)));
  client1.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps1 = client1.Install(nodes.Get(1));
  clientApps1.Start(Seconds(1.5));
  clientApps1.Stop(Seconds(10.0));

  UdpClientHelper client2(interfaces.GetAddress(4), 9);
  client2.SetAttribute("MaxPackets", UintegerValue(100));
  client2.SetAttribute("Interval", TimeValue(Seconds(0.2)));
  client2.SetAttribute("PacketSize", UintegerValue(256));
  ApplicationContainer clientApps2 = client2.Install(nodes.Get(2));
  clientApps2.Start(Seconds(2.0));
  clientApps2.Stop(Seconds(10.0));

  UdpClientHelper client3(interfaces.GetAddress(4), 9);
  client3.SetAttribute("MaxPackets", UintegerValue(100));
  client3.SetAttribute("Interval", TimeValue(Seconds(0.25)));
  client3.SetAttribute("PacketSize", UintegerValue(128));
  ApplicationContainer clientApps3 = client3.Install(nodes.Get(3));
  clientApps3.Start(Seconds(2.5));
  clientApps3.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(nodes.Get(4));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  std::ofstream csvFile;
  csvFile.open("simulation_data.csv");
  csvFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time\n";

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    csvFile << t.sourceAddress << "," << t.destinationAddress << "," << i->second.txBytes/i->second.txPackets << "," << i->second.timeFirstTxPacket.GetSeconds() << "," << i->second.timeLastRxPacket.GetSeconds() << "\n";
  }

  csvFile.close();

  Simulator::Destroy();

  return 0;
}