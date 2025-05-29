#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/applications-module.h"

using namespace ns3;

void MoveAp (Ptr<Node> apNode)
{
  static double x = 0.0;
  x += 5.0;
  Ptr<MobilityModel> mob = apNode->GetObject<MobilityModel> ();
  Vector pos = mob->GetPosition ();
  pos.x = x;
  mob->SetPosition (pos);
  Simulator::Schedule (Seconds(1.0), &MoveAp, apNode);
}

void TxTrace (std::string context, Ptr<const Packet> p)
{
  std::cout << Simulator::Now ().GetSeconds () << " TX " << context << " " << p->GetSize () << std::endl;
}

void RxTrace (std::string context, Ptr<const Packet> p)
{
  std::cout << Simulator::Now ().GetSeconds () << " RX " << context << " " << p->GetSize () << std::endl;
}

void PhyRxOkTrace (std::string context, Ptr<const Packet> p, double snr, WifiMode mode, enum WifiPreamble preamble)
{
  std::cout << Simulator::Now ().GetSeconds () << " PHY-RX-OK " << context << " " << snr << std::endl;
}

void PhyRxErrorTrace (std::string context, Ptr<const Packet> p, double snr)
{
  std::cout << Simulator::Now ().GetSeconds () << " PHY-RX-ERROR " << context << " " << snr << std::endl;
}

void StateTrace (std::string context, Time start, Time duration, WifiPhyState state)
{
  std::cout << Simulator::Now ().GetSeconds () << " PHY-STATE " << context << " " << state << std::endl;
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));
  positionAlloc->Add (Vector (0.0, 10.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Schedule AP movement
  Simulator::Schedule (Seconds (1.0), &MoveAp, wifiApNode.Get (0));

  PacketSocketHelper packetSocket;
  packetSocket.Install (wifiStaNodes);
  packetSocket.Install (wifiApNode);

  // Tracing
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback (&TxTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&RxTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback (&PhyRxOkTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback (&PhyRxErrorTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&StateTrace));

  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (staDevices.Get (0)->GetIfIndex ());
  socketAddr.SetPhysicalAddress (apDevice.Get (0)->GetAddress ());
  socketAddr.SetProtocol (0);

  uint32_t payloadSize = 512;
  OnOffHelper onoff ("ns3::PacketSocketFactory", Address (socketAddr));
  onoff.SetAttribute ("DataRate", DataRateValue(DataRate ("500kbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (0.5)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (44.0)));

  ApplicationContainer app = onoff.Install (wifiStaNodes.Get (0));

  Simulator::Stop (Seconds (44.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}