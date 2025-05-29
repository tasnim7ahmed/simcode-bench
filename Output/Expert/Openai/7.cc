#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer ndc[4];
  for (uint32_t i = 0; i < 4; ++i)
  {
    ndc[i] = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
  }

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer ifaces[4];
  for (uint32_t i = 0; i < 4; ++i)
  {
    std::ostringstream subnet;
    subnet << "192.9.39." << i * 8 << "/29";
    address.SetBase(subnet.str().c_str(), "255.255.255.248");
    ifaces[i] = address.Assign(ndc[i]);
    address.NewNetwork();
  }

  // Map each node's main interface (using interface toward next node in ring)
  Ipv4Address nodeIp[4];
  for (uint32_t i = 0; i < 4; ++i)
  {
    // Assign primary interface for each node as the lower address on each link
    nodeIp[i] = ifaces[i].GetAddress(0);
  }

  uint16_t echoPort = 9;

  ApplicationContainer serverApps[4], clientApps[4];

  // Node 0 server, Node 1 client
  UdpEchoServerHelper echoServer0(echoPort);
  serverApps[0] = echoServer0.Install(nodes.Get(0));
  serverApps[0].Start(Seconds(0.0));
  serverApps[0].Stop(Seconds(5.0));

  UdpEchoClientHelper echoClient0(nodeIp[0], echoPort);
  echoClient0.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps[0] = echoClient0.Install(nodes.Get(1));
  clientApps[0].Start(Seconds(1.0));
  clientApps[0].Stop(Seconds(5.0));

  // Node 1 server, Node 2 client
  UdpEchoServerHelper echoServer1(echoPort);
  serverApps[1] = echoServer1.Install(nodes.Get(1));
  serverApps[1].Start(Seconds(5.1));
  serverApps[1].Stop(Seconds(10.1));

  UdpEchoClientHelper echoClient1(nodeIp[1], echoPort);
  echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps[1] = echoClient1.Install(nodes.Get(2));
  clientApps[1].Start(Seconds(6.0));
  clientApps[1].Stop(Seconds(10.1));

  // Node 2 server, Node 3 client
  UdpEchoServerHelper echoServer2(echoPort);
  serverApps[2] = echoServer2.Install(nodes.Get(2));
  serverApps[2].Start(Seconds(10.2));
  serverApps[2].Stop(Seconds(15.2));

  UdpEchoClientHelper echoClient2(nodeIp[2], echoPort);
  echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps[2] = echoClient2.Install(nodes.Get(3));
  clientApps[2].Start(Seconds(11.0));
  clientApps[2].Stop(Seconds(15.2));

  // Node 3 server, Node 0 client
  UdpEchoServerHelper echoServer3(echoPort);
  serverApps[3] = echoServer3.Install(nodes.Get(3));
  serverApps[3].Start(Seconds(15.3));
  serverApps[3].Stop(Seconds(20.3));

  UdpEchoClientHelper echoClient3(nodeIp[3], echoPort);
  echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
  echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
  clientApps[3] = echoClient3.Install(nodes.Get(0));
  clientApps[3].Start(Seconds(16.0));
  clientApps[3].Stop(Seconds(20.3));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(21.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}