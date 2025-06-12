#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class V2VWaveStats {
public:
  uint32_t txPackets;
  uint32_t rxPackets;

  V2VWaveStats() : txPackets(0), rxPackets(0) {}

  void TxPacketCallback(Ptr<const Packet> packet) {
    txPackets++;
  }

  void RxPacketCallback(Ptr<const Packet> packet) {
    rxPackets++;
  }
};

int main(int argc, char *argv[]) {
  double simDuration = 10.0; // seconds

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it) {
    Ptr<ConstantVelocityMobilityModel> model = (*it)->GetObject<ConstantVelocityMobilityModel>();
    model->SetVelocity(Vector(20.0, 0.0, 0.0)); // Move along X-axis at 20 m/s
  }

  WaveHelper waveHelper = WaveHelper::Default();
  NetDeviceContainer waveDevices = waveHelper.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(waveDevices);

  V2VWaveStats stats;

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simDuration));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(20));
  client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simDuration));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&V2VWaveStats::TxPacketCallback, &stats));
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&V2VWaveStats::RxPacketCallback, &stats));

  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();

  std::cout << "Sent packets: " << stats.txPackets << std::endl;
  std::cout << "Received packets: " << stats.rxPackets << std::endl;

  Simulator::Destroy();

  return 0;
}