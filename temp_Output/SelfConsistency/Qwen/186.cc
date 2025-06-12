#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

class VanetExample
{
public:
  VanetExample();
  void Run();

private:
  void SetupNodes();
  void SetupWave();
  void SetupApplications();
  void SetupAnimation();

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
  AnimationInterface* m_anim;
};

VanetExample::VanetExample()
{
  m_nodes.Create(5);
  m_anim = nullptr;
}

void VanetExample::SetupNodes()
{
  // Mobility setup: constant velocity mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(m_nodes);

  // Set initial positions and velocities
  for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
  {
    Ptr<Node> node = m_nodes.Get(i);
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    Vector pos(100.0 + i * 50.0, 0.0, 0.0); // Spread nodes along x-axis
    mobility->SetPosition(pos);
    DynamicCast<ConstantVelocityMobilityModel>(mobility)->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s in x-direction
  }
}

void VanetExample::SetupWave()
{
  // Use 802.11p (WAVE) PHY and MAC
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211_2016);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  WifiChannelHelper channel = WifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  m_devices = wifi.Install(wifiPhy, mac, m_nodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void VanetExample::SetupApplications()
{
  // Use UDP echo client-server model to simulate BSMs
  uint16_t port = 8080;

  // Install server on all nodes
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(m_nodes);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Create periodic BSM-like traffic from each node to others
  UdpEchoClientHelper echoClient;
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(100));

  for (uint32_t i = 0; i < m_nodes.GetN(); ++i)
  {
    for (uint32_t j = 0; j < m_nodes.GetN(); ++j)
    {
      if (i != j)
      {
        echoClient.SetRemote(m_interfaces.GetAddress(j), port);
        ApplicationContainer clientApps = echoClient.Install(m_nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
      }
    }
  }
}

void VanetExample::SetupAnimation()
{
  m_anim = new AnimationInterface("vanet-animation.xml");
  m_anim->Set MobilityPollInterval(Seconds(0.1));
  m_anim->UpdateNodeDescription(m_nodes, "Vehicle");
  m_anim->UpdateNodeColor(m_nodes, 255, 0, 0); // Red color for vehicles
}

void VanetExample::Run()
{
  SetupNodes();
  SetupWave();
  SetupApplications();
  SetupAnimation();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char* argv[])
{
  LogComponentEnable("VanetSimulation", LOG_LEVEL_INFO);
  VanetExample vanetSim;
  vanetSim.Run();
  return 0;
}