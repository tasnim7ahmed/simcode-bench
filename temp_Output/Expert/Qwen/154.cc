#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketTransmissionDataset");

class PacketLogger
{
public:
  static void LogPacket(Ptr<const Packet> packet, const Address &from, const Address &to)
  {
    std::ofstream &os = GetOutputStream();
    if (!os.is_open())
      return;

    double txTime = Now().GetSeconds();
    os << from << "," << to << "," << packet->GetSize() << "," << txTime << ",0\n";
  }

  static void LogRx(Ptr<const Packet> packet, const Address &from)
  {
    std::ofstream &os = GetOutputStream();
    if (!os.is_open())
      return;

    double rxTime = Now().GetSeconds();
    std::string line;
    std::ifstream is("temp.csv");
    std::ofstream temp("temp_new.csv");

    bool found = false;
    while (std::getline(is, line))
    {
      std::istringstream iss(line);
      std::string src, dst, size, txStr, rxStr;
      std::getline(iss, src, ',');
      std::getline(iss, dst, ',');
      std::getline(iss, size, ',');
      std::getline(iss, txStr, ',');
      std::getline(iss, rxStr, ',');

      if (rxStr == "0" && from == Address::ConvertFrom(dst))
      {
        temp << src << "," << dst << "," << size << "," << txStr << "," << rxTime << "\n";
        found = true;
      }
      else
      {
        temp << line << "\n";
      }
    }

    is.close();
    temp.close();

    if (found)
    {
      std::remove("temp.csv");
      std::rename("temp_new.csv", "temp.csv");
    }
    else
    {
      std::remove("temp_new.csv");
    }
  }

private:
  static std::ofstream &GetOutputStream()
  {
    static std::ofstream os("temp.csv", std::ios::out | std::ios::trunc);
    return os;
  }
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> wifiChannel = channel.Create();
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiChannel, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
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
  ApplicationContainer serverApp = server.Install(nodes.Get(4));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  for (uint32_t i = 0; i < 4; ++i)
  {
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(i));
    clientApp.Start(Seconds(2.0 + i));
    clientApp.Stop(Seconds(10.0));
  }

  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&PacketLogger::LogPacket));
  Config::Connect("/NodeList/4/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&PacketLogger::LogRx));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  std::ofstream finalOut("packet_dataset.csv");
  finalOut << "Source,Destination,Size,TransmitTime,ReceiveTime\n";

  std::ifstream in("temp.csv");
  std::string line;
  while (std::getline(in, line))
  {
    finalOut << line << "\n";
  }

  finalOut.close();
  std::remove("temp.csv");

  return 0;
}