#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("PointToPointUdp"); // Not requested, so remove to be precise.

int main (int argc, char *argv[])
{
  // 1. Create two nodes
  ns3::NodeContainer nodes;
  nodes.Create (2);

  // 2. Configure Point-to-Point link attributes
  ns3::PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("5ms"));

  // 3. Install network devices on nodes
  ns3::NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // 4. Install the Internet stack on nodes
  ns3::InternetStackHelper internet;
  internet.Install (nodes);

  // 5. Assign IP addresses
  ns3::Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // 6. Setup UDP Server on Node 1 (receiver)
  uint16_t port = 9; // Discard port
  ns3::UdpServerHelper server (port);
  ns3::ApplicationContainer serverApps = server.Install (nodes.Get (1));
  serverApps.Start (ns3::Seconds (1.0)); // Server starts at 1s
  // No explicit stop time for server, it will run until simulation ends

  // 7. Setup UDP Client on Node 0 (sender)
  ns3::UdpClientHelper client (interfaces.GetAddress (1), port); // Send to Node 1's IP
  client.SetAttribute ("MaxPackets", ns3::UintegerValue (0)); // Send infinite packets
  client.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (0.01))); // Send every 10ms (100 packets/sec)
  client.SetAttribute ("PacketSize", ns3::UintegerValue (1024)); // 1KB packet size
  client.SetAttribute ("Rate", ns3::DataRateValue(ns3::DataRate("10Mbps"))); // Target rate

  ns3::ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (ns3::Seconds (2.0));  // Client starts sending at 2s
  clientApps.Stop (ns3::Seconds (10.0)); // Client stops sending at 10s

  // 8. Set simulation stop time
  ns3::Simulator::Stop (ns3::Seconds (10.0));

  // 9. Run the simulation
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}