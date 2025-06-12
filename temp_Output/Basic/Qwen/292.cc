#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GEpcUeMobilitySimulation");

class UdpClient : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpClient")
      .SetParent<Application> ()
      .AddConstructor<UdpClient> ()
      .AddAttribute ("RemoteAddress", "The destination Address of the outbound packets",
                     AddressValue (),
                     MakeAddressAccessor (&UdpClient::m_peerAddress),
                     MakeAddressChecker ())
      .AddAttribute ("RemotePort", "The destination port of the outbound packets",
                     UintegerValue (9),
                     MakeUintegerAccessor (&UdpClient::m_peerPort),
                     MakeUintegerChecker<uint16_t> ());
    return tid;
  }

  UdpClient ()
    : m_socket (0),
      m_sendInterval (MilliSeconds (500)),
      m_packetSize (512)
  {
  }

  virtual ~UdpClient ()
  {
    m_socket = nullptr;
  }

protected:
  void DoStart () override
  {
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress::ConvertFrom (m_peerAddress, m_peerPort));
    Simulator::ScheduleNow (&UdpClient::SendPacket, this);
  }

  void DoDispose () override
  {
    m_socket->Close ();
    Application::DoDispose ();
  }

private:
  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_packetSize);
    m_socket->Send (packet);

    NS_LOG_INFO ("Sent packet of size " << m_packetSize << " bytes at time " << Simulator::Now ().GetSeconds ());

    Simulator::Schedule (m_sendInterval, &UdpClient::SendPacket, this);
  }

  Address m_peerAddress;
  uint16_t m_peerPort;
  Ptr<Socket> m_socket;
  Time m_sendInterval;
  uint32_t m_packetSize;
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  LogComponentEnable ("Nr5GEpcUeMobilitySimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

  // Create EPC helper
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper> ();
  Config::SetDefault ("ns3::EpcEnbApplication::S1CDelay", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::EpcMmeApplication::S1CDelay", TimeValue (MilliSeconds (10)));

  // Create NR helper and set parameters
  NrPointToPointEpcHelper nrEpcHelper;
  nrEpcHelper.SetEpcHelper (epcHelper);

  // Install internet stack
  InternetStackHelper internet;
  NodeContainer nodes;
  nodes.Create (2); // gNB and UE

  internet.Install (nodes);

  // Setup NR devices
  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  enbDevs = nrEpcHelper.InstallEnbDevice (nodes.Get (0));
  ueDevs = nrEpcHelper.InstallUeDevice (nodes.Get (1));

  // Attach UE to network via EPC
  epcHelper->Attach (ueDevs);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));

  // Set default gateway for UE
  Ptr<Node> ueNode = nodes.Get (1);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4> ueIpv4 = ueNode->GetObject<Ipv4> ();
  uint32_t ueOif = ueIpv4->GetInterfaceForDevice (ueDevs.Get (0));
  Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueIpv4);
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), ueOif);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (nodes);
  nodes.Get (0)->GetObject<MobilityModel>()->SetPosition (Vector (0.0, 0.0, 0.0));
  nodes.Get (1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity (Vector (1.0, 0.0, 0.0)); // Moving along X-axis

  // Install UDP client on UE to send data to gNB
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Address sinkAddress = InetSocketAddress (Ipv4Address::ConvertFrom (ueIpIfaces.GetAddress (0)), 9);

  UdpClientHelper clientHelper (sinkAddress);
  clientHelper.SetAttribute ("RemotePort", UintegerValue (9));
  clientHelper.SetAttribute ("RemoteAddress", AddressValue (sinkAddress));
  clientHelper.SetAttribute ("SendInterval", TimeValue (MilliSeconds (500)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (1));
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  // Enable logging
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("nr-epc-ue-mobility.tr");
  nrEpcHelper.EnableTraces (stream);

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}