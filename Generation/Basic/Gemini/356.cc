#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Enable logging for specific components for better understanding
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // 1. Create two nodes for the P2P network
  NodeContainer nodes;
  nodes.Create (2);

  // 2. Configure Point-to-Point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // 3. Install P2P devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // 4. Install the Internet stack on the nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // 5. Assign IP addresses from the 10.1.1.0/24 subnet
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // 6. Setup PacketSink application on Node 1 (receiver)
  uint16_t sinkPort = 9; // Port for the PacketSink application
  
  // Get the IP address of Node 1 (the receiver)
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));

  // Create and install PacketSinkHelper
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1)); // Install on Node 1
  sinkApps.Start (Seconds (1.0)); // Start receiving at 1 second
  sinkApps.Stop (Seconds (10.0)); // Stop receiving at 10 seconds

  // 7. Setup BulkSend application on Node 0 (sender)
  // Create and install BulkSendHelper pointing to Node 1's address and port
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
  
  // Set MaxBytes to 0 for unlimited data transmission
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer clientApps = bulkSendHelper.Install (nodes.Get (0)); // Install on Node 0
  clientApps.Start (Seconds (1.0)); // Start sending at 1 second
  clientApps.Stop (Seconds (10.0)); // Stop sending at 10 seconds

  // 8. Set the global simulation stop time
  Simulator::Stop (Seconds (10.0));

  // 9. Run the simulation
  Simulator::Run ();
  Simulator::Destroy (); // Clean up simulation resources

  return 0;
}