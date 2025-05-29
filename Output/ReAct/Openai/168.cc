#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpServerClient");

class UdpServerThroughput : public Object
{
public:
  UdpServerThroughput () : m_bytesReceived (0) {}
  void HandleRead (Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom (from)))
      {
        m_bytesReceived += packet->GetSize ();
      }
  }
  void Reset () { m_bytesReceived = 0; }
  uint64_t GetBytesReceived () const { return m_bytesReceived; }
private:
  uint64_t m_bytesReceived;
};

int main (int argc, char *argv[])
{
  uint32_t nSta = 4;
  uint32_t udpServerIndex = 1;
  uint32_t udpClientIndex = 2;
  double simulationTime = 10.0;
  uint32_t packetSize = 1024;
  double interPacketInterval = 0.01; // 10ms
  std::string dataRate = "54Mbps";
  std::string delay = "2ms";
  uint16_t udpPort = 5000;
  CommandLine cmd;
  cmd.AddValue ("nSta", "Number of Wi-Fi STA devices", nSta);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  Ptr<YansWifiChannel> chan = channel.Create ();

  Ptr<YansWifiPhy> phyHelper = CreateObject<YansWifiPhy> ();
  YansWifiPhyHelper phy;
  phy.SetChannel (chan);
  phy.Set ("TxPowerStart", DoubleValue (16.0));
  phy.Set ("TxPowerEnd", DoubleValue (16.0));
  phy.SetErrorRateModel ("ns3::YansErrorRateModel");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("ErpOfdmRate54Mbps"),
                               "ControlMode", StringValue ("ErpOfdmRate24Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (phy, mac, wifiApNode);

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));

  // Set channel data rate and delay at the PHY layer
  phy.Set ("DataLinkDelay", TimeValue (MilliSeconds (2)));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nSta; ++i)
    {
      positionAlloc->Add (Vector (0.0, i * 5.0 + 1.0, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  MobilityHelper mobilityAp;
  Ptr<ListPositionAllocator> posAp = CreateObject<ListPositionAllocator> ();
  posAp->Add (Vector (0.0, 0.0, 0.0));
  mobilityAp.SetPositionAllocator (posAp);
  mobilityAp.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterfaces;

  NetDeviceContainer allDevices;
  allDevices.Add (staDevices);
  allDevices.Add (apDevices);
  Ipv4InterfaceContainer allInterfaces = address.Assign (allDevices);

  // UDP Server socket (on wifiStaNodes[udpServerIndex])
  Ptr<Node> serverNode = wifiStaNodes.Get (udpServerIndex);
  Ptr<Socket> udpServerSocket = Socket::CreateSocket (serverNode, UdpSocketFactory::GetTypeId ());
  InetSocketAddress localAddr = InetSocketAddress (Ipv4Address::GetAny (), udpPort);
  udpServerSocket->Bind (localAddr);
  udpServerSocket->SetRecvPktInfo (true);

  Ptr<UdpServerThroughput> serverApp = CreateObject<UdpServerThroughput> ();
  udpServerSocket->SetRecvCallback (MakeCallback (&UdpServerThroughput::HandleRead, serverApp));

  // UDP Client application (on wifiStaNodes[udpClientIndex] to serverNode)
  Ptr<Node> clientNode = wifiStaNodes.Get (udpClientIndex);
  Ptr<Socket> udpClientSocket = Socket::CreateSocket (clientNode, UdpSocketFactory::GetTypeId ());

  Simulator::ScheduleWithContext (udpClientSocket->GetNode ()->GetId (), Seconds (1.0), [&] {
    Ipv4Address serverIp = allInterfaces.GetAddress (udpServerIndex);
    udpClientSocket->Connect (InetSocketAddress (serverIp, udpPort));
    Simulator::Schedule (Seconds (0.0), [&]{
      Time pktInterval = Seconds (interPacketInterval);
      EventId eId;
      std::function<void ()> sendFunc = [&, pktInterval, eId]() mutable {
        Ptr<Packet> pkt = Create<Packet> (packetSize);
        udpClientSocket->Send (pkt);
        if (Simulator::Now ().GetSeconds () + pktInterval.GetSeconds () < simulationTime)
          eId = Simulator::Schedule (pktInterval, sendFunc);
      };
      sendFunc ();
    });
  });

  udpServerSocket->SetRecvCallback (MakeCallback (&UdpServerThroughput::HandleRead, serverApp));

  // Enable pcap and ASCII tracing
  phy.EnablePcapAll ("wifi-udp");
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-udp.tr"));

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  double throughput = (serverApp->GetBytesReceived () * 8.0) / simulationTime / 1e6; // Mbps
  std::cout << "Received bytes: " << serverApp->GetBytesReceived ()
            << " Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}