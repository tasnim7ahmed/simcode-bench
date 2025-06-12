#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer vehicles;
  vehicles.Create(3);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                     "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, vehicles);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

  mobility.Install(vehicles);

  Ptr<ConstantVelocityMobilityModel> mob0 = vehicles.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  Ptr<ConstantVelocityMobilityModel> mob1 = vehicles.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  Ptr<ConstantVelocityMobilityModel> mob2 = vehicles.Get(2)->GetObject<ConstantVelocityMobilityModel>();

  mob0->SetPosition(Vector(0.0, 0.0, 0.0));
  mob1->SetPosition(Vector(10.0, 0.0, 0.0));
  mob2->SetPosition(Vector(20.0, 0.0, 0.0));

  mob0->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s
  mob1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s
  mob2->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Last vehicle as UDP Echo Server
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(vehicles.Get(2));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // First vehicle as UDP Echo Client
  UdpEchoClientHelper echoClient(interfaces.GetAddress(2), port);
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