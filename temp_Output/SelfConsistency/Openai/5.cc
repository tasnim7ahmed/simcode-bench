#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging (optional)
  // LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure the point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install net devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack on both nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices);

  // Enable packet capture on the point-to-point devices
  pointToPoint.EnablePcapAll ("traffic-capture");

  // Run the simulator (no traffic is generated, focus is on capturing)
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}