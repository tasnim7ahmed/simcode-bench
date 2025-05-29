#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-mac-module.h"
#include "ns3/wave-net-device.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wave-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  Wifi80211pHelper wifi80211pHelper;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifi80211pHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                            "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                            "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer devices = wifi80211pHelper.Install(wifiPhy, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob1->SetVelocity(Vector(10, 0, 0));
  Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
  mob2->SetVelocity(Vector(10, 0, 0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  AnimationInterface anim("vanet.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
  anim.SetConstantPosition(nodes.Get(1), 5.0, 0.0);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}