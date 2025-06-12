#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiSwitchLan");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string dataRateStr = "100Mbps";
  std::string wanDataRateStr = "10Mbps";
  std::string wanDelayStr = "2ms";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("dataRate", "LAN data rate", dataRateStr);
  cmd.AddValue ("wanDataRate", "WAN data rate", wanDataRateStr);
  cmd.AddValue ("wanDelay", "WAN delay", wanDelayStr);
  cmd.Parse (argc, argv);

  DataRate lanDataRate = DataRate(dataRateStr);
  DataRate wanDataRate = DataRate(wanDataRateStr);
  Time wanDelay = TimeValue(Seconds(0));
  wanDelay = Time::FromString(wanDelayStr, wanDelay);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer topNodes, bottomNodes, routers;
  topNodes.Create (3);
  bottomNodes.Create (3);
  routers.Create (2);

  NodeContainer allNodes;
  allNodes.Add (topNodes);
  allNodes.Add (bottomNodes);
  allNodes.Add (routers);

  NS_LOG_INFO ("Create channels.");
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (lanDataRate));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  PointToPointHelper p2pHelper;
  p2pHelper.SetDeviceAttribute ("DataRate", DataRateValue (wanDataRate));
  p2pHelper.SetChannelAttribute ("Delay", TimeValue (wanDelay));

  NetDeviceContainer topLanDevices1, topLanDevices2, topLanDevices3;
  NetDeviceContainer bottomLanDevices1, bottomLanDevices2, bottomLanDevices3;
  NetDeviceContainer wanDevices;

  NodeContainer topSwitchNodes1, topSwitchNodes2, bottomSwitchNodes1, bottomSwitchNodes2;
  topSwitchNodes1.Create(2);
  topSwitchNodes2.Create(1);
  bottomSwitchNodes1.Create(2);
  bottomSwitchNodes2.Create(1);

  BridgeHelper bridge;

  NetDeviceContainer topBridgeDevices1, topBridgeDevices2, bottomBridgeDevices1, bottomBridgeDevices2;
  NetDeviceContainer topSwitchDevices1, topSwitchDevices2, topSwitchDevices3;
  NetDeviceContainer bottomSwitchDevices1, bottomSwitchDevices2, bottomSwitchDevices3;

  topLanDevices1 = csmaHelper.Install (NodeContainer (topNodes.Get(0), topSwitchNodes1.Get(0)));
  topSwitchDevices1 = csmaHelper.Install (topSwitchNodes1);
  topBridgeDevices1.Add(topSwitchDevices1.Get(0));
  topBridgeDevices1.Add(topSwitchDevices1.Get(1));

  topLanDevices2 = csmaHelper.Install (NodeContainer (topNodes.Get(1), topSwitchNodes2.Get(0)));
  topSwitchDevices2 = csmaHelper.Install (topSwitchNodes2);
  topBridgeDevices2.Add(topSwitchDevices2.Get(0));

  topLanDevices3 = csmaHelper.Install (NodeContainer (topNodes.Get(2), topSwitchNodes2.Get(0)));
  topSwitchDevices3 = csmaHelper.Install (NodeContainer (topSwitchNodes2.Get(0), routers.Get(0)));
  topBridgeDevices2.Add(topSwitchDevices3.Get(0));

  bottomLanDevices1 = csmaHelper.Install (NodeContainer (bottomNodes.Get(0), bottomSwitchNodes1.Get(0)));
  bottomSwitchDevices1 = csmaHelper.Install (bottomSwitchNodes1);
  bottomBridgeDevices1.Add(bottomSwitchDevices1.Get(0));
  bottomBridgeDevices1.Add(bottomSwitchDevices1.Get(1));

  bottomLanDevices2 = csmaHelper.Install (NodeContainer (bottomNodes.Get(1), bottomSwitchNodes2.Get(0)));
  bottomSwitchDevices2 = csmaHelper.Install (bottomSwitchNodes2);
  bottomBridgeDevices2.Add(bottomSwitchDevices2.Get(0));

  bottomLanDevices3 = csmaHelper.Install (NodeContainer (bottomNodes.Get(2), bottomSwitchNodes2.Get(0)));
  bottomSwitchDevices3 = csmaHelper.Install (NodeContainer (bottomSwitchNodes2.Get(0), routers.Get(1)));
  bottomBridgeDevices2.Add(bottomSwitchDevices3.Get(0));

  bridge.Install(routers.Get(0), topBridgeDevices1);
  bridge.Install(routers.Get(0), topBridgeDevices2);

  bridge.Install(routers.Get(1), bottomBridgeDevices1);
  bridge.Install(routers.Get(1), bottomBridgeDevices2);

  wanDevices = p2pHelper.Install (routers);

  NS_LOG_INFO ("Install Internet stack.");
  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer topInterfaces1 = address.Assign (topLanDevices1);
  address.Assign (topSwitchDevices1);
  Ipv4InterfaceContainer topInterfaces2 = address.Assign (topLanDevices2);
  address.Assign (topSwitchDevices2);
  Ipv4InterfaceContainer topInterfaces3 = address.Assign (topLanDevices3);
  address.Assign (topSwitchDevices3);
  Ipv4InterfaceContainer topRouterInterface = address.Assign (NetDeviceContainer (wanDevices.Get(0)));
  address.NewNetwork ();

  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottomInterfaces1 = address.Assign (bottomLanDevices1);
  address.Assign (bottomSwitchDevices1);
  Ipv4InterfaceContainer bottomInterfaces2 = address.Assign (bottomLanDevices2);
  address.Assign (bottomSwitchDevices2);
  Ipv4InterfaceContainer bottomInterfaces3 = address.Assign (bottomLanDevices3);
  address.Assign (bottomSwitchDevices3);
  Ipv4InterfaceContainer bottomRouterInterface = address.Assign (NetDeviceContainer (wanDevices.Get(1)));
  address.NewNetwork ();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps;
  serverApps.Add (echoServer.Install (topNodes.Get (2)));
  serverApps.Add (echoServer.Install (bottomNodes.Get (1)));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (topInterfaces3.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps;
  clientApps.Add (echoClient.Install (topNodes.Get (1)));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (bottomInterfaces3.GetAddress (1), echoPort);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2;
  clientApps2.Add (echoClient2.Install (bottomNodes.Get (2)));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  NS_LOG_INFO ("Enable Tracing.");
  if (enablePcap)
    {
      csmaHelper.EnablePcapAll ("multi-switch-lan");
      p2pHelper.EnablePcapAll ("multi-switch-lan");
    }

  AnimationInterface anim ("multi-switch-lan.xml");
  anim.SetConstantPosition(topNodes.Get(0), 10, 10);
  anim.SetConstantPosition(topNodes.Get(1), 10, 30);
  anim.SetConstantPosition(topNodes.Get(2), 10, 50);
  anim.SetConstantPosition(bottomNodes.Get(0), 50, 10);
  anim.SetConstantPosition(bottomNodes.Get(1), 50, 30);
  anim.SetConstantPosition(bottomNodes.Get(2), 50, 50);
  anim.SetConstantPosition(routers.Get(0), 30, 20);
  anim.SetConstantPosition(routers.Get(1), 30, 40);
  anim.SetConstantPosition(topSwitchNodes1.Get(0), 15, 15);
  anim.SetConstantPosition(topSwitchNodes1.Get(1), 15, 20);
  anim.SetConstantPosition(topSwitchNodes2.Get(0), 15, 45);
  anim.SetConstantPosition(bottomSwitchNodes1.Get(0), 55, 15);
  anim.SetConstantPosition(bottomSwitchNodes1.Get(1), 55, 20);
  anim.SetConstantPosition(bottomSwitchNodes2.Get(0), 55, 45);

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}