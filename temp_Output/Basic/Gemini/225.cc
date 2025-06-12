#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DsdvVanet");

int main(int argc, char* argv[]) {
  bool enableFlowMonitor = true;

  CommandLine cmd;
  cmd.AddValue("EnableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);

  NodeContainer rsuNode;
  rsuNode.Create(1);

  NodeContainer vehicleNodes;
  vehicleNodes.Create(10);

  NodeContainer allNodes;
  allNodes.Add(rsuNode);
  allNodes.Add(vehicleNodes);

  DsdvHelper dsdv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(dsdv);
  stack.Install(allNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("1500"),
                                "Y", StringValue("1500"),
                                "Z", StringValue("0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel("ns3::WaypointMobilityModel");
  mobility.Install(allNodes);

  for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
    Ptr<Node> node = allNodes.Get(i);
    Ptr<WaypointMobilityModel> mob = node->GetObject<WaypointMobilityModel>();
    if (mob) {
      for (double j = 0; j < 30; j += 5) {
        Waypoint wp;
        wp.time = Seconds(j);
        wp.position = Vector3D(j * 100 + i*10, 100 + i * 2, 0);
        mob->AddWaypoint(wp);
      }
    }
  }

  NetDeviceContainer devices;
  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  WifiMacHelper mac;
  Ssid ssid = Ssid("vanet-network");
  mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
  devices = wifi.Install(phy, mac, allNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  UdpClientHelper client(interfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < allNodes.GetN(); ++i) {
    clientApps.Add(client.Install(vehicleNodes.Get(i - 1)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(30.0));

  UdpServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(rsuNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(30.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  AnimationInterface anim("dsdv-vanet.xml");
  anim.SetConstantPosition(rsuNode.Get(0), 100, 100);

  if (enableFlowMonitor) {
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();

    flowMonitor->SerializeToXmlFile("dsdv-vanet.flowmon", false, true);
  } else {
    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
  }

  Simulator::Destroy();
  return 0;
}