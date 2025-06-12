#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpMultiClientExample");

// Server application to log received packets
class UdpServerLogger : public Application
{
public:
  UdpServerLogger () {}
  virtual ~UdpServerLogger () {}

protected:
  virtual void StartApplication () override
  {
    // Create socket and bind it
    m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
    m_socket->Bind (local);
    m_socket->SetRecvCallback (MakeCallback (&UdpServerLogger::HandleRead, this));
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->Close ();
        m_socket = nullptr;
      }
  }

public:
  void SetPort (uint16_t port)
  {
    m_port = port;
  }

private:
  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        if (packet->GetSize () > 0)
          {
            InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
            NS_LOG_INFO ("Server received " << packet->GetSize ()
                            << " bytes from " << address.GetIpv4 ()
                            << " port " << address.GetPort ()
                            << " at " << Simulator::Now ().GetSeconds () << "s");
          }
      }
  }
  Ptr<Socket> m_socket;
  uint16_t m_port;
};


int
main (int argc, char *argv[])
{
  uint32_t nClients = 3;
  double simulationTime = 10.0;
  uint16_t port = 4000;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.AddValue ("simulationTime", "Duration of simulation [s]", simulationTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nClients);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Set up Wi-Fi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Wi-Fi MAC and device config
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility (static, not moving)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  // Create and install server application
  Ptr<UdpServerLogger> serverApp = CreateObject<UdpServerLogger> ();
  serverApp->SetPort (port);
  wifiApNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (simulationTime));
  
  // Create and install client applications
  for (uint32_t i = 0; i < nClients; ++i)
    {
      Ptr<Socket> clientSocket = Socket::CreateSocket (wifiStaNodes.Get (i), UdpSocketFactory::GetTypeId ());
      Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();

      // Send a packet every second
      uint32_t packetSize = 512;
      double interval = 1.0;

      // Lambda to schedule periodic packet sends
      auto sendFunction = [clientSocket, apInterface, port, packetSize, i] (void) {
        Ptr<Packet> packet = Create<Packet> (packetSize);
        clientSocket->SendTo (packet, 0, InetSocketAddress (apInterface.GetAddress (0), port));
      };

      // Schedule periodic sends
      for (double t = 1.0; t < simulationTime; t += interval)
        {
          Simulator::Schedule (Seconds (t + i * 0.01), sendFunction); // Small offset to avoid perfect synchronization
        }
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}