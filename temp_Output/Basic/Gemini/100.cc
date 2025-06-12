#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiLanWan");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  std::string csmaSpeed = "100Mbps";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("csmaSpeed", "CSMA link speed (100Mbps or 10Mbps)", csmaSpeed);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer topSwitches;
  topSwitches.Create (4);

  NodeContainer bottomSwitches;
  bottomSwitches.Create (5);

  NodeContainer topEndpoints;
  topEndpoints.Create (2);

  NodeContainer bottomEndpoints;
  bottomEndpoints.Create (2);

  NodeContainer topRouter, bottomRouter;
  topRouter.Create (1);
  bottomRouter.Create (1);


  BridgeHelper bridge;
  NetDeviceContainer topBridgeDevices;
  for (uint32_t i = 0; i < topSwitches.GetN (); ++i)
  {
    NetDeviceContainer switchDevices = bridge.Install (topSwitches.Get (i));
    topBridgeDevices.Add (switchDevices);
  }

  NetDeviceContainer bottomBridgeDevices;
  for (uint32_t i = 0; i < bottomSwitches.GetN (); ++i)
  {
    NetDeviceContainer switchDevices = bridge.Install (bottomSwitches.Get (i));
    bottomBridgeDevices.Add (switchDevices);
  }

  CsmaHelper csmaTop, csmaBottom;
  csmaTop.SetChannelAttribute ("DataRate", StringValue (csmaSpeed));
  csmaTop.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csmaBottom.SetChannelAttribute ("DataRate", StringValue (csmaSpeed));
  csmaBottom.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));


  NetDeviceContainer topCsmaDevices, bottomCsmaDevices;

  for (uint32_t i = 0; i < topEndpoints.GetN (); ++i)
  {
    NetDeviceContainer endpointDevices = csmaTop.Install (NodeContainer (topSwitches.Get (i % topSwitches.GetN ()), topEndpoints.Get (i)));
    topCsmaDevices.Add (endpointDevices.Get (0));
    topCsmaDevices.Add (endpointDevices.Get (1));
  }

   for (uint32_t i = 0; i < bottomEndpoints.GetN (); ++i)
  {
    NetDeviceContainer endpointDevices = csmaBottom.Install (NodeContainer (bottomSwitches.Get (i % bottomSwitches.GetN ()), bottomEndpoints.Get (i)));
    bottomCsmaDevices.Add (endpointDevices.Get (0));
    bottomCsmaDevices.Add (endpointDevices.Get (1));
  }

  NetDeviceContainer topRouterDevices = csmaTop.Install (NodeContainer (topSwitches.Get (0), topRouter.Get (0)));
  topCsmaDevices.Add (topRouterDevices.Get (0));
  NetDeviceContainer bottomRouterDevices = csmaBottom.Install (NodeContainer (bottomSwitches.Get (0), bottomRouter.Get (0)));
  bottomCsmaDevices.Add (bottomRouterDevices.Get (0));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("50ms"));

  NetDeviceContainer wanDevices = pointToPoint.Install (topRouter.Get (0), bottomRouter.Get (0));

  InternetStackHelper stack;
  stack.Install (topRouter);
  stack.Install (bottomRouter);
  stack.Install (topEndpoints);
  stack.Install (bottomEndpoints);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topInterfaces = address.Assign (topCsmaDevices);

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomInterfaces = address.Assign (bottomCsmaDevices);

  address.SetBase ("76.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = address.Assign (wanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (bottomEndpoints.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (bottomInterfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (topEndpoints.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServer2 (9);
  ApplicationContainer serverApps2 = echoServer2.Install (bottomEndpoints.Get (1));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (bottomInterfaces.GetAddress (3), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (topEndpoints.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  if (enablePcap)
  {
    csmaTop.EnablePcap ("multi-lan-top", topCsmaDevices, true);
    csmaBottom.EnablePcap ("multi-lan-bottom", bottomCsmaDevices, true);
    pointToPoint.EnablePcapAll ("multi-lan-wan");
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}