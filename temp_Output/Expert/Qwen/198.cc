#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class V2VWaveSimulator {
public:
  V2VWaveSimulator();
  void Run();

private:
  void SetupNodes();
  void SetupMobility();
  void SetupWave();
  void SetupApplications();
  void ReportStatistics();

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;
};

V2VWaveSimulator::V2VWaveSimulator()
    : m_packetsSent(0), m_packetsReceived(0) {
}

void V2VWaveSimulator::Run() {
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  SetupNodes();
  SetupMobility();
  SetupWave();
  SetupApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  ReportStatistics();
  Simulator::Destroy();
}

void V2VWaveSimulator::SetupNodes() {
  m_nodes.Create(2);
}

void V2VWaveSimulator::SetupMobility() {
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);
}

void V2VWaveSimulator::SetupWave() {
  YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
  WAVEMacHelper waveMac = WAVEMacHelper::Default();

  WaveHelper waveHelper = WaveHelper::Default();
  waveHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("OfdmRate6_5MbpsBW10MHz"),
                                    "ControlMode", StringValue("OfdmRate6_5MbpsBW10MHz"));

  InternetStackHelper internet;
  internet.Install(m_nodes);

  m_devices = waveHelper.Install(wavePhy, waveMac, m_nodes);
  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void V2VWaveSimulator::SetupApplications() {
  uint16_t port = 9;

  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(m_nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(m_interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(20));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(m_nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Trace packets sent and received
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", MakeCallback([](Ptr<const Packet> p) {
    ++instance->m_packetsSent;
  }));

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback([](Ptr<const Packet> p) {
    ++instance->m_packetsReceived;
  }));
}

void V2VWaveSimulator::ReportStatistics() {
  NS_LOG_UNCOND("Total Packets Sent: " << m_packetsSent);
  NS_LOG_UNCOND("Total Packets Received: " << m_packetsReceived);
  NS_LOG_UNCOND("Packet Delivery Ratio (%): " << (static_cast<double>(m_packetsReceived) / m_packetsSent) * 100.0);
}

int main(int argc, char *argv[]) {
  V2VWaveSimulator sim;
  sim.Run();
  return 0;
}