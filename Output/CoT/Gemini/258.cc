#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer vehicles;
  vehicles.Create(3);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  QosWaveMacHelper wifiMac = QosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifiMac, vehicles);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles);

  Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob0->SetVelocity(Vector(5, 0, 0));

  Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetVelocity(Vector(10, 0, 0));

  Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();
  mob2->SetVelocity(Vector(15, 0, 0));

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(3));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(vehicles.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}