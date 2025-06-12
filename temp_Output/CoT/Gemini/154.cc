#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/csv-helper.h"
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSimulation");

std::ofstream outputFile;

void
ReceivePacket (std::string context, Ptr<const Packet> p, const Address &addr)
{
  Time receiveTime = Simulator::Now ();
  Ipv4Address senderIp = Ipv4Address::GetZero ();
  uint16_t senderPort = 0;

  if (context.find("UdpClient") != std::string::npos)
    {
        return;
    }

  std::string nodeStr = context.substr(context.find("/$ns3::NodeList/")+17);
  int nodeId = std::stoi(nodeStr.substr(0, nodeStr.find("/")));
  
  UdpHeader udpHeader;
  p->PeekHeader(udpHeader);
  senderPort = udpHeader.GetSourcePort();
  senderIp = Ipv4Address("10.1.1." + std::to_string(senderPort-9));
  
  
  
  outputFile << senderIp << ","
               << Ipv4Address("10.1.1.5") << ","
               << p->GetSize () << ","
               << Simulator::GetLastEventStart () << ","
               << receiveTime << std::endl;
}

int
main (int argc, char *argv[])
{
  bool tracing = false;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  outputFile.open("packet_data.csv");
  outputFile << "Source IP,Destination IP,Packet Size,Transmission Time,Reception Time" << std::endl;

  NodeContainer nodes;
  nodes.Create (5);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (0));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (1)));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (2)));
  staDevices.Add(wifi.Install (phy, mac, nodes.Get (3)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (4));

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer::Create (staDevices, apDevices));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient1 (interfaces.GetAddress (4), 10);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient2 (interfaces.GetAddress (4), 11);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient3 (interfaces.GetAddress (4), 12);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient4 (interfaces.GetAddress (4), 13);
  echoClient4.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient4.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient4.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (simulationTime));

  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (simulationTime));

  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (2.0));
  clientApps3.Stop (Seconds (simulationTime));

  ApplicationContainer clientApps4 = echoClient4.Install (nodes.Get (3));
  clientApps4.Start (Seconds (2.0));
  clientApps4.Stop (Seconds (simulationTime));

  for (int i = 0; i < 5; ++i)
    {
      std::string context = "/NodeList/" + std::to_string(i) + "/$ns3::UdpSocketImpl/Rx";
      Config::Connect (context, MakeCallback (&ReceivePacket));
    }

  Simulator::Stop (Seconds (simulationTime));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      phy.EnableAsciiAll (ascii.CreateFileStream ("wireless-simulation.tr"));
      phy.EnablePcapAll ("wireless-simulation");
    }
  AnimationInterface anim("wireless-animation.xml");

  Simulator::Run ();
  Simulator::Destroy ();
  outputFile.close();

  return 0;
}