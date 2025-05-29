#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer leafNodes;
  leafNodes.Create(3);

  Ptr<Node> hubNode = CreateObject<Node> ();
  NodeContainer nodes;
  for (uint32_t i = 0; i < 3; ++i)
    {
      nodes.Add (leafNodes.Get(i));
      nodes.Add (hubNode);
    }

  // Each pair: {endNode, hub}
  std::vector<NetDeviceContainer> deviceContainers(3);
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  for (uint32_t i = 0; i < 3; ++i)
    {
      NodeContainer pair(leafNodes.Get(i), hubNode);
      deviceContainers[i] = csma.Install(pair);
    }

  InternetStackHelper internet;
  for (uint32_t i = 0; i < 3; ++i)
    {
      internet.Install(leafNodes.Get(i));
    }
  internet.Install(hubNode);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces(3);

  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces[0] = address.Assign(deviceContainers[0]); // leafNodes.Get(0) <-> hubNode

  address.SetBase ("10.2.2.0", "255.255.255.0");
  interfaces[1] = address.Assign(deviceContainers[1]); // leafNodes.Get(1) <-> hubNode

  address.SetBase ("10.3.3.0", "255.255.255.0");
  interfaces[2] = address.Assign(deviceContainers[2]); // leafNodes.Get(2) <-> hubNode

  Ipv4StaticRoutingHelper staticRoutingHelper;

  // Set default route for each leaf node to its hub link
  for (uint32_t i = 0; i < 3; ++i)
    {
      Ptr<Ipv4> ipv4 = leafNodes.Get(i)->GetObject<Ipv4> ();
      Ptr<Ipv4StaticRouting> staticRouting = staticRoutingHelper.GetStaticRouting (ipv4);
      Ipv4Address nextHop = interfaces[i].GetAddress(1); // hub's address on that subnet
      staticRouting->SetDefaultRoute (nextHop, 1);
    }

  // On the hub node, add static routes to all leaf subnets via their interfaces
  Ptr<Ipv4> hubIpv4 = hubNode->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> hubStaticRouting = staticRoutingHelper.GetStaticRouting (hubIpv4);

  for (uint32_t i = 0; i < 3; ++i)
    {
      Ipv4Address subnetAddress;
      if (i == 0)
        subnetAddress = Ipv4Address("10.1.1.0");
      else if (i == 1)
        subnetAddress = Ipv4Address("10.2.2.0");
      else
        subnetAddress = Ipv4Address("10.3.3.0");
      Ipv4Mask mask("255.255.255.0");
      hubStaticRouting->AddNetworkRouteTo (subnetAddress, mask, interfaces[i].GetAddress(0), i+1);
    }

  // Applications: echo servers on nodes 1 & 2, clients on node 0 and node 2
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);

  ApplicationContainer serverApps1 = echoServer.Install(leafNodes.Get(1));
  serverApps1.Start(Seconds(1.0));
  serverApps1.Stop(Seconds(10.0));

  ApplicationContainer serverApps2 = echoServer.Install(leafNodes.Get(2));
  serverApps2.Start(Seconds(1.0));
  serverApps2.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient1(interfaces[1].GetAddress(0), port);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps1 = echoClient1.Install(leafNodes.Get(0));
  clientApps1.Start(Seconds(2.0));
  clientApps1.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(0), port);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps2 = echoClient2.Install(leafNodes.Get(0));
  clientApps2.Start(Seconds(4.0));
  clientApps2.Stop(Seconds(10.0));

  // Enable packet capture on all devices
  for (uint32_t i = 0; i < 3; ++i)
    {
      csma.EnablePcap ("hub-vlan", deviceContainers[i].Get(0), true); // leaf
      csma.EnablePcap ("hub-vlan", deviceContainers[i].Get(1), true); // hub
    }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}