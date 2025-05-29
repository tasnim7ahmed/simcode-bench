#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/internet-module.h"
#include "ns3/packet.h"
#include "ns3/packet-socket-address.h"

using namespace ns3;

void
MoveAp (Ptr<Node> apNode)
{
  Ptr<MobilityModel> mob = apNode->GetObject<MobilityModel> ();
  Vector pos = mob->GetPosition ();
  pos.x += 5.0;
  mob->SetPosition (pos);
  Simulator::Schedule (Seconds (1.0), &MoveAp, apNode);
}

void
PhyRxOkTrace (std::string context, Ptr<const Packet> packet)
{
}

void
PhyRxErrorTrace (std::string context, Ptr<const Packet> packet)
{
}

void
PhyStateTrace (std::string context, Time start, Time duration, WifiPhyState state)
{
}

void
MacTxTrace (std::string context, Ptr<const Packet> packet)
{
}

void
MacRxTrace (std::string context, Ptr<const Packet> packet)
{
}

int
main (int argc, char *argv[])
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
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  // Set station positions
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // Set AP position
  MobilityHelper apMobility;
  Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator> ();
  apPositionAlloc->Add (Vector (10.0, 2.0, 0.0));
  apMobility.SetPositionAllocator (apPositionAlloc);
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.Install (wifiApNode);

  PacketSocketHelper packetSocket;
  packetSocket.Install (wifiStaNodes);
  packetSocket.Install (wifiApNode);

  // Tracing
  phy.EnablePcapAll ("wifi-pktsocket");
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeCallback (&PhyRxOkTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxError", MakeCallback (&PhyRxErrorTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback (&PhyStateTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback (&MacTxTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback (&MacRxTrace));

  // Set up PacketSocketAddresses for STA 0 and STA 1
  PacketSocketAddress sta0Addr;
  sta0Addr.SetSingleDevice (staDevices.Get (1)->GetIfIndex ());
  sta0Addr.SetPhysicalAddress (staDevices.Get (0)->GetAddress ());
  sta0Addr.SetProtocol (0);

  PacketSocketAddress sta1Addr;
  sta1Addr.SetSingleDevice (staDevices.Get (0)->GetIfIndex ());
  sta1Addr.SetPhysicalAddress (staDevices.Get (1)->GetAddress ());
  sta1Addr.SetProtocol (0);

  Ptr<Socket> sta0Socket = Socket::CreateSocket (wifiStaNodes.Get (0), PacketSocketFactory::GetTypeId ());
  sta0Socket->Bind ();
  sta0Socket->Connect (sta0Addr);

  Ptr<Socket> sta1Socket = Socket::CreateSocket (wifiStaNodes.Get (1), PacketSocketFactory::GetTypeId ());
  sta1Socket->Bind ();
  sta1Socket->Connect (sta1Addr);

  // Traffic generation (500kbps)
  uint32_t pktSize = 1000;
  double dataRate_bps = 500000;
  double interval = (double)pktSize * 8 / dataRate_bps; // sec

  void
  (*SendPkt0)(Ptr<Socket>, uint32_t) = [](Ptr<Socket> sock, uint32_t pktSize) {
    Ptr<Packet> pkt = Create<Packet> (pktSize);
    sock->Send (pkt);
    Simulator::Schedule (Seconds (interval), SendPkt0, sock, pktSize);
  };

  void
  (*SendPkt1)(Ptr<Socket>, uint32_t) = [](Ptr<Socket> sock, uint32_t pktSize) {
    Ptr<Packet> pkt = Create<Packet> (pktSize);
    sock->Send (pkt);
    Simulator::Schedule (Seconds (interval), SendPkt1, sock, pktSize);
  };

  Simulator::Schedule (Seconds (0.5), SendPkt0, sta0Socket, pktSize);
  Simulator::Schedule (Seconds (0.5), SendPkt1, sta1Socket, pktSize);

  Simulator::Schedule (Seconds (1.0), &MoveAp, wifiApNode.Get (0));

  Simulator::Stop (Seconds (44.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}