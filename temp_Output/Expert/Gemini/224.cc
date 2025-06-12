#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/random-waypoint-mobility-model.h"
#include "ns3/udp-client-server-helper.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(10);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.Add(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
  mobility.Install(nodes);

  uint16_t port = 9;
  UdpClientServerHelper echoClient(interfaces.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  for (uint32_t i = 1; i < nodes.GetN(); ++i) {
      Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
      rand->SetStream(i);
      
      Ptr<Node> source = nodes.Get(i);

      for(uint32_t j = 0; j < 5; ++j){
          uint32_t destNodeId = rand->GetValue(0, nodes.GetN() -1 );

          UdpClientServerHelper client(interfaces.GetAddress(destNodeId), port);
          client.SetAttribute("MaxPackets", UintegerValue(5));
          client.SetAttribute("Interval", TimeValue(Seconds(2.0 + (0.2 * i))));
          client.SetAttribute("PacketSize", UintegerValue(1024));
          ApplicationContainer clientApps2 = client.Install(source);
          clientApps2.Start(Seconds(2.0 + (0.5*i) + (1*j)));
          clientApps2.Stop(Seconds(15.0 + (0.5*i) + (1*j)));
      }
  }


  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}