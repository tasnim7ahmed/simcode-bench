#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-multicast-routing.h"

NS_LOG_COMPONENT_DEFINE ("MulticastBlacklistingSimulation");

int main (int argc, char *argv[])
{
  // 1. Simulation parameters
  uint32_t packetSize = 500; // bytes
  std::string dataRate = "1Mbps";
  uint32_t numPacketsToSend = 10;
  double simulationStopTime = 10.0; // seconds
  uint16_t multicastPort = 4000;
  Ipv4Address multicastGroup = "225.1.2.3";

  // 2. Enable logging for relevant modules
  LogComponentEnable ("MulticastBlacklistingSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv4StaticMulticastRouting", LOG_LEVEL_DEBUG); // For routing details

  // 3. Create five nodes: A, B, C, D, E
  NodeContainer nodes;
  nodes.Create (5);
  Ptr<Node> aNode = nodes.Get (0); // Node A (Source)
  Ptr<Node> bNode = nodes.Get (1); // Node B (Intermediate Router & Sink)
  Ptr<Node> cNode = nodes.Get (2); // Node C (Sink)
  Ptr<Node> dNode = nodes.Get (3); // Node D (Sink)
  Ptr<Node> eNode = nodes.Get (4); // Node E (Sink)

  // 4. Configure PointToPointHelper for link characteristics
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // 5. Install Internet Stack with Static Multicast Routing Helper
  InternetStackHelper internet;
  Ipv4StaticMulticastRoutingHelper staticMulticastRoutingHelper;
  internet.SetRoutingHelper (staticMulticastRoutingHelper);
  internet.Install (nodes);

  // 6. Create links and assign IP addresses
  // Link A-B (10.1.1.0/24)
  NetDeviceContainer abDevices = p2p.Install (aNode, bNode);
  Ipv4AddressHelper abAddresses;
  abAddresses.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer abInterfaces = abAddresses.Assign (abDevices);

  // Link B-C (10.1.2.0/24)
  NetDeviceContainer bcDevices = p2p.Install (bNode, cNode);
  Ipv4AddressHelper bcAddresses;
  bcAddresses.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bcInterfaces = bcAddresses.Assign (bcDevices);

  // Link B-D (10.1.3.0/24)
  NetDeviceContainer bdDevices = p2p.Install (bNode, dNode);
  Ipv4AddressHelper bdAddresses;
  bdAddresses.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer bdInterfaces = bdAddresses.Assign (bdDevices);

  // Link B-E (10.1.4.0/24)
  NetDeviceContainer beDevices = p2p.Install (bNode, eNode);
  Ipv4AddressHelper beAddresses;
  beAddresses.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer beInterfaces = beAddresses.Assign (beDevices);

  // Link A-C (10.1.5.0/24) - This link is physically present but will be "blacklisted" for Node A's multicast output
  NetDeviceContainer acDevices = p2p.Install (aNode, cNode);
  Ipv4AddressHelper acAddresses;
  acAddresses.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer acInterfaces = acAddresses.Assign (acDevices);

  // Get pointers to Ipv4 objects for each node
  Ptr<Ipv4> ipv4A = aNode->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4B = bNode->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4C = cNode->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4D = dNode->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4E = eNode->GetObject<Ipv4>();

  // Get interface indices for static routing setup
  // Node A interfaces:
  uint32_t aIfToB_idx = ipv4A->GetInterfaceForAddress(abInterfaces.GetAddress(0)); // Interface to B
  uint32_t aIfToC_idx = ipv4A->GetInterfaceForAddress(acInterfaces.GetAddress(0)); // Interface to C
  uint32_t aLoopbackIf_idx = 0; // Loopback interface index is typically 0

  // Node B interfaces:
  uint32_t bIfToA_idx = ipv4B->GetInterfaceForAddress(abInterfaces.GetAddress(1)); // Interface to A
  uint32_t bIfToC_idx = ipv4B->GetInterfaceForAddress(bcInterfaces.GetAddress(0)); // Interface to C
  uint32_t bIfToD_idx = ipv4B->GetInterfaceForAddress(bdInterfaces.GetAddress(0)); // Interface to D
  uint32_t bIfToE_idx = ipv4B->GetInterfaceForAddress(beInterfaces.GetAddress(0)); // Interface to E

  // Node C interfaces:
  uint32_t cIfToB_idx = ipv4C->GetInterfaceForAddress(bcInterfaces.GetAddress(1)); // Interface to B
  uint32_t cIfToA_idx = ipv4C->GetInterfaceForAddress(acInterfaces.GetAddress(1)); // Interface to A (not used for multicast from A)

  // Node D interfaces:
  uint32_t dIfToB_idx = ipv4D->GetInterfaceForAddress(bdInterfaces.GetAddress(1)); // Interface to B

  // Node E interfaces:
  uint32_t eIfToB_idx = ipv4E->GetInterfaceForAddress(beInterfaces.GetAddress(1)); // Interface to B

  // Get Node A's IP address on the A-B link, used as source IP for routing
  Ipv4Address sourceIpA_on_AB = abInterfaces.GetAddress(0);

  // 7. Configure Static Multicast Routing
  Ptr<Ipv4StaticMulticastRouting> staticRoutingA = ipv4A->GetRoutingProtocol()->GetObject<Ipv4StaticMulticastRouting>();
  Ptr<Ipv4StaticMulticastRouting> staticRoutingB = ipv4B->GetRoutingProtocol()->GetObject<Ipv4StaticMulticastRouting>();
  Ptr<Ipv4StaticMulticastRouting> staticRoutingC = ipv4C->GetRoutingProtocol()->GetObject<Ipv4StaticMulticastRouting>();
  Ptr<Ipv4StaticMulticastRouting> staticRoutingD = ipv4D->GetRoutingProtocol()->GetObject<Ipv4StaticMulticastRouting>();
  Ptr<Ipv4StaticMulticastRouting> staticRoutingE = ipv4E->GetRoutingProtocol()->GetObject<Ipv4StaticMulticastRouting>();

  // Node A (Source): Add a route for source-generated multicast packets.
  // The "blacklisting" of the A-C link is achieved by explicitly telling Node A
  // to only use its interface connected to Node B for sending multicast traffic.
  // `Ipv4Address::GetAnyIpv4Source()` (0.0.0.0) indicates locally generated traffic.
  staticRoutingA->AddMulticastRoute (Ipv4Address::GetAnyIpv4Source(), multicastGroup, aLoopbackIf_idx, {aIfToB_idx});

  // Node B (Intermediate Router & Sink):
  // Add a route for forwarding multicast packets received from A.
  // Packets for the multicast group, sourced from Node A's IP on the A-B link,
  // received on B's interface to A, should be forwarded out to C, D, and E.
  staticRoutingB->AddMulticastRoute (sourceIpA_on_AB, multicastGroup, bIfToA_idx, {bIfToC_idx, bIfToD_idx, bIfToE_idx});
  // Node B also joins the multicast group on its interface connected to Node A (upstream).
  staticRoutingB->AddHostRouteToGroup (bIfToA_idx, multicastGroup);

  // Nodes C, D, E (Sinks):
  // Each sink node joins the multicast group on its respective interface connected to Node B (upstream).
  staticRoutingC->AddHostRouteToGroup (cIfToB_idx, multicastGroup);
  staticRoutingD->AddHostRouteToGroup (dIfToB_idx, multicastGroup);
  staticRoutingE->AddHostRouteToGroup (eIfToB_idx, multicastGroup);

  // 8. Configure OnOffHelper (Multicast Source Application on Node A)
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     InetSocketAddress (multicastGroup, multicastPort));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(simulationStopTime - 1.0) + "]")); // Source runs almost for entire duration
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // No off time
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  onoff.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPacketsToSend)); // Send a defined number of packets

  ApplicationContainer sourceApps = onoff.Install (aNode);
  sourceApps.Start (Seconds (1.0)); // Start sending at 1 second
  sourceApps.Stop (Seconds (simulationStopTime - 0.5)); // Stop sending before simulation ends

  // 9. Configure PacketSinkHelper (Multicast Sink Applications on B, C, D, E)
  // Sinks listen on the multicast group address and port.
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny(), multicastPort));

  ApplicationContainer sinkAppsB = sink.Install (bNode);
  ApplicationContainer sinkAppsC = sink.Install (cNode);
  ApplicationContainer sinkAppsD = sink.Install (dNode);
  ApplicationContainer sinkAppsE = sink.Install (eNode);

  // Start sinks slightly before source to ensure they are ready
  sinkAppsB.Start (Seconds (0.5));
  sinkAppsB.Stop (Seconds (simulationStopTime));
  sinkAppsC.Start (Seconds (0.5));
  sinkAppsC.Stop (Seconds (simulationStopTime));
  sinkAppsD.Start (Seconds (0.5));
  sinkAppsD.Stop (Seconds (simulationStopTime));
  sinkAppsE.Start (Seconds (0.5));
  sinkAppsE.Stop (Seconds (simulationStopTime));

  // 10. Enable Pcap Tracing on all network devices
  p2p.EnablePcapAll ("multicast-static-blacklisted");

  // 11. Run simulation
  Simulator::Stop (Seconds (simulationStopTime));
  Simulator::Run ();

  // 12. Output packet statistics from each sink
  Ptr<PacketSink> sinkB = DynamicCast<PacketSink> (sinkAppsB.Get (0));
  Ptr<PacketSink> sinkC = DynamicCast<PacketSink> (sinkAppsC.Get (0));
  Ptr<PacketSink> sinkD = DynamicCast<PacketSink> (sinkAppsD.Get (0));
  Ptr<PacketSink> sinkE = DynamicCast<PacketSink> (sinkAppsE.Get (0));

  NS_LOG_INFO ("\n--- Packet Sink Statistics ---");
  NS_LOG_INFO ("Node B received " << sinkB->GetTotalRx () << " bytes in " << sinkB->GetTotalReceivedPackets() << " packets.");
  NS_LOG_INFO ("Node C received " << sinkC->GetTotalRx () << " bytes in " << sinkC->GetTotalReceivedPackets() << " packets.");
  NS_LOG_INFO ("Node D received " << sinkD->GetTotalRx () << " bytes in " << sinkD->GetTotalReceivedPackets() << " packets.");
  NS_LOG_INFO ("Node E received " << sinkE->GetTotalRx () << " bytes in " << sinkE->GetTotalReceivedPackets() << " packets.");
  NS_LOG_INFO ("------------------------------\n");

  Simulator::Destroy ();
  return 0;
}