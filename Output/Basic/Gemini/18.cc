#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void
ApMove (Ptr<Node> apNode, double x)
{
  Ptr<MobilityModel> mobility = apNode->GetObject<MobilityModel> ();
  Vector pos = mobility->GetPosition ();
  pos.x = x;
  mobility->SetPosition (pos);
  Simulator::Schedule (Seconds (1.0), &ApMove, apNode, x + 5.0);
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer staNodes;
  staNodes.Create (2);

  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");

  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  Simulator::Schedule (Seconds (0.0), &ApMove, apNode.Get (0), 0.0);

  InternetStackHelper internet;
  internet.Install (staNodes);
  internet.Install (apNode);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (NetDeviceContainer::Create (staDevices, apDevices));

  PacketSocketHelper packetSocketHelper;
  packetSocketHelper.Install (staNodes);
  packetSocketHelper.Install (apNode);

  Ptr<Node> sender = staNodes.Get (0);
  Ptr<Node> receiver = staNodes.Get (1);

  TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (receiver, tid);
  InetSocketAddress local = InetSocketAddress (i.GetAddress (3), 8080);
  sink->Bind (local);
  sink->SetRecvCallback ([](Ptr<Socket> socket) {
      Address from;
      Ptr<Packet> packet = socket->RecvFrom (from);
      std::cout << "Received packet!" << std::endl;
  });

  Simulator::Schedule (Seconds (0.5), [&]() {
      Ptr<Socket> source = Socket::CreateSocket (sender, tid);
      InetSocketAddress remote = InetSocketAddress (i.GetAddress (3), 8080);
      source->Connect (remote);

      Simulator::Schedule (Seconds (0.0), [&, source]() {
          Ptr<Packet> packet = Create<Packet> (1024);
          source->Send (packet);
      });
  });

  wifiPhy.EnableMacTraces ();
  wifiPhy.EnablePhyTraces ();

  Simulator::Stop (Seconds (44.0));

  AnimationInterface anim ("wifi-animation.xml");
  anim.SetConstantPosition (apNode.Get (0), 50, 50);
  anim.SetConstantPosition (staNodes.Get (0), 0, 0);
  anim.SetConstantPosition (staNodes.Get (1), 0, 100);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}