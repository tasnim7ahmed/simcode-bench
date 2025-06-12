#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiUdpClientServer");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("UdpServer")
      .SetParent<Application> ()
      .SetGroupName("Applications")
      .AddConstructor<UdpServer> ()
      ;
    return tid;
  }

  UdpServer () {}
  virtual ~UdpServer () {}

private:
  virtual void StartApplication (void)
  {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
    recvSocket->Bind (local);
    recvSocket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
    NS_LOG_INFO ("UDP server socket created and bound.");
  }

  virtual void StopApplication (void) {}

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("Received packet of size " << packet->GetSize () << " from " << from);
    }
  }
};

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  NetDeviceContainer apDevice;
  mac.SetType ("ns3::ApWifiMac");
  apDevice = wifi.Install (phy, mac, wifiApNode);

  NetDeviceContainer staDevice;
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("wifi-test")));
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  Ipv4InterfaceContainer staInterface;
  staInterface = address.Assign (staDevice);

  UdpServerHelper serverHelper;
  ApplicationContainer serverApp = serverHelper.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 20;
  Time interPacketInterval = Seconds (1.0);

  UdpClientHelper client (apInterface.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = client.Install (wifiStaNode.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}