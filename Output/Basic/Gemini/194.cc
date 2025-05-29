#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {
  bool tracing = true;
  uint32_t numStaAp1 = 5;
  uint32_t numStaAp2 = 5;
  double simulationTime = 10;
  std::string rate ("100kbps");
  uint16_t port = 9;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable tracing", tracing);
  cmd.AddValue ("numStaAp1", "Number of STA devices for AP1", numStaAp1);
  cmd.AddValue ("numStaAp2", "Number of STA devices for AP2", numStaAp2);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("UdpClient", LOG_LEVEL_INFO);

  NodeContainer apNodes;
  apNodes.Create(2);
  NodeContainer staNodesAp1;
  staNodesAp1.Create(numStaAp1);
  NodeContainer staNodesAp2;
  staNodesAp2.Create(numStaAp2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid("ns-3-ssid-1");
  Ssid ssid2 = Ssid("ns-3-ssid-2");

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid1),
              "BeaconInterval", TimeValue(Seconds(0.1)));
  NetDeviceContainer ap1Device;
  ap1Device = wifi.Install(phy, mac, apNodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid2),
              "BeaconInterval", TimeValue(Seconds(0.1)));
  NetDeviceContainer ap2Device;
  ap2Device = wifi.Install(phy, mac, apNodes.Get(1));


  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid1),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staAp1Device;
  staAp1Device = wifi.Install(phy, mac, staNodesAp1);

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid2),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staAp2Device;
  staAp2Device = wifi.Install(phy, mac, staNodesAp2);


  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodesAp1);

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(50.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

   mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodesAp2);


  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(25.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(50.0),
                                "GridWidth", UintegerValue(1),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);

  InternetStackHelper internet;
  internet.Install(apNodes);
  internet.Install(staNodesAp1);
  internet.Install(staNodesAp2);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ap1Interface;
  ap1Interface = address.Assign(ap1Device);
  Ipv4InterfaceContainer staAp1Interfaces = address.Assign(staAp1Device);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ap2Interface;
  ap2Interface = address.Assign(ap2Device);
  Ipv4InterfaceContainer staAp2Interfaces = address.Assign(staAp2Device);


  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(apNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(simulationTime));

  UdpClientHelper client(ap1Interface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numStaAp1; ++i) {
    clientApps.Add(client.Install(staNodesAp1.Get(i)));
  }
  for (uint32_t i = 0; i < numStaAp2; ++i) {
    clientApps.Add(client.Install(staNodesAp2.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  if (tracing) {
    phy.EnablePcapAll("wifi-example");
  }

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi-example.tr"));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  uint64_t totalBytesSent[numStaAp1 + numStaAp2];
  for(uint32_t i = 0; i < numStaAp1 + numStaAp2; ++i){
    totalBytesSent[i] = 0;
  }

  for(uint32_t i = 0; i < numStaAp1; ++i){
      Ptr<Application> app = clientApps.Get(i);
      Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
      totalBytesSent[i] = client->GetTotalBytesSent();
      std::cout << "Station " << i+1 << " (AP1) sent " << totalBytesSent[i] << " bytes" << std::endl;
  }
    for(uint32_t i = 0; i < numStaAp2; ++i){
      Ptr<Application> app = clientApps.Get(i+numStaAp1);
      Ptr<UdpClient> client = DynamicCast<UdpClient>(app);
      totalBytesSent[i+numStaAp1] = client->GetTotalBytesSent();
      std::cout << "Station " << i+1 << " (AP2) sent " << totalBytesSent[i+numStaAp1] << " bytes" << std::endl;
  }


  Simulator::Destroy();
  return 0;
}