#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetworkSimulation");

uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
uint32_t g_packetsLost = 0;
double g_totalDelay = 0.0;

void
RxPacket (Ptr<const Packet> packet, const Address &from, const Address &to)
{
  g_packetsReceived++;
}

void
PacketDelayTracer (std::string context, Ptr<const Packet> packet, const Address &address)
{
  Time now = Simulator::Now ();
  SocketAddressTag tag;
  if (packet->PeekPacketTag (tag))
    {
      Time sendTime = tag.GetTimeStamp ();
      g_totalDelay += (now - sendTime).GetSeconds ();
    }
}

void
TxPacket (Ptr<const Packet> packet)
{
  g_packetsSent++;
  SocketAddressTag tag;
  tag.SetTimeStamp(Simulator::Now());
  Ptr<Packet> copy = packet->Copy();
  copy->AddPacketTag(tag);
}

void
LostPacket (Ptr<const Packet> packet)
{
  g_packetsLost++;
}

class PeriodicUdpSender : public Application
{
public:
  PeriodicUdpSender () {}
  virtual ~PeriodicUdpSender () {}

  void SetPeerAddresses (std::vector<Address> peers)
  {
    m_peerAddresses = peers;
  }

  void SetPort (uint16_t port)
  {
    m_port = port;
  }

  void SetInterval (Time interval)
  {
    m_interval = interval;
  }

protected:
  virtual void StartApplication ()
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Bind ();
    ScheduleSend ();
  }
  virtual void StopApplication ()
  {
    if (m_sendEvent.IsRunning())
      Simulator::Cancel (m_sendEvent);
    m_socket = 0;
  }
  void ScheduleSend ()
  {
    m_sendEvent = Simulator::Schedule (m_interval, &PeriodicUdpSender::SendPacket, this);
  }
  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (100);
    for (auto &address : m_peerAddresses)
      {
        m_socket->SendTo (packet->Copy (), 0, address);
      }
    ScheduleSend ();
  }

private:
  Ptr<Socket> m_socket;
  std::vector<Address> m_peerAddresses;
  uint16_t m_port;
  EventId m_sendEvent;
  Time m_interval;
};

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 4;
  double simTime = 10.0;
  double velocity = 20.0; // m/s
  double interval = 0.2; // seconds
  uint16_t port = 4000;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("velocity", "Vehicle velocity (m/s)", velocity);
  cmd.Parse (argc, argv);

  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, vehicles);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (10.0),
                                "DeltaX", DoubleValue (30.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (numVehicles),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (vehicles);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (velocity, 0, 0));
    }

  InternetStackHelper internet;
  internet.Install (vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("vehicular-wifi.tr"));

  // Each vehicle sends to all others
  std::vector<Ptr<Application>> senders (numVehicles);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      std::vector<Address> peers;
      for (uint32_t j = 0; j < numVehicles; ++j)
        {
          if (i != j)
            {
              peers.push_back (InetSocketAddress (interfaces.GetAddress (j), port));
            }
        }
      Ptr<PeriodicUdpSender> app = CreateObject<PeriodicUdpSender> ();
      app->SetPeerAddresses (peers);
      app->SetPort (port);
      app->SetInterval (Seconds (interval));
      vehicles.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (0.5));
      app->SetStopTime (Seconds (simTime));
      senders[i] = app;
    }

  // Set up sinks on all vehicles
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      Ptr<Socket> sink = Socket::CreateSocket (vehicles.Get (i), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
      sink->Bind (local);
      sink->SetRecvCallback (MakeCallback (&RxPacket));
    }

  // Tracing
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PeriodicUdpSender/Tx", MakeCallback (&TxPacket));
  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback (&LostPacket));
  // Not natively available: use receive callback for delay calculation
  // We'll measure average delay based on application rx and current time

  // NetAnim
  AnimationInterface anim ("vehicular-network.xml");
  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      anim.UpdateNodeDescription (vehicles.Get (i), "Vehicle" + std::to_string(i));
      anim.UpdateNodeColor (vehicles.Get (i), 255, 100, 10);
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  double avgDelay = (g_packetsReceived > 0) ? (g_totalDelay / g_packetsReceived) : 0;
  std::cout << "Packets sent: " << g_packetsSent << std::endl;
  std::cout << "Packets received: " << g_packetsReceived << std::endl;
  std::cout << "Packets lost: " << g_packetsLost << std::endl;
  std::cout << "Packet loss rate: " << (g_packetsSent ? 100.0 * (g_packetsSent - g_packetsReceived) / g_packetsSent : 0) << " %" << std::endl;
  std::cout << "Average packet delay: " << avgDelay << " s" << std::endl;

  Simulator::Destroy ();
  return 0;
}