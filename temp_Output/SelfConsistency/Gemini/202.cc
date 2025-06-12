#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/spectrum-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CircularWSN");

int main(int argc, char *argv[]) {
  // Enable logging for specific categories
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t numNodes = 6;
  double simulationTime = 10.0; // seconds
  double dataRate = 1.0;        // Mbps
  double interval = 1.0;        // seconds
  uint16_t sinkPort = 9;

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Configure the 802.15.4 PHY and MAC layers
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(lrWpanDevices);

  // Configure mobility - circular topology
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::CirclePositionAllocator",
                                "Radius", DoubleValue(5.0),
                                "Theta", DoubleValue(0.0));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // Create sink application (UDP server) on node 0
  UdpServerHelper server(sinkPort);
  ApplicationContainer sinkApp = server.Install(nodes.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simulationTime + 1));

  // Create source applications (UDP clients) on other nodes
  UdpClientHelper client(interfaces.GetAddress(0), sinkPort);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(1024)); // bytes

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numNodes; ++i) {
    clientApps.Add(client.Install(nodes.Get(i)));
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime));

  // Create a flow monitor to track packet flows
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Enable PCAP tracing
  lrWpanHelper.EnablePcapAll("circular-wsn");

  // Enable NetAnim visualization
  AnimationInterface anim("circular-wsn.xml");
  anim.SetMaxPktsPerTraceFile(10000000); // adjust to avoid packet loss in animation
  anim.EnablePacketMetadata(true); // optional - displays packet metadata

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  // Analyze the results using the flow monitor
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Packet Delivery Ratio: " << ((double)i->second.rxPackets / (double)i->second.txPackets) << "\n";
    std::cout << "  End-to-End Delay: " << i->second.delaySum << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}