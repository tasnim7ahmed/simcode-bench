#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class UdpServerWithLogging : public Application
{
public:
  UdpServerWithLogging () : m_socket (0) {}
  virtual ~UdpServerWithLogging () { m_socket = 0; }

protected:
  virtual void StartApplication () override
  {
    if (!m_socket)
      {
        m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9000);
        m_socket->Bind (local);
        m_socket->SetRecvCallback (MakeCallback (&UdpServerWithLogging::HandleRead, this));
      }
  }
  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
      }
  }
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        if (packet->GetSize () > 0)
          {
            InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
            std::cout << Simulator::Now ().GetSeconds ()
                      << "s Packet received from " << address.GetIpv4 ()
                      << " (size=" << packet->GetSize () << " bytes)" << std::endl;
          }
      }
  }

private:
  Ptr<Socket> m_socket;
};

int main (int argc, char *argv[])
{
  uint32_t nClients = 4;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
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
  Ssid ssid = Ssid ("ns3-wifi-simple");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i=0; i<nClients; ++i)
    positionAlloc->Add (Vector (5.0 * i, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 5.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
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

  Ptr<Node> serverNode = wifiApNode.Get (0);
  Ptr<UdpServerWithLogging> app = CreateObject<UdpServerWithLogging> ();
  serverNode->AddApplication (app);
  app->SetStartTime (Seconds (0.0));
  app->SetStopTime (Seconds (simulationTime));

  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Node> clientNode = wifiStaNodes.Get (i);
      UdpClientHelper clientHelper (apInterface.GetAddress (0), 9000);
      clientHelper.SetAttribute ("MaxPackets", UintegerValue (320));
      clientHelper.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
      clientHelper.SetAttribute ("PacketSize", UintegerValue (128));
      ApplicationContainer clientApps = clientHelper.Install (clientNode);
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}