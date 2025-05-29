#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer spokeNodes;
  spokeNodes.Create (8);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices[8];

  for (int i = 0; i < 8; ++i)
    {
      NetDeviceContainer devices = pointToPoint.Install (hubNode.Get (0), spokeNodes.Get (i));
      hubDevices.Add (devices.Get (0));
      spokeDevices[i].Add (devices.Get (1));
    }

  InternetStackHelper stack;
  stack.Install (hubNode);
  stack.Install (spokeNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces[8];

  for (int i = 0; i < 8; ++i)
    {
      address.Assign (NetDeviceContainer (hubDevices.Get (i)));
      hubInterfaces.Add (address.GetAddress (0));
      address.Assign (NetDeviceContainer (spokeDevices[i].Get (0)));
      spokeInterfaces[i].Add (address.GetAddress (0));
      address.NewNetwork ();
    }

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (hubInterfaces.GetAddress (0), 50000));
  ApplicationContainer sinkApp = sinkHelper.Install (hubNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper onOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (hubInterfaces.GetAddress (0), 50000));
  onOffHelper.SetConstantRate (DataRate ("14kbps"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (137));

  ApplicationContainer spokeApps[8];
  for (int i = 0; i < 8; ++i)
    {
        spokeApps[i] = onOffHelper.Install (spokeNodes.Get (i));
        spokeApps[i].Start (Seconds (2.0 + i * 0.5));
        spokeApps[i].Stop (Seconds (9.0));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  pointToPoint.EnablePcapAll ("star");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}