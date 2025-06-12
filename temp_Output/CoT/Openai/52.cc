#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParamsDemo");

class ThroughputCalculator
{
public:
  void Rx (Ptr<const Packet> packet, const Address &)
  {
    m_bytes += packet->GetSize();
  }
  void Reset () { m_bytes = 0; }
  uint64_t GetBytes () const { return m_bytes; }
private:
  uint64_t m_bytes = 0;
};

int main(int argc, char *argv[])
{
  double slotTime = 9.0;
  double sifsTime = 10.0;
  double pifsTime = 19.0;
  double simTime = 10.0;
  uint32_t payloadSize = 1024;
  uint32_t nPackets = 10000;
  double interPacketInterval = 0.001;
  
  CommandLine cmd;
  cmd.AddValue("slotTime", "Slot time in microseconds", slotTime);
  cmd.AddValue("sifsTime", "SIFS time in microseconds", sifsTime);
  cmd.AddValue("pifsTime", "PIFS time in microseconds", pifsTime);
  cmd.AddValue("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue("payloadSize", "Payload size per packet (bytes)", payloadSize);
  cmd.AddValue("nPackets", "Number of packets", nPackets);
  cmd.AddValue("interPacketInterval", "Time between packets (s)", interPacketInterval);
  cmd.Parse(argc, argv);

  NodeContainer wifiNodes;
  wifiNodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

  // Set timing parameters
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  Config::SetDefault("ns3::WifiPhy::Slot", TimeValue(MicroSeconds(slotTime)));
  Config::SetDefault("ns3::WifiPhy::Sifs", TimeValue(MicroSeconds(sifsTime)));
  Config::SetDefault("ns3::WifiPhy::Pifs", TimeValue(MicroSeconds(pifsTime)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-timing-demo");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiNodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiNodes.Get(0));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
  positionAlloc->Add(Vector(5.0, 0.0, 0.0)); // STA
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = ipv4.Assign(NetDeviceContainer(apDevice, staDevice));

  uint16_t port = 9999;

  // UDP server on STA
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(simTime));

  // UDP client on AP
  UdpClientHelper client(ifs.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(nPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
  client.SetAttribute("PacketSize", UintegerValue(payloadSize));
  ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(simTime));

  // Throughput calculation
  Ptr<ThroughputCalculator> tput = CreateObject<ThroughputCalculator>();
  wifiNodes.Get(1)->GetObject<Ipv4>()->GetObject<NetDevice>()->TraceConnectWithoutContext(
    "PhyRxEnd", MakeCallback(&ThroughputCalculator::Rx, tput)
  );

  // Alternatively, hook directly to server
  Ptr<UdpServer> actualServer = DynamicCast<UdpServer>(serverApp.Get(0));
  Simulator::Schedule(Seconds(simTime), [&]() {
    uint64_t totalBytes = actualServer->GetReceived();
    double throughput = (totalBytes * 8.0) / (simTime - 1.0) / 1000000.0; // Mbps
    std::cout << "Received " << totalBytes << " bytes in "
              << (simTime - 1.0) << "s: Throughput = "
              << throughput << " Mbps" << std::endl;
  });

  Simulator::Stop(Seconds(simTime + 0.1));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}