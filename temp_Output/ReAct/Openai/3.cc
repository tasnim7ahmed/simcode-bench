#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point link with 5 Mbps data rate and 2 ms delay
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices and channels on nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack (IP) on both nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses to devices
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices);

  // Run simulation for 1 second (no traffic required)
  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}