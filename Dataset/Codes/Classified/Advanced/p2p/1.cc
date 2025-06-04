/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Set up the point-to-point link between the two nodes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5 Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2 ms"));

  // Install network devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install the Internet stack (IP protocol)
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses to the devices
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create a UDP echo server on node 1
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Create a UDP echo client on node 0
  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable packet capture
  pointToPoint.EnablePcapAll("simple-point-to-point");

  // Run the simulation
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

