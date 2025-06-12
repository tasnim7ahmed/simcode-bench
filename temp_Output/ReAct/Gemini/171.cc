#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (10);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 500, 0, 500)));
  mobility.Install (nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpClientServerHelper cbr(9);
  cbr.SetAttribute ("MaxPackets", UintegerValue (0xffffffff));
  cbr.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  cbr.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t i = 0; i < 5; ++i)
  {
      uint32_t clientNodeId = i;
      uint32_t serverNodeId = (i + 5) % 10;

      serverApps.Add (cbr.Install (nodes.Get (serverNodeId)));
      Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable> ();
      startTimeSeconds->SetAttribute ("Min", DoubleValue (0.0));
      startTimeSeconds->SetAttribute ("Max", DoubleValue (5.0));
      Time startTime = Seconds (startTimeSeconds->GetValue ());

      clientApps.Add (cbr.Install (nodes.Get (clientNodeId)));
      Ptr<UdpClient> client = DynamicCast<UdpClient> (clientApps.Get (clientApps.GetN() - 1));
      client->SetRemote (interfaces.GetAddress (serverNodeId, 0), 9);
      client->SetStartTime(startTime);
  }

  serverApps.Start (Seconds (0.0));
  clientApps.Start (Seconds (5.0));
  Simulator::Stop (Seconds (30.0));

  phy.EnablePcapAll ("aodv-adhoc");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = 0;
  int numFlows = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationPort == 9)
        {
          totalPacketsSent += i->second.txPackets;
          totalPacketsReceived += i->second.rxPackets;
          totalDelay += i->second.delaySum.GetSeconds();
          numFlows++;
        }
    }

    double packetDeliveryRatio = 0;
    double averageEndToEndDelay = 0;

    if(totalPacketsSent > 0)
      packetDeliveryRatio = (totalPacketsReceived / totalPacketsSent) * 100.0;

    if(totalPacketsReceived > 0)
      averageEndToEndDelay = totalDelay / totalPacketsReceived;

    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << " s" << std::endl;

  Simulator::Destroy ();

  return 0;
}