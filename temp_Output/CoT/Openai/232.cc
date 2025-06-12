#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiMultiClientServerExample");

class TcpServerApp : public Application
{
public:
  TcpServerApp () {}
  virtual ~TcpServerApp () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication (void)
  {
    Address anyAddress (InetSocketAddress (Ipv4Address::GetAny (), m_port));
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    m_socket->Bind (anyAddress);
    m_socket->Listen ();
    m_socket->SetAcceptCallback (
      MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
      MakeCallback (&TcpServerApp::HandleAccept, this));
    m_socket->SetRecvCallback (
      MakeCallback (&TcpServerApp::HandleRead, this));
  }

  virtual void StopApplication (void)
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void HandleAccept (Ptr<Socket> s, const Address& from)
  {
    s->SetRecvCallback (MakeCallback (&TcpServerApp::HandleRead, this));
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom (from))
      {
        if (packet->GetSize () > 0)
          {
            NS_LOG_UNCOND ("Server received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4 ());
          }
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  uint32_t nClients = 4;
  double simulationTime = 5.0;
  uint16_t port = 5000;
  uint32_t dataBytes = 1024;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // WiFi Configuration (use default basic settings)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility (minimal)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (2.0),
                                "DeltaY", DoubleValue (2.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // Server application on AP node
  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp> ();
  serverApp->Setup (port);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simulationTime));

  // Client applications
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> sock = Socket::CreateSocket (wifiStaNodes.Get (i), TcpSocketFactory::GetTypeId ());
      Ptr<Node> clientNode = wifiStaNodes.Get(i);

      // Application to connect and send data
      Ptr<Application> clientApp = CreateObject<Application> ();
      clientNode->AddApplication (clientApp);

      Address serverAddr = InetSocketAddress (apInterface.GetAddress(0), port);
      clientApp->SetStartTime (Seconds (1.0 + 0.1 * i));
      clientApp->SetStopTime (Seconds (simulationTime));

      // Custom send event for each client
      Simulator::ScheduleWithContext (
          clientNode->GetId (),
          Seconds (1.1 + 0.1 * i),
          [sock, serverAddr, dataBytes]() {
            sock->Connect (serverAddr);
            Ptr<Packet> packet = Create<Packet> (dataBytes);
            sock->Send (packet);
            sock->Close ();
          }
        );
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}