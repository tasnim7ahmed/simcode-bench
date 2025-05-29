#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dvdv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorRouting");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("DvdvRoutingProtocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4RoutingTable", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);

  NodeContainer routers;
  routers.Create (3);

  NodeContainer subnets[4];
  for (int i = 0; i < 4; ++i)
    {
      subnets[i].Create (1);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer routerDevices[3];

  NetDeviceContainer subnetDevices[4];

  NetDeviceContainer linkRouter0Router1 = p2p.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer linkRouter0Router2 = p2p.Install (routers.Get (0), routers.Get (2));
  NetDeviceContainer linkRouter1Router2 = p2p.Install (routers.Get (1), routers.Get (2));

  NetDeviceContainer linkRouter0Subnet0 = p2p.Install (routers.Get (0), subnets[0].Get (0));
  NetDeviceContainer linkRouter0Subnet1 = p2p.Install (routers.Get (0), subnets[1].Get (0));
  NetDeviceContainer linkRouter1Subnet2 = p2p.Install (routers.Get (1), subnets[2].Get (0));
  NetDeviceContainer linkRouter2Subnet3 = p2p.Install (routers.Get (2), subnets[3].Get (0));

  InternetStackHelper internet;
  internet.Install (routers);
  for (int i = 0; i < 4; ++i)
    {
      internet.Install (subnets[i]);
    }

  DvdvHelper dvdv;
  Ipv4ListRoutingHelper routingHelper;
  routingHelper.Add (dvdv, 0);

  Ipv4InterfaceContainer routerInterfaces[3];
  Ipv4InterfaceContainer subnetInterfaces[4];

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  routerInterfaces[0] = address.Assign (linkRouter0Router1);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  routerInterfaces[1] = address.Assign (linkRouter0Router2);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  routerInterfaces[2] = address.Assign (linkRouter1Router2);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  subnetInterfaces[0] = address.Assign (linkRouter0Subnet0);
  address.SetBase ("10.1.5.0", "255.255.255.0");
  subnetInterfaces[1] = address.Assign (linkRouter0Subnet1);
  address.SetBase ("10.1.6.0", "255.255.255.0");
  subnetInterfaces[2] = address.Assign (linkRouter1Subnet2);
  address.SetBase ("10.1.7.0", "255.255.255.0");
  subnetInterfaces[3] = address.Assign (linkRouter2Subnet3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (subnets[3].Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (subnetInterfaces[3].GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (subnets[0].Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      p2p.EnablePcapAll ("dvr");
    }
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}