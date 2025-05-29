#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2vWaveUdpExample");

uint32_t g_packetsTx = 0;
uint32_t g_packetsRx = 0;

void TxTrace (Ptr<const Packet> packet)
{
  g_packetsTx++;
}

void RxTrace (Ptr<const Packet> packet, const Address &addr)
{
  g_packetsRx++;
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("V2vWaveUdpExample", LOG_LEVEL_INFO);

  NodeContainer vehicles;
  vehicles.Create (2);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();

  NetDeviceContainer devices = wifi80211p.Install (phy, mac, vehicles);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0, 0.0, 0.0));
  pos->Add (Vector (10.0, 0.0, 0.0));
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (vehicles);

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 4000;
  uint32_t packetSize = 512;
  uint32_t numPackets = 20;
  double interval = 0.5;

  // UDP Server on vehicle 2
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (vehicles.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (20.0));

  // UDP Client on vehicle 1
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (vehicles.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (20.0));

  // Trace packet transmissions
  devices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&TxTrace));
  devices.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (
    [](Ptr<const Packet> p) { /* Optional: Increment packets received at PHY */ }));

  // Trace packet receptions at app layer
  serverApp.Get (0)->GetObject<UdpServer> ()->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  std::cout << "Packets sent: " << g_packetsTx << std::endl;
  std::cout << "Packets received: " << g_packetsRx << std::endl;

  Simulator::Destroy ();
  return 0;
}