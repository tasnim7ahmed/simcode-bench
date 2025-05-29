#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices and channel
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (devices);

  // Enable pcap capture on the point-to-point devices
  pointToPoint.EnablePcapAll ("traffic-capture");

  // Run simulation for a short duration
  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}