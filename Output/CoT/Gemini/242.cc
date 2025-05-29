#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("6LoWPAN-IoT");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  uint32_t numDevices = 5;
  uint16_t port = 9;
  std::string animFile = "6lowpan-iot.xml";

  CommandLine cmd;
  cmd.AddValue ("numDevices", "Number of IoT devices", numDevices);
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("animFile", "File name for animation output", animFile);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer devices;
  devices.Create (numDevices);

  NodeContainer server;
  server.Create (1);

  NodeContainer allNodes;
  allNodes.Add (devices);
  allNodes.Add (server);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer deviceInterfaces = lrWpanHelper.Install (devices);
  NetDeviceContainer serverInterface = lrWpanHelper.Install (server);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDeviceInterfaces = sixLowPanHelper.Install (deviceInterfaces);
  NetDeviceContainer sixLowPanServerInterface = sixLowPanHelper.Install (serverInterface);

  InternetStackHelper internetv6;
  internetv6.Install (allNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:0::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer deviceAddresses = ipv6.Assign (sixLowPanDeviceInterfaces);
  Ipv6InterfaceContainer serverAddress = ipv6.Assign (sixLowPanServerInterface);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> serverRouting = ipv6RoutingHelper.GetOrCreateRoutingProtocol (server.Get (0));
  serverRouting->SetDefaultRoute (Ipv6Address ("2001:db8:0:0:0:0:0:1"), 0);

  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      Ptr<Ipv6StaticRouting> deviceRouting = ipv6RoutingHelper.GetOrCreateRoutingProtocol (devices.Get (i));
      deviceRouting->SetDefaultRoute (Ipv6Address ("2001:db8:0:0::1"), 0);
    }

  UdpServerHelper serverApp (port);
  ApplicationContainer serverApps = serverApp.Install (server.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper clientApp (serverAddress.GetAddress (0, 1), port);
  clientApp.SetAttribute ("MaxPackets", UintegerValue (100));
  clientApp.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientApp.SetAttribute ("PacketSize", UintegerValue (10));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      clientApps.Add (clientApp.Install (devices.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      lrWpanHelper.EnablePcapAll ("6lowpan-iot");
    }

  AnimationInterface anim (animFile);
  anim.SetConstantPosition (server.Get (0), 10.0, 10.0);
  for(uint32_t i = 0; i < numDevices; ++i){
      anim.SetConstantPosition (devices.Get(i), 20.0 + i*5, 20.0);
  }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}