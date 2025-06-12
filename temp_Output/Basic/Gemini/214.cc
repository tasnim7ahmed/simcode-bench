#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSimulation");

int main(int argc, char *argv[]) {
  bool enablePcap = true;
  bool enableNetAnim = false;

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer wifiNodes;
  wifiNodes.Create(4);

  NodeContainer staNodes = NodeContainer(wifiNodes.Get(1), wifiNodes.Get(2), wifiNodes.Get(3));
  NodeContainer apNode = NodeContainer(wifiNodes.Get(0));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

  NetDeviceContainer devices;
  devices.Add(apDevice);
  devices.Add(staDevices);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  InternetStackHelper internet;
  internet.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  ApplicationContainer sinkApp = sinkHelper.Install(wifiNodes.Get(1));

  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onOffHelper.SetConstantRate(DataRate("5Mb/s"));
  ApplicationContainer app = onOffHelper.Install(wifiNodes.Get(2));

  app.Start(Seconds(2.0));
  app.Stop(Seconds(10.0));

  PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
  ApplicationContainer sinkApp2 = sinkHelper2.Install(wifiNodes.Get(2));

  sinkApp2.Start(Seconds(1.0));
  sinkApp2.Stop(Seconds(10.0));

  OnOffHelper onOffHelper2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
  onOffHelper2.SetConstantRate(DataRate("5Mb/s"));
  ApplicationContainer app2 = onOffHelper2.Install(wifiNodes.Get(3));

  app2.Start(Seconds(2.0));
  app2.Stop(Seconds(10.0));


  if (enablePcap) {
    phy.EnablePcapAll("wifi-simulation");
  }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(10.0));

  if (enableNetAnim) {
    AnimationInterface anim("wifi-simulation.xml");
    anim.SetConstantPosition(apNode.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(staNodes.Get(0), 5.0, 5.0);
    anim.SetConstantPosition(staNodes.Get(1), 15.0, 5.0);
    anim.SetConstantPosition(staNodes.Get(2), 10.0, 15.0);
  }

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  double totalThroughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

    double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    totalThroughput += throughput;
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Throughput: " << throughput << " Mbps\n";
  }

  std::cout << "Total Throughput: " << totalThroughput << " Mbps\n";

  Simulator::Destroy();

  return 0;
}