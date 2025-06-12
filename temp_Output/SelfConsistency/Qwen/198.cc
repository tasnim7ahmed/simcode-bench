#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/config.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2VWaveSimulation");

class V2VWaveExperiment
{
public:
  V2VWaveExperiment();
  void Run();

private:
  void SetupNodes();
  void SetupWave();
  void SetupApplications();
  void Report();

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;
};

V2VWaveExperiment::V2VWaveExperiment()
    : m_packetsSent(0), m_packetsReceived(0)
{
}

void
V2VWaveExperiment::SetupNodes()
{
  m_nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(m_nodes);

  // Set initial position and velocity for node 0 (moving towards node 1)
  m_nodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
  m_nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(20.0, 0.0, 0.0));

  // Set initial position for node 1 (stationary)
  m_nodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(100.0, 0.0, 0.0));
}

void
V2VWaveExperiment::SetupWave()
{
  WAVEHelper waveHelper = WAVEHelper::Default();
  InternetStackHelper internet;
  internet.Install(m_nodes);

  m_devices = waveHelper.InstallWaveDevice(m_nodes);
  waveHelper.AssignStreams(m_devices, 0);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void
V2VWaveExperiment::SetupApplications()
{
  UdpServerHelper server(80);
  ApplicationContainer serverApps = server.Install(m_nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(m_interfaces.GetAddress(1), 80);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(m_nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Connect packet sent/received trace sources
  Config::Connect("/NodeList/0/ApplicationList/0/$ns3::UdpClient/PacketSent",
                  MakeCallback(&V2VWaveExperiment::Report),
                  this);
  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/PacketReceived",
                  MakeCallback(&V2VWaveExperiment::Report),
                  this);
}

void
V2VWaveExperiment::Report()
{
  m_packetsSent++;
}

void
V2VWaveExperiment::Report()
{
  m_packetsReceived++;
}

void
V2VWaveExperiment::Run()
{
  SetupNodes();
  SetupWave();
  SetupApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  std::cout << "\n--- Simulation Statistics ---" << std::endl;
  std::cout << "Packets Sent:     " << m_packetsSent << std::endl;
  std::cout << "Packets Received: " << m_packetsReceived << std::endl;
  std::cout << "Packet Loss Rate: "
            << ((double)(m_packetsSent - m_packetsReceived) / m_packetsSent * 100.0) << "%"
            << std::endl;

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("V2VWaveSimulation", LOG_LEVEL_INFO);
  V2VWaveExperiment experiment;
  experiment.Run();
  return 0;
}