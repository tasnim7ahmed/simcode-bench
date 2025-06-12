#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  // Set time resolution and logging if needed
  Time::SetResolution(Time::NS);

  // Create three CSMA nodes
  NodeContainer nodes;
  nodes.Create(3);

  // Configure CSMA channel and devices
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // Set up UDP server on Node 2 (indexing from 0)
  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(2));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Set up UDP client on Node 0 to send to server (Node 2)
  UdpClientHelper client(interfaces.GetAddress(2), port);
  client.SetAttribute("MaxPackets", UintegerValue(320)); // 40 packets/sec x 8s = 320
  client.SetAttribute("Interval", TimeValue(Seconds(0.025))); // 40 packets/sec
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}