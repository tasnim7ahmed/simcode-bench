#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketTransmissionDataset");

class PacketLogger
{
public:
  PacketLogger();
  void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime);

private:
  std::ofstream m_csvFile;
};

PacketLogger::PacketLogger()
{
  m_csvFile.open("packet_dataset.csv");
  m_csvFile << "SourceNode,DestinationNode,PacketSize,TransmissionTime,ReceptionTime\n";
}

void
PacketLogger::LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime)
{
  Ipv4Address fromIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
  Ipv4Address toIp = InetSocketAddress::ConvertFrom(to).GetIpv4();

  int srcNodeId = -1;
  int dstNodeId = -1;

  for (int i = 0; i < 5; ++i)
  {
    if (Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()) == fromIp)
      srcNodeId = i;
    if (Ipv4Address(("10.1.1." + std::to_string(i + 1)).c_str()) == toIp)
      dstNodeId = i;
  }

  m_csvFile << srcNodeId << ","
            << dstNodeId << ","
            << packet->GetSize() << ","
            << txTime.GetSeconds() << ","
            << rxTime.GetSeconds() << "\n";
}

static void
RxTrace(Ptr<const Packet> packet, const Address &from, const Address &to, Time txTime, Time rxTime, PacketLogger *logger)
{
  logger->LogPacket(packet, from, to, txTime, rxTime);
}

int
main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(5);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  PacketLogger logger;

  // Server on node 4
  UdpServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(4));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Connect trace to capture received packets
  Config::ConnectWithoutContext("/NodeList/4/ApplicationList/0/$ns3::UdpServer/Rx", MakeBoundCallback(&RxTrace, &logger));

  // Clients on nodes 0-3 sending to node 4
  for (uint32_t i = 0; i < 4; ++i)
  {
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(i));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}