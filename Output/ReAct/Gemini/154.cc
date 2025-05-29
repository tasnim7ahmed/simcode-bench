#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/olsr-module.h"
#include "ns3/csv.h"
#include <fstream>
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(1)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(2)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(3)));
  staDevices.Add(wifi.Install(phy, mac, nodes.Get(4)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconInterval", TimeValue(Seconds(1.0)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(4));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps[4];
  for (int i = 0; i < 4; ++i) {
    clientApps[i] = echoClient.Install(nodes.Get(i));
    clientApps[i].Start(Seconds(2.0 + i));
    clientApps[i].Stop(Seconds(9.0 + i));
  }

  std::ofstream outputFile("simulation_data.csv");
  if (outputFile.is_open()) {
    outputFile << "Source Node,Destination Node,Packet Size,Transmission Time,Reception Time\n";

    Simulator::ScheduleDestroy(&outputFile);

    for (uint32_t i = 0; i < 4; ++i) {
        for (uint32_t j = 0; j < clientApps[i].GetNApplications(); ++j) {
          Ptr<Application> app = clientApps[i].Get(j);
          Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient>(app);

          client->TraceConnectWithoutContext("Tx", MakeCallback([&outputFile, i](Ptr<const Packet> packet) {
              Time txTime = Simulator::Now();
              uint32_t packetSize = packet->GetSize();

              Ptr<Ipv4> ipv4 = packet->FindFirstMatchingProtocol<Ipv4>();
              Ipv4Address destAddr = ipv4->GetDestination();

              Simulator::ScheduleWithContext(nodes.Get(4)->GetId(), Seconds(0.001), [&outputFile, i, packetSize, txTime, destAddr]() {
                  Time rxTime = Simulator::Now();
                  outputFile << i << "," << "4" << "," << packetSize << "," << txTime.GetSeconds() << "," << rxTime.GetSeconds() << "\n";
              });
          }));
        }
    }
    
  } else {
    std::cerr << "Error opening output file!" << std::endl;
    return 1;
  }

  Simulator::Stop(Seconds(15.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}