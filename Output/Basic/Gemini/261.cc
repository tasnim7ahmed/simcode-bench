#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wave-net-device.h"
#include "ns3/wave-helper.h"
#include "ns3/uinteger.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer vehicles;
  vehicles.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("Model", "ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("Model", "ns3::FriisPropagationLossModel");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
  WaveHelper waveHelper;
  NetDeviceContainer devices = waveHelper.Install(wifiPhy, wifiMac, vehicles);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles);

  Ptr<ConstantVelocityMobilityModel> cvmm0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  cvmm0->SetVelocity(Vector(20, 0, 0));

  Ptr<ConstantVelocityMobilityModel> cvmm1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  cvmm1->SetVelocity(Vector(15, 0, 0));

  Ptr<ConstantVelocityMobilityModel> cvmm2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  cvmm2->SetVelocity(Vector(25, 0, 0));

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}