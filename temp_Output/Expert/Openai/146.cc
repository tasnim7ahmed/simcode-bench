#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshUdpExample");

class PacketStats : public Object
{
public:
  PacketStats () : m_txPackets (0), m_rxPackets (0) {}
  void TxCallback (Ptr<const Packet> packet)
  {
    m_txPackets++;
  }
  void RxCallback (Ptr<const Packet> packet, const Address & addr)
  {
    m_rxPackets++;
  }
  uint32_t GetTxPackets () const { return m_txPackets; }
  uint32_t GetRxPackets () const { return m_rxPackets; }
private:
  uint32_t m_txPackets;
  uint32_t m_rxPackets;
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer meshNodes;
  meshNodes.Create (4);

  // Mobility: 2x2 grid (2 meters spacing)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (2.0),
                                "DeltaY", DoubleValue (2.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);

  // WiFi Mesh Setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  MeshHelper mesh;
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (phy, meshNodes);

  // Internet stack
  InternetStackHelper internetStack;
  internetStack.Install (meshNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  // UDP server on node 3 (node index 3)
  uint16_t serverPort = 4000;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApps = udpServer.Install (meshNodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (15.0));

  // UDP client on node 0 targeting node 3
  uint32_t maxPackets = 1000;
  Time interPacketInterval = MilliSeconds (100);
  UdpClientHelper udpClient (interfaces.GetAddress (3), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (15.0));

  // Statistics collection
  Ptr<PacketStats> stats = CreateObject<PacketStats> ();

  meshDevices.Get (0)->TraceConnectWithoutContext ("MacTx", MakeCallback (&PacketStats::TxCallback, stats));
  meshDevices.Get (3)->TraceConnectWithoutContext ("MacRx", MakeCallback (&PacketStats::RxCallback, stats));

  // Enable PCAP for all devices
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcapAll ("mesh-udp");

  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();

  // Report statistics
  std::cout << "Packets sent from node 0: " << stats->GetTxPackets () << std::endl;
  std::cout << "Packets received at node 3: " << stats->GetRxPackets () << std::endl;

  Simulator::Destroy ();
  return 0;
}