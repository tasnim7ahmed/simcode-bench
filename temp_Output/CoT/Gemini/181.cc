#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer vehicles;
  vehicles.Create(2);

  Wifi80211pHelper wifi80211pHelper;
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  QosWaveMacHelper waveMac = QosWaveMacHelper::Default();
  WaveNetDeviceHelper waveNetDevice = WaveNetDeviceHelper::Default();
  waveNetDevice.SetPhy(wifiPhy);
  waveNetDevice.SetMac(waveMac);

  NetDeviceContainer waveDevices = waveNetDevice.Install(vehicles);

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(waveDevices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles);

  Ptr<ConstantVelocityMobilityModel> mobilityModel0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mobilityModel0->SetVelocity(Vector(5, 0, 0));
  Ptr<ConstantVelocityMobilityModel> mobilityModel1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mobilityModel1->SetVelocity(Vector(5, 0, 0));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AnimationInterface anim("vanet-animation.xml");
  anim.SetConstantPosition(vehicles.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(vehicles.Get(1), 5.0, 0.0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}