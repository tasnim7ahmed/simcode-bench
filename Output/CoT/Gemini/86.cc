#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSenderReceiver");

class WifiSender : public Application
{
public:
  WifiSender ();
  virtual ~WifiSender ();

  void Setup (Address address, uint32_t packetSize, DataRate dataRate, Time interPacketInterval, uint32_t maxPackets);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void PacketReception (Ptr<Socket> socket);

  Address m_peer;
  uint32_t m_packetSize;
  DataRate m_dataRate;
  Time m_interPacketInterval;
  uint32_t m_packetsSent;
  uint32_t m_maxPackets;
  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  bool m_running;
};

WifiSender::WifiSender ()
  : m_peer (),
    m_packetSize (0),
    m_dataRate (0),
    m_interPacketInterval (0),
    m_packetsSent (0),
    m_maxPackets (0),
    m_socket (nullptr),
    m_sendEvent (),
    m_running (false)
{
}

WifiSender::~WifiSender ()
{
  m_socket = nullptr;
}

void
WifiSender::Setup (Address address, uint32_t packetSize, DataRate dataRate,
                           Time interPacketInterval, uint32_t maxPackets)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
  m_interPacketInterval = interPacketInterval;
  m_maxPackets = maxPackets;
}

void
WifiSender::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  m_socket->SetRecvCallback (MakeCallback (&WifiSender::PacketReception, this));

  SendPacket ();
}

void
WifiSender::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
WifiSender::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_maxPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interPacketInterval, &WifiSender::SendPacket, this);
    }
}

void
WifiSender::PacketReception (Ptr<Socket> socket)
{

}

class WifiReceiver : public Application
{
public:
  WifiReceiver ();
  virtual ~WifiReceiver();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ReceivePacket (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  std::ofstream m_outputFile;
};

WifiReceiver::WifiReceiver ()
  : m_port (0),
    m_socket (nullptr)
{
}

WifiReceiver::~WifiReceiver()
{
  m_socket = nullptr;
  m_outputFile.close();
}

void
WifiReceiver::Setup (uint16_t port)
{
  m_port = port;
}

void
WifiReceiver::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TypeId::LookupByName ("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&WifiReceiver::ReceivePacket, this));

  m_outputFile.open ("receiver_log.txt");
}

void
WifiReceiver::StopApplication (void)
{
  m_socket->Close ();
}

void
WifiReceiver::ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Time arrivalTime = Simulator::Now ();

  if (InetSocketAddress::IsMatchingType (from)) {
        InetSocketAddress iaddr = InetSocketAddress::ConvertFrom (from);
        Ipv4Address senderAddr = iaddr.GetIpv4 ();
        uint16_t senderPort = iaddr.GetPort ();

        m_outputFile << "Received one packet from " << senderAddr << " port " << senderPort
                     << " at " << arrivalTime.GetSeconds() << "s" << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 2;
  double simulationTime = 10;
  std::string dataRate = "5Mbps";
  uint32_t packetSize = 1024;
  uint16_t port = 9;
  Time interPacketInterval = Seconds (0.01);
  uint32_t maxPackets = 1000;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if possible", verbose);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dataRate", "Data rate for Wi-Fi network", dataRate);
  cmd.AddValue ("packetSize", "Size of each packet", packetSize);
  cmd.AddValue ("port", "Port number", port);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("maxPackets", "Maximum packets to send", maxPackets);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiSenderReceiver", LOG_LEVEL_INFO);
    }

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<WifiSender> appSource = CreateObject<WifiSender> ();
  appSource->Setup (InetSocketAddress (staInterfaces.GetAddress (1), port), packetSize, DataRate (dataRate), interPacketInterval, maxPackets);
  wifiStaNodes.Get (0)->AddApplication (appSource);
  appSource->SetStartTime (Seconds (1.0));
  appSource->SetStopTime (Seconds (simulationTime));

  Ptr<WifiReceiver> appSink = CreateObject<WifiReceiver> ();
  appSink->Setup (port);
  wifiStaNodes.Get (1)->AddApplication (appSink);
  appSink->SetStartTime (Seconds (0.0));
  appSink->SetStopTime (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}