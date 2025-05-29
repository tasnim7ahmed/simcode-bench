#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class ServerApp : public Application
{
public:
  ServerApp () {}
  virtual ~ServerApp () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->Listen ();
    m_socket->SetAcceptCallback (
      MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
      MakeCallback (&ServerApp::HandleAccept, this));
    m_socket->SetRecvCallback (MakeCallback (&ServerApp::HandleRead, this));
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void HandleAccept (Ptr<Socket> s, const Address& from)
  {
    s->SetRecvCallback (MakeCallback (&ServerApp::HandleRead, this));
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
      {
        NS_LOG_UNCOND ("Server received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

class ClientApp : public Application
{
public:
  ClientApp () {}
  virtual ~ClientApp () {}

  void Setup (Ipv4Address serverAddr, uint16_t port, uint32_t sendSize)
  {
    m_serverAddr = serverAddr;
    m_port = port;
    m_sendSize = sendSize;
  }

private:
  virtual void StartApplication (void)
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_serverAddr, m_port));
    Simulator::ScheduleWithContext (m_socket->GetNode ()->GetId (), Seconds (0.1), &ClientApp::SendPacket, this);
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }

  void SendPacket ()
  {
    Ptr<Packet> packet = Create<Packet> (m_sendSize);
    m_socket->Send (packet);
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_serverAddr;
  uint16_t m_port;
  uint32_t m_sendSize;
};

int main (int argc, char *argv[])
{
  uint32_t nClients = 4;
  uint16_t serverPort = 8080;
  uint32_t packetSize = 64;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nClients);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  Ptr<ServerApp> serverApp = CreateObject<ServerApp> ();
  serverApp->Setup (serverPort);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (5.0));

  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<ClientApp> clientApp = CreateObject<ClientApp> ();
      clientApp->Setup (apInterface.GetAddress (0), serverPort, packetSize);
      wifiStaNodes.Get (i)->AddApplication (clientApp);
      clientApp->SetStartTime (Seconds (1.0 + 0.1 * i));
      clientApp->SetStopTime (Seconds (2.0));
    }

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}