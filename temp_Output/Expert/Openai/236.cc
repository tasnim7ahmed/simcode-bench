#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerApp : public Application
{
public:
  UdpServerApp () {}
  virtual ~UdpServerApp () {}
  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  uint16_t        m_port;
};

void
UdpServerApp::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServerApp::StartApplication (void)
{
  Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), m_port));
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->Bind (localAddress);
  m_socket->SetRecvCallback (MakeCallback (&UdpServerApp::HandleRead, this));
}

void
UdpServerApp::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_socket = nullptr;
    }
}

void
UdpServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
          NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from "
                                   << address.GetIpv4 () << ":" << address.GetPort ());
        }
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnable ("UdpServerApp", LOG_LEVEL_INFO);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiStaNode);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevice);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevice);

  uint16_t port = 4000;
  Ptr<UdpServerApp> serverApp = CreateObject<UdpServerApp> ();
  serverApp->Setup (port);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  UdpClientHelper client (apInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (20));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiStaNode.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}