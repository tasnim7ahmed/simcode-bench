#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketDatasetSimulation");

class PacketLogger
{
public:
  static void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, uint64_t txTime, uint64_t rxTime)
  {
    static std::ofstream csvFile("packet_dataset.csv", std::ios::out);
    if (csvFile.is_open())
    {
      csvFile << from << "," << to << "," << packet->GetSize() << "," << txTime << "," << rxTime << "\n";
    }
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);
  NodeContainer serverNode = NodeContainer(nodes.Get(4));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(serverNode);
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < 4; ++i)
  {
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(i));
    clientApp.Start(Seconds(2.0 + i * 0.5));
    clientApp.Stop(Seconds(10.0));
  }

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi.tr");
  phy.EnableAsciiAll(stream);

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeBoundCallback(&PacketLogger::LogPacket));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}