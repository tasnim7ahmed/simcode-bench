#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiMultiClientToServer");

class TcpServerApp : public Application
{
public:
  TcpServerApp () : m_socket(0), m_received(0) {}
  virtual ~TcpServerApp () { m_socket = 0; }

  void Setup (Address address, uint16_t port)
  {
    m_local = InetSocketAddress (Ipv4Address::GetAny(), port);
  }
  
  virtual void StartApplication ()
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        m_socket->Bind (m_local);
        m_socket->Listen ();
        m_socket->SetAcceptCallback (
          MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
          MakeCallback (&TcpServerApp::HandleAccept, this));
        m_socket->SetRecvCallback (MakeCallback (&TcpServerApp::HandleRead, this));
      }
  }

  virtual void StopApplication ()
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = 0;
      }
  }

private:
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        m_received += packet->GetSize ();
        std::ofstream log ("server-receive.log", std::ios_base::app);
        log << "Time " << Simulator::Now ().GetSeconds () << "s: Received "
            << packet->GetSize () << " bytes from "
            << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
            << std::endl;
      }
  }
  void HandleAccept (Ptr<Socket> s, const Address& from) {}

  Ptr<Socket> m_socket;
  Address m_local;
  uint32_t m_received;
};

int main (int argc, char *argv[])
{
  uint32_t nClients = 5;
  uint32_t payloadSize = 512;
  double simTime = 5.0;
  uint16_t serverPort = 50000;

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
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

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
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevices);

  Ptr<TcpServerApp> serverApp = CreateObject<TcpServerApp> ();
  serverApp->Setup (InetSocketAddress (Ipv4Address::GetAny (), serverPort), serverPort);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simTime));

  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> clientSocket = Socket::CreateSocket (wifiStaNodes.Get (i), TcpSocketFactory::GetTypeId ());
      Address serverAddress = InetSocketAddress (apInterface.GetAddress (0), serverPort);

      Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
      app->SetAttribute ("Remote", AddressValue(serverAddress));
      app->SetAttribute ("MaxBytes", UintegerValue (payloadSize));
      wifiStaNodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.0 + i*0.1));
      app->SetStopTime (Seconds (simTime));
    }

  Simulator::Stop (Seconds (simTime + 0.5));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}