#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  NodeContainer rsu;
  rsu.Add(nodes.Get(0));

  NodeContainer vehicle;
  vehicle.Add(nodes.Get(1));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WaveMacHelper wifi80211pMac = WaveMacHelper::Default();
  WifiHelper wifi80211p;
  wifi80211p.SetStandard(WIFI_PHY_STANDARD_80211p);
  wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                      "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer rsuDevice = wifi80211p.Install(wifiPhy, wifi80211pMac.CreateOcbWifiMac(OcbWifiMacHelper::OcbMode::RSU), rsu);
  NetDeviceContainer vehicleDevice = wifi80211p.Install(wifiPhy, wifi80211pMac.CreateOcbWifiMac(OcbWifiMacHelper::OcbMode::OCB), vehicle);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(nodes);

  Ptr<ConstantVelocityMobilityModel> vehicleModel = vehicle.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  vehicleModel->SetVelocity(Vector(10, 0, 0)); // 10 m/s

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(rsuDevice,vehicleDevice));

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(rsu.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(vehicle.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
  }

  Simulator::Destroy();
  return 0;
}