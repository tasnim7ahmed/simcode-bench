#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("V2vWaveSimulation");

class PacketStats : public Object
{
public:
  PacketStats () : m_txPackets (0), m_rxPackets (0) {}

  void TxTracer (Ptr<const Packet> packet)
  {
    m_txPackets++;
  }

  void RxTracer (Ptr<const Packet> packet, const Address &address)
  {
    m_rxPackets++;
  }

  uint32_t GetTx () const { return m_txPackets; }
  uint32_t GetRx () const { return m_rxPackets; }

private:
  uint32_t m_txPackets;
  uint32_t m_rxPackets;
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer vehicles;
  vehicles.Create (2);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper waveHelper = Wifi80211pHelper::Default ();
  waveHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = waveHelper.Install (wifiPhy, waveMac, vehicles);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (vehicles);

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 5000;
  double simulationTime = 5.0;

  // UDP Server (receive) on vehicle 1
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (vehicles.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP Client (send) on vehicle 0
  uint32_t packetSize = 200;
  uint32_t packetCount = 10;
  Time interPacketInterval = Seconds (0.5);
  UdpClientHelper udpClient (interfaces.GetAddress (1), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (packetCount));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = udpClient.Install (vehicles.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime));

  Ptr<PacketStats> pktStats = CreateObject<PacketStats> ();

  devices.Get (0)->GetObject<NetDevice> ()->TraceConnectWithoutContext (
      "MacTx", MakeCallback (&PacketStats::TxTracer, pktStats));
  devices.Get (1)->GetObject<NetDevice> ()->TraceConnectWithoutContext (
      "MacRx", MakeCallback (&PacketStats::RxTracer, pktStats));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  uint32_t totalTx = pktStats->GetTx ();
  uint32_t totalRx = pktStats->GetRx ();
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApp.Get (0));

  std::cout << "Simulation completed." << std::endl;
  std::cout << "Packets sent (MacTx): " << totalTx << std::endl;
  std::cout << "Packets received (MacRx): " << totalRx << std::endl;
  std::cout << "UDP packets received by server: " << server->GetReceived () << std::endl;

  Simulator::Destroy ();
  return 0;
}