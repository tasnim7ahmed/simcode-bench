#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

static void
CourseChange (std::string context, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  NS_LOG_UNCOND (context << " x = " << position.x << ", y = " << position.y);
}

static void
MoveAp (Ptr<Node> ap, double distance, double time)
{
  Vector pos = ap->GetObject<MobilityModel>()->GetPosition();
  pos.x += distance;
  ap->GetObject<MobilityModel>()->SetPosition(pos);

  Simulator::Schedule (Seconds (time), &MoveAp, ap, distance, time);
}

int
main (int argc, char *argv[])
{
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
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

  NetDeviceContainer staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifi.Install (wifiPhy, wifiMac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  PacketSocketHelper packetSocketHelper;
  packetSocketHelper.Install (staNodes);
  packetSocketHelper.Install (apNode);

  Ptr<Node> ap = apNode.Get (0);
  MoveAp(ap, 5.0, 1.0);

  // Tracing
  if (tracing)
    {
      wifiPhy.EnablePcapAll ("wifi-example");
    }

  // Setup tracing for PHY and MAC
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback (&CourseChange));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&CourseChange));

  Simulator::Stop (Seconds (44.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}