#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

static void
CourseChange (std::string context, Ptr<const MobilityModel> mobility)
{
}

static void
ApMove (Ptr<Node> ap)
{
  Vector position = ap->GetObject<MobilityModel>()->GetPosition ();
  position.x += 5;
  ap->GetObject<MobilityModel>()->SetPosition (position);
  Simulator::Schedule (Seconds (1.0), &ApMove, ap);
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

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

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0 50 0 50"));
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  Ptr<Node> ap = apNode.Get (0);
  Simulator::Schedule (Seconds (1.0), &ApMove, ap);

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

  TypeId tid = TypeId::LookupByName ("ns3::PacketSocket");
  Ptr<Socket> sink = Socket::CreateSocket (receiver, tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 8080);
  sink->Bind (local);
  sink->SetRecvCallback ([](Ptr<Socket> socket) {
    Address from;
    auto packet = socket->RecvFrom (from);
  });

  Ptr<Socket> source = Socket::CreateSocket (sender, tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (3), 8080);
  source->Connect (remote);

  Simulator::ScheduleWithContext (sender->GetId (), Seconds (0.5), [source, sender]() {
    for (int i = 0; i < 100; ++i) {
      Ptr<Packet> packet = Create<Packet> (1024);
      source->Send (packet);
    }
  });

  Config::Set ("/NodeList/*/$ns3::WifiNetDevice/Mac/MacRxDropThreshold", UintegerValue (0));

  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyTxEnd", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Phy/PhyState/State", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/$ns3::WifiNetDevice/Mac/MacRxDrop", MakeCallback (&CourseChange));

  Simulator::Stop (Seconds (44.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}