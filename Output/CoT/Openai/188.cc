#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiStaApUdpNetAnim");

class UdpMetrics : public Application
{
public:
  UdpMetrics () : m_rxPackets (0), m_rxBytes (0) {}

  void Setup (Ptr<Socket> socket)
  {
    m_socket = socket;
  }

  uint32_t GetRxPackets () const { return m_rxPackets; }
  uint32_t GetRxBytes () const { return m_rxBytes; }

private:
  virtual void StartApplication () override
  {
    if (m_socket)
      {
        m_socket->SetRecvCallback (MakeCallback (&UdpMetrics::HandleRead, this));
      }
  }

  virtual void StopApplication () override
  {
    if (m_socket)
      {
        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      }
  }

  void HandleRead (Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        m_rxPackets++;
        m_rxBytes += packet->GetSize ();
      }
  }

  Ptr<Socket> m_socket;
  uint32_t m_rxPackets;
  uint32_t m_rxBytes;
};

int main (int argc, char *argv[])
{
  double simulationTime = 10.0; // seconds
  uint32_t payloadSize = 1024; // bytes
  std::string dataRate = "5Mbps";
  std::string wifiChannelNumber = "6";
  std::string phyMode ("HtMcs7");
  std::string ssidString ("ns3-wifi-ssid");

  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // WiFi - Channel and PHY
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // WiFi MAC
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid (ssidString);

  NetDeviceContainer staDevices, apDevice;

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (2.5, 5.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterface;
  NetDeviceContainer allDevices;
  allDevices.Add (staDevices.Get (0));
  allDevices.Add (staDevices.Get (1));
  allDevices.Add (apDevice.Get (0));
  Ipv4InterfaceContainer interfaces = address.Assign (allDevices);

  Ipv4Address sta1Addr = interfaces.GetAddress (0);
  Ipv4Address sta2Addr = interfaces.GetAddress (1);

  // UDP Application Setup: STA1 (Sender) --> STA2 (Receiver) via AP
  uint16_t port = 50000;
  // Receiver app at STA2
  Ptr<Socket> recvSocket = Socket::CreateSocket (wifiStaNodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress localAddr = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (localAddr);

  Ptr<UdpMetrics> metricsApp = CreateObject<UdpMetrics> ();
  metricsApp->Setup (recvSocket);
  wifiStaNodes.Get(1)->AddApplication (metricsApp);
  metricsApp->SetStartTime (Seconds (1.0));
  metricsApp->SetStopTime (Seconds (simulationTime));

  // Sender app at STA1
  Ptr<Socket> sourceSocket = Socket::CreateSocket (wifiStaNodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remoteAddr = InetSocketAddress (sta2Addr, port);
  sourceSocket->Connect (remoteAddr);

  // Generate CBR UDP traffic using an OnOffApplication
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (sta2Addr, port));
  onoff.SetAttribute ("DataRate", StringValue (dataRate));
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
  ApplicationContainer senderApp = onoff.Install (wifiStaNodes.Get (0));

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // NetAnim
  AnimationInterface anim ("wifi_sta_ap_udp.xml");
  anim.SetConstantPosition (wifiStaNodes.Get(0), 0.0, 0.0, 0.0); // STA1
  anim.SetConstantPosition (wifiStaNodes.Get(1), 5.0, 0.0, 0.0); // STA2
  anim.SetConstantPosition (wifiApNode.Get(0), 2.5, 5.0, 0.0); // AP
  anim.UpdateNodeDescription (wifiStaNodes.Get(0), "STA1");
  anim.UpdateNodeDescription (wifiStaNodes.Get(1), "STA2");
  anim.UpdateNodeDescription (wifiApNode.Get(0), "AP");
  anim.UpdateNodeColor (wifiStaNodes.Get(0), 255,0,0);    // Red
  anim.UpdateNodeColor (wifiStaNodes.Get(1), 0,255,0);    // Green
  anim.UpdateNodeColor (wifiApNode.Get(0), 0,0,255);      // Blue

  // Enable PCAP tracing
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("sta1", staDevices.Get(0), true, true);
  phy.EnablePcap ("sta2", staDevices.Get(1), true, true);
  phy.EnablePcap ("ap", apDevice.Get(0), true, true);

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Collect Metrics
  uint32_t totalRxPackets = metricsApp->GetRxPackets ();
  uint32_t totalRxBytes = metricsApp->GetRxBytes ();
  double throughput = (totalRxBytes * 8.0) / (simulationTime - 2.0) / 1e6; // Mbps

  uint32_t sentPackets = DynamicCast<OnOffApplication> (senderApp.Get(0))->GetTotalTx ();
  std::cout << "Total Rx Packets: " << totalRxPackets << std::endl;
  std::cout << "Total Tx Packets: " << sentPackets << std::endl;
  std::cout << "Packet loss: " << (sentPackets > totalRxPackets ? sentPackets - totalRxPackets : 0) << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}