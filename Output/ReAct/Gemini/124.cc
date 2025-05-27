#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HubRoutingExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer hubNode;
  hubNode.Create (1);

  NodeContainer clientNodes;
  clientNodes.Create (3);

  NodeContainer allNodes;
  allNodes.Add (hubNode);
  allNodes.Add (clientNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (Seconds (0.002)));

  NetDeviceContainer hubDevice;
  for (int i = 0; i < 3; ++i)
    {
      NodeContainer tempNodes;
      tempNodes.Add (hubNode);
      tempNodes.Add (clientNodes.Get (i));
      hubDevice.Add (csma.Install (tempNodes));
    }

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer hubInterface;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  hubInterface.Add (address.Assign (hubDevice.Get (0)));

  address.SetBase ("10.1.2.0", "255.255.255.0");
  hubInterface.Add (address.Assign (hubDevice.Get (1)));

  address.SetBase ("10.1.3.0", "255.255.255.0");
  hubInterface.Add (address.Assign (hubDevice.Get (2)));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (clientNodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper echoClient (hubInterface.GetAddress (5), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (clientNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  
  UdpClientHelper echoClient2 (hubInterface.GetAddress (5), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (clientNodes.Get (1));
  clientApps2.Start (Seconds (3.0));
  clientApps2.Stop (Seconds (10.0));
  

  if (tracing == true)
    {
      csma.EnablePcapAll ("hub-routing-example");
    }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}