#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BridgeExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  std::string csmaDataRate = "100Mbps";

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable or disable PCAP traces", enablePcap);
  cmd.AddValue ("CsmaDataRate", "CSMA Data Rate (10Mbps or 100Mbps)", csmaDataRate);
  cmd.Parse (argc, argv);

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

  NetDeviceContainer topSwitchDevices;
  NetDeviceContainer bottomSwitchDevices;

  CsmaHelper csmaTop, csmaBottom;
  csmaTop.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaTop.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  csmaBottom.SetChannelAttribute ("DataRate", StringValue (csmaDataRate));
  csmaBottom.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));


  for (uint32_t i = 0; i < 4; ++i) {
      NetDeviceContainer temp = csmaTop.Install (NodeContainer (topSwitches.Get (i), topSwitches.Get ((i + 1) % 4)));
      topSwitchDevices.Add (temp.Get (0));
      topSwitchDevices.Add (temp.Get (1));
  }
    
  for (uint32_t i = 0; i < 5; ++i) {
      NetDeviceContainer temp = csmaBottom.Install (NodeContainer (bottomSwitches.Get (i), bottomSwitches.Get ((i + 1) % 5)));
      bottomSwitchDevices.Add (temp.Get (0));
      bottomSwitchDevices.Add (temp.Get (1));
  }

  NetDeviceContainer topEndpointDevices = csmaTop.Install (NodeContainer (topEndpoints.Get (0), topSwitches.Get (0)));
  topSwitchDevices.Add (topEndpointDevices.Get (1));

  topEndpointDevices = csmaTop.Install (NodeContainer (topEndpoints.Get (1), topSwitches.Get (3)));
  topSwitchDevices.Add (topEndpointDevices.Get (1));

  NetDeviceContainer bottomEndpointDevices = csmaBottom.Install (NodeContainer (bottomEndpoints.Get (0), bottomSwitches.Get (0)));
  bottomSwitchDevices.Add (bottomEndpointDevices.Get (1));

  bottomEndpointDevices = csmaBottom.Install (NodeContainer (bottomEndpoints.Get (1), bottomSwitches.Get (4)));
  bottomSwitchDevices.Add (bottomEndpointDevices.Get (1));

  for (uint32_t i = 0; i < 4; ++i) {
    bridge.Install (topSwitches.Get (i), topSwitchDevices.Get (2*i));
    bridge.Install (topSwitches.Get (i), topSwitchDevices.Get (2*i + 1));
  }
  
  for (uint32_t i = 0; i < 5; ++i) {
    bridge.Install (bottomSwitches.Get (i), bottomSwitchDevices.Get (2*i));
    bridge.Install (bottomSwitches.Get (i), bottomSwitchDevices.Get (2*i + 1));
  }

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("50ms"));

  NetDeviceContainer topRouterDevices, bottomRouterDevices;
  NetDeviceContainer wanDevices = pointToPoint.Install (topRouter, bottomRouter);
  topRouterDevices = csmaTop.Install (topRouter, topSwitches.Get (1));
  bottomRouterDevices = csmaBottom.Install (bottomRouter, bottomSwitches.Get (2));

  bridge.Install(topRouter.Get(0), topRouterDevices.Get(1));
  bridge.Install(bottomRouter.Get(0), bottomRouterDevices.Get(1));

  InternetStackHelper stack;
  stack.Install (topRouter);
  stack.Install (bottomRouter);
  stack.Install (topEndpoints);
  stack.Install (bottomEndpoints);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topRouterInterfaces = address.Assign (topRouterDevices);

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomRouterInterfaces = address.Assign (bottomRouterDevices);
  
  address.SetBase ("76.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = address.Assign (wanDevices);

  Ipv4AddressHelper endpointAddressTop, endpointAddressBottom;
  endpointAddressTop.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topEndpointInterfaces = endpointAddressTop.Assign (topEndpointDevices);

  endpointAddressBottom.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomEndpointInterfaces = endpointAddressBottom.Assign (bottomEndpointDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (bottomEndpoints.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (bottomEndpointInterfaces.GetAddress (0), 9);
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

  UdpEchoClientHelper echoClient2 (bottomEndpointInterfaces.GetAddress (1), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (topEndpoints.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  if (enablePcap)
    {
      csmaTop.EnablePcap ("top-lan", topSwitches.Get (0)->GetId (), 0);
      csmaBottom.EnablePcap ("bottom-lan", bottomSwitches.Get (0)->GetId (), 0);
      pointToPoint.EnablePcapAll ("wan");
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}