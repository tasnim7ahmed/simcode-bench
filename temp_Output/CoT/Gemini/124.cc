#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HubTrunkSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer subnetNodes;
  subnetNodes.Create (3);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer hubDevices;
  NetDeviceContainer subnetDevices;

  for (int i = 0; i < 3; ++i)
    {
      NodeContainer link;
      link.Add (hubNode.Get (0));
      link.Add (subnetNodes.Get (i));

      NetDeviceContainer device = csma.Install (link);
      hubDevices.Add (device.Get (0));
      subnetDevices.Add (device.Get (1));
    }

  InternetStackHelper stack;
  stack.Install (hubNode);
  stack.Install (subnetNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces1 = address.Assign (hubDevices.Get (0));
  Ipv4InterfaceContainer subnetInterfaces1 = address.Assign (subnetDevices.Get (0));

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces2 = address.Assign (hubDevices.Get (1));
  Ipv4InterfaceContainer subnetInterfaces2 = address.Assign (subnetDevices.Get (1));

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer hubInterfaces3 = address.Assign (hubDevices.Get (2));
  Ipv4InterfaceContainer subnetInterfaces3 = address.Assign (subnetDevices.Get (2));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (subnetNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (subnetInterfaces1.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (subnetNodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (subnetInterfaces1.GetAddress (0), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (subnetNodes.Get (2));
  clientApps2.Start (Seconds (3.0));
  clientApps2.Stop (Seconds (10.0));


  csma.EnablePcapAll ("hub-trunk");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}