#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
  // Simulation parameters
  uint32_t nSpokes = 8;
  double simTime = 10.0;

  // Create nodes: 1 hub + nSpokes
  NodeContainer allNodes;
  allNodes.Create (nSpokes + 1);
  Ptr<Node> hubNode = allNodes.Get (0);
  NodeContainer spokeNodes;
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    spokeNodes.Add (allNodes.Get (i + 1));
  }

  // Configure PointToPointHelper
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install InternetStack on all nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  // Containers for devices and interfaces on each link
  std::vector<NetDeviceContainer> deviceContainers(nSpokes);
  std::vector<Ipv4InterfaceContainer> interfaceContainers(nSpokes);
  Ipv4AddressHelper ipv4;

  // Create links between hub and each spoke, and assign IP addresses
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    NetDeviceContainer linkDevices = p2p.Install (hubNode, spokeNodes.Get (i));
    deviceContainers[i] = linkDevices;

    std::string networkAddress = "10.1." + std::to_string(i + 1) + ".0";
    ipv4.SetBase (networkAddress.c_str (), "255.255.255.252"); // /30 subnet
    interfaceContainers[i] = ipv4.Assign (linkDevices);
  }

  // Create a Packet Sink on the hub node
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), 50000));
  ApplicationContainer sinkApps = sinkHelper.Install (hubNode);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  // Create OnOff applications on spoke nodes
  for (uint32_t i = 0; i < nSpokes; ++i)
  {
    // Get the hub's specific IP address on this link for the OnOff application
    // interfaceContainers[i].GetAddress(0) refers to the address on the first node in the pair (hub)
    Ipv4Address hubSpecificIp = interfaceContainers[i].GetAddress (0).GetLocal ();

    OnOffHelper onoffHelper ("ns3::TcpSocketFactory",
                             InetSocketAddress (hubSpecificIp, 50000));
    onoffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Always On
    onoffHelper.SetAttribute ("PacketSize", UintegerValue (137));
    onoffHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("14kbps")));

    ApplicationContainer clientApps = onoffHelper.Install (spokeNodes.Get (i));
    clientApps.Start (Seconds (1.0)); // Start slightly after the sink
    clientApps.Stop (Seconds (simTime));
  }

  // Enable static global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Enable pcap traces for all devices
  p2p.EnablePcapAll ("star-topology");

  // Run the simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}