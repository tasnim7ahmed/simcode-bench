#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/ocb-wifi-phy.h"
#include "ns3/wave-helper.h"
#include "ns3/uan-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VANETSimulation");

static void
ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet!");
    }
}

static void
InstallApplication (Ptr<Node> node, Address localAddress)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (localAddress, 8080);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFirstLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (5);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Wifi configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211p);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
                                  "SystemLoss", DoubleValue(1),
                                  "Frequency", DoubleValue(5.9e9));
  wifiPhy.SetChannel (wifiChannel.Create ());

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifiMac, nodes);

  // Setup IPv4 addresses
  Ipv4ListRoutingHelper ipv4RoutingHelper;
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  // Install Applications (UDP Echo)
  for (uint32_t j = 0; j < nodes.GetN (); ++j)
    {
      InstallApplication (nodes.Get (j), i.GetAddress (j));
    }

  // Create one udp echo client, connect to server 0
  TypeId tid = TypeId::LookupByName ("ns3::UdpClient");
  Ptr<Application> clientApp = CreateObject<Application> ("ns3::UdpClient");
  Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(clientApp);
  udpClient->SetAttribute ("RemoteAddress", AddressValue (InetSocketAddress (i.GetAddress (0), 8080)));
  udpClient->SetAttribute ("RemotePort", UintegerValue (8080));
  udpClient->SetAttribute ("PacketSize", UintegerValue (1024));
  udpClient->SetAttribute ("MaxPackets", UintegerValue (1000));
  udpClient->SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  udpClient->SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  nodes.Get (1)->AddApplication (clientApp);

  // Animation Interface
  AnimationInterface anim ("vanet-animation.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      anim.SetConstantPosition (nodes.Get (i), i * 10, 10);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}