#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiSwitchLanWan");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string wanDataRate = "10Mbps";
  std::string wanDelay = "2ms";
  std::string lan100MDataRate = "100Mbps";
  std::string lan10MDataRate = "10Mbps";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable pcap tracing", enablePcap);
  cmd.AddValue ("wanDataRate", "Wan data rate", wanDataRate);
  cmd.AddValue ("wanDelay", "Wan delay", wanDelay);
  cmd.AddValue ("lan100MDataRate", "LAN 100M data rate", lan100MDataRate);
  cmd.AddValue ("lan10MDataRate", "LAN 10M data rate", lan10MDataRate);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer topNodes, bottomNodes, routers;
  topNodes.Create (3);
  bottomNodes.Create (3);
  routers.Create (2);

  NodeContainer allNodes;
  allNodes.Add (topNodes);
  allNodes.Add (bottomNodes);
  allNodes.Add (routers);

  InternetStackHelper stack;
  stack.Install (allNodes);

  // Top LAN
  NodeContainer topSwitch1, topSwitch2;
  topSwitch1.Create (1);
  topSwitch2.Create (1);

  BridgeHelper bridgeHelper;
  NetDeviceContainer bridgeDevices1, bridgeDevices2;

  CsmaHelper csmaTop100M;
  csmaTop100M.SetChannelAttribute ("DataRate", StringValue (lan100MDataRate));
  csmaTop100M.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer topT2Switch1 = csmaTop100M.Install (NodeContainer (topNodes.Get (0), topSwitch1.Get (0)));
  bridgeDevices1.Add (topT2Switch1.Get (1));

  NetDeviceContainer topSwitch1Switch2 = csmaTop100M.Install (NodeContainer (topSwitch1.Get (0), topSwitch2.Get (0)));
  bridgeDevices1.Add (topSwitch1Switch2.Get (0));
  bridgeDevices2.Add (topSwitch1Switch2.Get (1));

  NetDeviceContainer topSwitch2Tr = csmaTop100M.Install (NodeContainer (topSwitch2.Get (0), routers.Get (0)));
  bridgeDevices2.Add (topSwitch2Tr.Get (0));

  CsmaHelper csmaTop10M;
  csmaTop10M.SetChannelAttribute ("DataRate", StringValue (lan10MDataRate));
  csmaTop10M.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer topT3Switch2 = csmaTop10M.Install (NodeContainer (topNodes.Get (1), topSwitch2.Get (0)));
  bridgeDevices2.Add (topT3Switch2.Get (1));


  bridgeHelper.Install (topSwitch1.Get (0), bridgeDevices1);
  bridgeHelper.Install (topSwitch2.Get (0), bridgeDevices2);

  // Bottom LAN
  NodeContainer bottomSwitch1, bottomSwitch2;
  bottomSwitch1.Create (1);
  bottomSwitch2.Create (1);

  NetDeviceContainer bridgeDevices3, bridgeDevices4;

  NetDeviceContainer bottomB2Switch1 = csmaTop100M.Install (NodeContainer (bottomNodes.Get (0), bottomSwitch1.Get (0)));
  bridgeDevices3.Add (bottomB2Switch1.Get (1));

  NetDeviceContainer bottomSwitch1Switch2 = csmaTop100M.Install (NodeContainer (bottomSwitch1.Get (0), bottomSwitch2.Get (0)));
  bridgeDevices3.Add (bottomSwitch1Switch2.Get (0));
  bridgeDevices4.Add (bottomSwitch1Switch2.Get (1));

  NetDeviceContainer bottomSwitch2Br = csmaTop100M.Install (NodeContainer (bottomSwitch2.Get (0), routers.Get (1)));
  bridgeDevices4.Add (bottomSwitch2Br.Get (0));

  NetDeviceContainer bottomB3Switch2 = csmaTop10M.Install (NodeContainer (bottomNodes.Get (1), bottomSwitch2.Get (0)));
  bridgeDevices4.Add (bottomB3Switch2.Get (1));

  bridgeHelper.Install (bottomSwitch1.Get (0), bridgeDevices3);
  bridgeHelper.Install (bottomSwitch2.Get (0), bridgeDevices4);

  // WAN
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (wanDataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (wanDelay));

  NetDeviceContainer wanDevices = pointToPoint.Install (routers);

  // Addressing
  Ipv4AddressHelper address;

  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topInterfaces = address.Assign (NetDeviceContainer (topT2Switch1.Get (0), topT3Switch2.Get (0), topSwitch2Tr.Get (1)));

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomInterfaces = address.Assign (NetDeviceContainer (bottomB2Switch1.Get (0), bottomB3Switch2.Get (0), bottomSwitch2Br.Get (1)));

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wanInterfaces = address.Assign (wanDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Applications
  UdpEchoServerHelper echoServerTop (9);
  ApplicationContainer serverAppTop = echoServerTop.Install (topNodes.Get (1));
  serverAppTop.Start (Seconds (1.0));
  serverAppTop.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientTop (topInterfaces.GetAddress (1), 9);
  echoClientTop.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClientTop.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientTop.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppTop = echoClientTop.Install (topNodes.Get (0));
  clientAppTop.Start (Seconds (2.0));
  clientAppTop.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServerBottom (9);
  ApplicationContainer serverAppBottom = echoServerBottom.Install (bottomNodes.Get (0));
  serverAppBottom.Start (Seconds (1.0));
  serverAppBottom.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClientBottom (bottomInterfaces.GetAddress (0), 9);
  echoClientBottom.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClientBottom.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClientBottom.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientAppBottom = echoClientBottom.Install (bottomNodes.Get (1));
  clientAppBottom.Start (Seconds (2.0));
  clientAppBottom.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt (Seconds (5.0));

  if (enablePcap)
  {
    csmaTop100M.EnablePcap ("top-lan", topSwitch1.Get (0)->GetId (), true);
    pointToPoint.EnablePcapAll ("wan");
  }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}