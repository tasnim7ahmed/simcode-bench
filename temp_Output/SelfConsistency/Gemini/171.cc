#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(10);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", StringValue("500x500"),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
  mobility.Install(nodes);

  // Internet stack
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(nodes);

  // UDP traffic
  UdpClientServerHelper udp(9); // Port 9
  udp.SetAttribute("MaxPackets", UintegerValue(1000));
  udp.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  udp.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t i = 0; i < 5; ++i) {
      Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
      startTimeSeconds->SetAttribute("Min", DoubleValue(1.0));
      startTimeSeconds->SetAttribute("Max", DoubleValue(10.0));

      serverApps.Add(udp.Install(nodes.Get(i), i.GetAddress(i)));
      clientApps.Add(udp.Install(nodes.Get(i + 5), i.GetAddress(i)));

      clientApps.Start(Seconds(startTimeSeconds->GetValue()));
      serverApps.Start(Seconds(startTimeSeconds->GetValue()));
  }


  // Pcap tracing
  Packet::EnablePcapAll("adhoc-aodv");

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation Interface
  // Comment out to disable.
  //AnimationInterface anim("adhoc-aodv.xml");

  // Run simulation
  Simulator::Stop(Seconds(30));
  Simulator::Run();

  // Calculate and display metrics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = Seconds(0).GetDouble();
  int numFlows = 0;

  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

    totalPacketsSent += i->second.txPackets;
    totalPacketsReceived += i->second.rxPackets;
    totalDelay += i->second.delaySum.GetSeconds();
    numFlows++;

    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
  }

  double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
  double averageEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

  std::cout << "\nSimulation Results:\n";
  std::cout << "  Packet Delivery Ratio: " << packetDeliveryRatio << "\n";
  std::cout << "  Average End-to-End Delay: " << averageEndToEndDelay << " seconds\n";

  Simulator::Destroy();

  return 0;
}