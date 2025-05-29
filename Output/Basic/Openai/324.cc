#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHttpSimulation");

class SimpleHttpServer : public Application
{
public:
  SimpleHttpServer () {}
  virtual ~SimpleHttpServer () {}

  void Setup (uint16_t port)
  {
    m_port = port;
  }

private:
  virtual void StartApplication () override
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
        m_socket->Bind (local);
        m_socket->Listen ();
        m_socket->SetAcceptCallback (
          MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
          MakeCallback (&SimpleHttpServer::HandleAccept, this));
      }
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void HandleAccept (Ptr<Socket> s, const Address &from)
  {
    s->SetRecvCallback (MakeCallback (&SimpleHttpServer::HandleRead, this));
  }

  void HandleRead (Ptr<Socket> s)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = s->RecvFrom (from)))
      {
        // Simulate fixed HTTP response (short text)
        std::string httpResponse = 
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html\r\n"
          "Content-Length: 20\r\n"
          "\r\n"
          "<html>Hello</html>\n";
        Ptr<Packet> responsePacket = Create<Packet> ((uint8_t*) httpResponse.c_str (), httpResponse.size ());
        s->Send (responsePacket);
        s->Close ();
        break;
      }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

class SimpleHttpClient : public Application
{
public:
  SimpleHttpClient () {}
  virtual ~SimpleHttpClient () {}
  void Setup (Ipv4Address addr, uint16_t port)
  {
    m_addr = addr;
    m_port = port;
  }

private:
  virtual void StartApplication () override
  {
    m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
    m_socket->Connect (InetSocketAddress (m_addr, m_port));
    m_socket->SetConnectCallback (
        MakeCallback (&SimpleHttpClient::ConnectionSucceeded, this),
        MakeCallback (&SimpleHttpClient::ConnectionFailed, this));
    m_socket->SetRecvCallback (MakeCallback (&SimpleHttpClient::HandleRead, this));
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

  void ConnectionSucceeded (Ptr<Socket> socket)
  {
    std::string httpRequest =
      "GET / HTTP/1.1\r\n"
      "Host: 10.1.1.1\r\n"
      "Connection: close\r\n"
      "\r\n";
    Ptr<Packet> packet = Create<Packet> ((uint8_t*) httpRequest.c_str (), httpRequest.size ());
    socket->Send (packet);
  }

  void ConnectionFailed (Ptr<Socket> socket)
  {
    // Do nothing for now
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv ()))
      {
        // Received server response
      }
  }

  Ptr<Socket> m_socket;
  Ipv4Address m_addr;
  uint16_t m_port;
};

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Wi-Fi PHY & channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("network-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

  NetDeviceContainer devices;
  devices.Add (staDevice);
  devices.Add (apDevice);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t httpPort = 80;

  Ptr<SimpleHttpServer> serverApp = CreateObject<SimpleHttpServer> ();
  serverApp->Setup (httpPort);
  wifiNodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (1.0));
  serverApp->SetStopTime (Seconds (10.0));

  Ptr<SimpleHttpClient> clientApp = CreateObject<SimpleHttpClient> ();
  clientApp->Setup (interfaces.GetAddress (1), httpPort);
  wifiNodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (2.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}