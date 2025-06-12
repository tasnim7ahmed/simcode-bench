#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[]) {

  bool enablePcap = false;
  bool enableNetAnim = false;

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("enableNetAnim", "Enable NetAnim output", enableNetAnim);
  cmd.Parse(argc, argv);

  // Set up basic simulation parameters
  int numNodes = 5;
  double simulationTime = 10.0;
  double nodeSpeed = 10.0; // m/s
  double pauseTime = 1.0;

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Configure wireless devices (using Wifi)
  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  NetDeviceContainer devices;
  Ssid ssid = Ssid("ns-3-adhoc");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
  devices = wifi.Install(phy, mac, nodes);

  // Install AODV routing protocol
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv); // has effect on the next Install
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create and install a mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("100x100"));
  mobility.Install(nodes);

  // Create Applications (e.g., UDP echo client/server)
  uint16_t port = 9; // Discard port (RFC 863)

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1)); // Last node as server
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime - 1.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // First node as client
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime - 2.0));

  // Enable PCAP tracing if requested
  if (enablePcap) {
    phy.EnablePcapAll("aodv_manet");
  }

  // Enable NetAnim if requested
  if (enableNetAnim) {
    AnimationInterface anim("aodv_manet.xml");
  }

  //FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Run the simulation
  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  //FlowMonitor Output
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	  std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
	  std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
	  std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }


  Simulator::Destroy();
  return 0;
}