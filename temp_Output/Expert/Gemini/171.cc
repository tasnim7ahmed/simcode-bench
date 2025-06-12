#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(10);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 0, 500, 500)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"));
  mobility.Install(nodes);

  uint16_t port = 9;
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    for (uint32_t j = 0; j < nodes.GetN(); ++j) {
      if (i != j) {
        Time startTime = Seconds(1.0 + (double)rand() / RAND_MAX * 5.0);
        UdpClientHelper client(interfaces.GetAddress(j), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
        client.SetAttribute("PacketSize", UintegerValue(1472));
        ApplicationContainer clientApps = client.Install(nodes.Get(i));
        clientApps.Start(startTime);
        clientApps.Stop(Seconds(30.0));

        UdpServerHelper server(port);
        ApplicationContainer serverApps = server.Install(nodes.Get(j));
        serverApps.Start(Seconds(0.0));
        serverApps.Stop(Seconds(30.0));
      }
    }
  }

  Simulator::Stop(Seconds(30.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalPDR = 0.0;
  double totalDelay = 0.0;
  int numFlows = 0;

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    if (t.sourceAddress != Ipv4Address("10.1.1.1")) {
      totalPDR += (double)i->second.rxPackets / (double)i->second.txPackets;
      totalDelay += i->second.delaySum.GetSeconds() / (double)i->second.rxPackets;
      numFlows++;
    }
  }

  std::cout << "Average PDR: " << totalPDR / numFlows << std::endl;
  std::cout << "Average End-to-End Delay: " << totalDelay / numFlows << " s" << std::endl;

  Simulator::Destroy();

  return 0;
}