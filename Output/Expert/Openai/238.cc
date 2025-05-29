#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerLogger : public Application
{
public:
  UdpServerLogger () {}
  virtual ~UdpServerLogger () {}

protected:
  virtual void StartApplication ()
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpServerLogger::HandleRead, this));
  }

  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
      {
        NS_LOG_UNCOND ("Received " << packet->GetSize () << " bytes at " << Simulator::Now ().GetSeconds () << "s from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
        m_totalRx += packet->GetSize ();
      }
  }

public:
  void SetPort (uint16_t port) { m_port = port; }
  uint32_t GetTotalRx () const { return m_totalRx; }

private:
  Ptr<Socket> m_socket;
  uint16_t m_port = 9;
  uint32_t m_totalRx = 0;
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpServerLogger", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));          // Static node
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));          // Will be set by mobility model, but needed for init
  mobility.SetPositionAllocator (positionAlloc);

  // Static node
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (0));
  // Mobile node
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes.Get (1));
  Ptr<ConstantVelocityMobilityModel> mob = nodes.Get (1)->GetObject<ConstantVelocityMobilityModel> ();
  mob->SetPosition (Vector (0.0, 5.0, 0.0));
  mob->SetVelocity (Vector (5.0, 0.0, 0.0)); // Move in +x at 5 m/s

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (meshDevices);

  uint16_t udpPort = 4000;
  Ptr<UdpServerLogger> serverApp = CreateObject<UdpServerLogger> ();
  serverApp->SetPort (udpPort);
  nodes.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (1.0));
  serverApp->SetStopTime (Seconds (20.0));

  uint32_t packetSize = 512;
  double interPacketInterval = 0.1; // seconds
  uint32_t numPackets = 100;
  UdpClientHelper client (interfaces.GetAddress (0), udpPort);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (1));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (20.0));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}