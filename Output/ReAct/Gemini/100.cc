#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BridgeWanSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  std::string lanLinkSpeed = "100Mbps";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP traces", enablePcap);
  cmd.AddValue ("lanLinkSpeed", "LAN link speed (100Mbps or 10Mbps)", lanLinkSpeed);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogFlag (LOG_PREFIX_TIME | LOG_PREFIX_NODE);

  NodeContainer topLanNodes;
  topLanNodes.Create (6);
  NodeContainer bottomLanNodes;
  bottomLanNodes.Create (7);
  NodeContainer routers;
  routers.Create (2);

  NodeContainer topSwitches;
  topSwitches.Add (topLanNodes.Get (0));
  topSwitches.Add (topLanNodes.Get (1));
  topSwitches.Add (topLanNodes.Get (2));
  topSwitches.Add (topLanNodes.Get (3));

  NodeContainer bottomSwitches;
  bottomSwitches.Add (bottomLanNodes.Get (0));
  bottomSwitches.Add (bottomLanNodes.Get (1));
  bottomSwitches.Add (bottomLanNodes.Get (2));
  bottomSwitches.Add (bottomLanNodes.Get (3));
  bottomSwitches.Add (bottomLanNodes.Get (4));

  NodeContainer topEndpoints;
  topEndpoints.Add (topLanNodes.Get (4));
  topEndpoints.Add (topLanNodes.Get (5));
  NodeContainer bottomEndpoints;
  bottomEndpoints.Add (bottomLanNodes.Get (5));
  bottomEndpoints.Add (bottomLanNodes.Get (6));


  InternetStackHelper stack;
  stack.InstallAll ();
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  CsmaHelper csmaLan;
  csmaLan.SetChannelAttribute ("DataRate", StringValue (lanLinkSpeed));
  csmaLan.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));

  BridgeHelper bridgeHelper;

  NetDeviceContainer topSwitchDevices;
  for (uint32_t i = 0; i < topSwitches.GetN (); ++i)
    {
      NetDeviceContainer bridgeDevices = bridgeHelper.Install (topSwitches.Get (i), NetDeviceContainer ());
      topSwitchDevices.Add (bridgeDevices.Get (0));
    }

  NetDeviceContainer bottomSwitchDevices;
  for (uint32_t i = 0; i < bottomSwitches.GetN (); ++i)
    {
      NetDeviceContainer bridgeDevices = bridgeHelper.Install (bottomSwitches.Get (i), NetDeviceContainer ());
      bottomSwitchDevices.Add (bridgeDevices.Get (0));
    }
  
  NetDeviceContainer topLanDevices;
  topLanDevices.Add (csmaLan.Install (topSwitches.Get (0), topEndpoints.Get (0)));
  topLanDevices.Add (csmaLan.Install (topSwitches.Get (3), topEndpoints.Get (1)));
  topLanDevices.Add (csmaLan.Install (topSwitches.Get (0), routers.Get (0)));
  
  NetDeviceContainer bottomLanDevices;
  bottomLanDevices.Add (csmaLan.Install (bottomSwitches.Get (1), bottomEndpoints.Get (0)));
  bottomLanDevices.Add (csmaLan.Install (bottomSwitches.Get (4), bottomEndpoints.Get (1)));
  bottomLanDevices.Add (csmaLan.Install (bottomSwitches.Get (0), routers.Get (1)));
  
  NetDeviceContainer switchLinks1 = csmaLan.Install (topSwitches.Get (0), topSwitches.Get (1));
  NetDeviceContainer switchLinks2 = csmaLan.Install (topSwitches.Get (1), topSwitches.Get (2));
  NetDeviceContainer switchLinks3 = csmaLan.Install (topSwitches.Get (2), topSwitches.Get (3));

  NetDeviceContainer switchLinks4 = csmaLan.Install (bottomSwitches.Get (0), bottomSwitches.Get (1));
  NetDeviceContainer switchLinks5 = csmaLan.Install (bottomSwitches.Get (1), bottomSwitches.Get (2));
  NetDeviceContainer switchLinks6 = csmaLan.Install (bottomSwitches.Get (2), bottomSwitches.Get (3));
  NetDeviceContainer switchLinks7 = csmaLan.Install (bottomSwitches.Get (3), bottomSwitches.Get (4));

  
  CsmaHelper csmaWan;
  csmaWan.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csmaWan.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (50)));
  NetDeviceContainer wanDevices = csmaWan.Install (routers);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topLanInterfaces = address.Assign (topLanDevices);

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomLanInterfaces = address.Assign (bottomLanDevices);

  address.SetBase ("76.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = address.Assign (wanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverAppsTop = echoServer.Install (topEndpoints.Get (0));
  serverAppsTop.Start (Seconds (1.0));
  serverAppsTop.Stop (Seconds (10.0));

  ApplicationContainer serverAppsBottom = echoServer.Install (bottomEndpoints.Get (0));
  serverAppsBottom.Start (Seconds (1.0));
  serverAppsBottom.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientTop (topLanInterfaces.GetAddress (1), 9);
  echoClientTop.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientTop.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientTop.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsTop = echoClientTop.Install (bottomEndpoints.Get (0));
  clientAppsTop.Start (Seconds (2.0));
  clientAppsTop.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientBottom (bottomLanInterfaces.GetAddress (1), 9);
  echoClientBottom.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClientBottom.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientBottom.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppsBottom = echoClientBottom.Install (topEndpoints.Get (1));
  clientAppsBottom.Start (Seconds (2.0));
  clientAppsBottom.Stop (Seconds (10.0));

  if (enablePcap)
    {
      csmaLan.EnablePcap ("bridge-wan", topLanDevices.Get (0), true);
      csmaLan.EnablePcap ("bridge-wan", bottomLanDevices.Get (0), true);
      csmaWan.EnablePcap ("bridge-wan", wanDevices.Get (0), true);
    }

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("bridge-wan-animation.xml");

  anim.SetNodePosition (topSwitches.Get(0), 1, 1);
  anim.SetNodePosition (topSwitches.Get(1), 2, 1);
  anim.SetNodePosition (topSwitches.Get(2), 3, 1);
  anim.SetNodePosition (topSwitches.Get(3), 4, 1);
  anim.SetNodePosition (topEndpoints.Get(0), 1, 2);
  anim.SetNodePosition (topEndpoints.Get(1), 4, 2);

  anim.SetNodePosition (bottomSwitches.Get(0), 1, 4);
  anim.SetNodePosition (bottomSwitches.Get(1), 2, 4);
  anim.SetNodePosition (bottomSwitches.Get(2), 3, 4);
  anim.SetNodePosition (bottomSwitches.Get(3), 4, 4);
  anim.SetNodePosition (bottomSwitches.Get(4), 5, 4);
  anim.SetNodePosition (bottomEndpoints.Get(0), 2, 5);
  anim.SetNodePosition (bottomEndpoints.Get(1), 5, 5);

  anim.SetNodePosition (routers.Get(0), 5, 1);
  anim.SetNodePosition (routers.Get(1), 5, 3);
  
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}