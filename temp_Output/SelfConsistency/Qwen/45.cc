#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessInterferenceSimulation");

class InterferencePacketSink : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    return TypeId("InterferencePacketSink")
        .SetParent<Application>()
        .AddConstructor<InterferencePacketSink>();
  }

  InterferencePacketSink() : m_socket(0), m_totalRx(0) {}
  virtual ~InterferencePacketSink() {}

protected:
  void DoDispose() override
  {
    m_socket = nullptr;
    Application::DoDispose();
  }

private:
  void StartApplication() override
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&InterferencePacketSink::HandleRead, this));
    }
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      m_totalRx += packet->GetSize();
      NS_LOG_UNCOND("Received packet of size " << packet->GetSize() << " at node "
                                              << GetNode()->GetId()
                                              << ". Total received: " << m_totalRx << " bytes.");
    }
  }

  Ptr<Socket> m_socket;
  uint64_t m_totalRx;
};

int main(int argc, char *argv[])
{
  double primaryRss = -80;          // dBm
  double interfererRss = -75;       // dBm
  double interferenceOffset = 0.1;  // seconds
  uint32_t primaryPacketSize = 1000;
  uint32_t interfererPacketSize = 1000;
  bool verbose = false;
  bool pcapTracing = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("primaryRss", "RSS of the primary transmission in dBm", primaryRss);
  cmd.AddValue("interfererRss", "RSS of the interfering transmission in dBm", interfererRss);
  cmd.AddValue("interferenceOffset", "Time offset between primary and interferer in seconds", interferenceOffset);
  cmd.AddValue("primaryPacketSize", "Size of packets sent by primary transmitter", primaryPacketSize);
  cmd.AddValue("interfererPacketSize", "Size of packets sent by interferer", interfererPacketSize);
  cmd.AddValue("verbose", "Turn on all device log messages", verbose);
  cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
  cmd.Parse(argc, argv);

  if (verbose)
  {
    WifiHelper::EnableLogComponents();
    LogComponentEnable("InterferencePacketSink", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(3); // 0: transmitter, 1: receiver, 2: interferer

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  NetDeviceContainer devices;
  mac.SetType("ns3::AdhocWifiMac");
  devices = wifi.Install(phy, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Primary transmission: node 0 -> node 1
  UdpServerHelper server(9);
  ApplicationContainer sinkApp = server.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(primaryPacketSize));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(0.0));
  clientApp.Stop(Seconds(10.0));

  // Interfering transmission: node 2 -> node 1 with offset
  UdpClientHelper interfererClient(interfaces.GetAddress(1), 9);
  interfererClient.SetAttribute("MaxPackets", UintegerValue(1));
  interfererClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  interfererClient.SetAttribute("PacketSize", UintegerValue(interfererPacketSize));

  ApplicationContainer interfererApp = interfererClient.Install(nodes.Get(2));
  interfererApp.Start(Seconds(interferenceOffset));
  interfererApp.Stop(Seconds(10.0));

  // Disable carrier sense for the interferer
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/CcaMode1Threshold", DoubleValue(-101.0)); // very low threshold to almost always transmit
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(interfererRss));
  Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(interfererRss));

  // Set primary TX power
  Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(primaryRss));
  Config::Set("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(primaryRss));

  if (pcapTracing)
  {
    phy.EnablePcap("wireless-interference-primary", devices.Get(0));
    phy.EnablePcap("wireless-interference-interferer", devices.Get(2));
    phy.EnablePcap("wireless-interference-receiver", devices.Get(1));
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}