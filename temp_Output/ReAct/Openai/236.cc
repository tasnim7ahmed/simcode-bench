#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerWithLogging : public Application
{
public:
  UdpServerWithLogging () {}
  virtual ~UdpServerWithLogging () {}

protected:
  virtual void StartApplication () override
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 4000);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&UdpServerWithLogging::HandleRead, this));
    }
  }
  virtual void StopApplication () override
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
      NS_LOG_UNCOND ("AP received " << packet->GetSize () << " bytes from "
                       << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                       << " at " << Simulator::Now ().GetSeconds () << "s");
    }
  }
private:
  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable("UdpServerWithLogging", LOG_LEVEL_INFO);

  NodeContainer wifiApNode, wifiStaNode;
  wifiApNode.Create (1);
  wifiStaNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices, apDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface, staInterface;
  staInterface = address.Assign (staDevices);
  apInterface = address.Assign (apDevices);

  // Install UDP server with logging at AP
  Ptr<UdpServerWithLogging> serverApp = CreateObject<UdpServerWithLogging> ();
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (10.0));

  // Install UDP client at STA
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double interval = 1.0;

  UdpClientHelper clientHelper (apInterface.GetAddress (0), 4000);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = clientHelper.Install (wifiStaNode.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}