#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes.Get(1)); // RSU

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes.Get(0)); // Vehicle

  Ptr<ConstantVelocityMobilityModel> velocityModel = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  velocityModel->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p;
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = wifi80211p.Install(phy, mac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper server(9);
  ApplicationContainer apps = server.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  apps = client.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}