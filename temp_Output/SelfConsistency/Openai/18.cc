#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiStaApPacketSocketExample");

// Trace Callbacks
void MacTxTrace (std::string context, Ptr<const Packet> packet)
{
  std::cout << Simulator::Now ().GetSeconds () << " [" << context << "] MAC TX: " << packet->GetSize () << " bytes" << std::endl;
}

void MacRxTrace (std::string context, Ptr<const Packet> packet)
{
  std::cout << Simulator::Now ().GetSeconds () << " [" << context << "] MAC RX: " << packet->GetSize () << " bytes" << std::endl;
}

void PhyRxOkTrace (std::string context, Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble)
{
  std::cout << Simulator::Now ().GetSeconds () << " [" << context << "] PHY RX OK: " << packet->GetSize () << " bytes, SNR=" << snr << std::endl;
}

void PhyRxErrorTrace (std::string context, Ptr<const Packet> packet, double snr)
{
  std::cout << Simulator::Now ().GetSeconds () << " [" << context << "] PHY RX ERROR: " << packet->GetSize () << " bytes, SNR=" << snr << std::endl;
}

void StateTransitionTrace (std::string context, Time start, Time duration, WifiPhyState state)
{
  std::cout << Simulator::Now ().GetSeconds () << " [" << context << "] PHY STATE TRANSITION: " << state << " (" << start.GetSeconds () << ", " << duration.GetSeconds () << ")" << std::endl;
}

// Helper function to move AP along x-axis by +5 meters every second
void MoveApPosition (Ptr<MobilityModel> mobility, uint32_t times)
{
  Vector pos = mobility->GetPosition();
  pos.x += 5.0;
  mobility->SetPosition(pos);
  std::cout << Simulator::Now ().GetSeconds ()
            << " [AP Mobility] Moved AP to x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z << std::endl;
  if (times > 1)
    {
      Simulator::Schedule (Seconds (1.0), &MoveApPosition, mobility, times - 1);
    }
}

// Application to send packets between the STAs
void SendPacket (Ptr<Socket> socket, Address dst, uint32_t pktSize, uint32_t count, Time interval)
{
  if (count > 0)
    {
      socket->SendTo (Create<Packet> (pktSize), 0, dst);
      Simulator::Schedule (interval, &SendPacket, socket, dst, pktSize, count - 1, interval);
    }
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("WifiStaApPacketSocketExample", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (2);
  wifiApNode.Create (1);

  // Wifi PHY and channel setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate1Mbps"),
                               "ControlMode", StringValue ("DsssRate1Mbps"));

  // MAC configuration for stations
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // MAC configuration for AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  // Set station positions
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // AP at origin (0,5,0)
  Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
  apAlloc->Add (Vector (0.0, 5.0, 0.0));
  mobility.SetPositionAllocator (apAlloc);
  mobility.Install (wifiApNode);

  // Tracing
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&MacTxTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&MacRxTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeCallback(&PhyRxOkTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxError", MakeCallback(&PhyRxErrorTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback(&StateTransitionTrace));

  // Install Packet Socket on nodes
  PacketSocketHelper packetSocket;
  packetSocket.Install(wifiStaNodes);
  packetSocket.Install(wifiApNode);

  // PacketSocket address setup
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice (staDevices.Get(0)->GetIfIndex());
  socketAddr.SetPhysicalAddress (staDevices.Get(1)->GetAddress());
  socketAddr.SetProtocol (0);

  Ptr<Socket> sta0Socket = Socket::CreateSocket (wifiStaNodes.Get(0), PacketSocketFactory::GetTypeId ());
  sta0Socket->Bind ();

  // Reverse direction (sta1 to sta0)
  PacketSocketAddress socketAddrReverse;
  socketAddrReverse.SetSingleDevice (staDevices.Get(1)->GetIfIndex());
  socketAddrReverse.SetPhysicalAddress (staDevices.Get(0)->GetAddress());
  socketAddrReverse.SetProtocol (0);

  Ptr<Socket> sta1Socket = Socket::CreateSocket (wifiStaNodes.Get(1), PacketSocketFactory::GetTypeId ());
  sta1Socket->Bind ();

  // Schedule AP movement (move for 43 seconds, once per second)
  Ptr<MobilityModel> apMobility = wifiApNode.Get (0)->GetObject<MobilityModel> ();
  Simulator::Schedule (Seconds (1.0), &MoveApPosition, apMobility, 43);

  // Simulate 500 kbps communication between stations
  uint32_t pktSize = 125; // 1kb = 125 bytes; 500kbps = 500,000 bits/sec = 62,500 bytes/sec; 62,500/125 = 500 pkt/sec
  double dataRateBps = 500000.0;
  double intervalSec = (double)pktSize * 8 / dataRateBps; // seconds between packets

  // Send 500 packets per second for each direction, for 43.5 seconds
  uint32_t nPackets = static_cast<uint32_t> (43.5 / intervalSec);

  // Schedule start after 0.5s
  Simulator::Schedule(Seconds (0.5), &SendPacket, sta0Socket, socketAddr, pktSize, nPackets, Seconds (intervalSec));
  Simulator::Schedule(Seconds (0.5), &SendPacket, sta1Socket, socketAddrReverse, pktSize, nPackets, Seconds (intervalSec));

  Simulator::Stop (Seconds (44.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}