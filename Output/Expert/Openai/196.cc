#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

struct StaStats
{
  uint32_t packetsReceived = 0;
  uint64_t bytesReceived = 0;
  Time firstRx;
  Time lastRx;

  StaStats() : packetsReceived(0), bytesReceived(0), firstRx(Seconds(0)), lastRx(Seconds(0)) {}
};

std::vector<StaStats> staStats;

void ReceivePacket(Ptr<Socket> socket, uint32_t staIndex)
{
  while (Ptr<Packet> packet = socket->Recv())
    {
      if (staStats[staIndex].packetsReceived == 0)
        staStats[staIndex].firstRx = Simulator::Now();
      staStats[staIndex].packetsReceived++;
      staStats[staIndex].bytesReceived += packet->GetSize();
      staStats[staIndex].lastRx = Simulator::Now();
    }
}

int main(int argc, char *argv[])
{
  uint32_t nSta = 2;
  double simulationTime = 10.0;
  std::string phyMode = "HtMcs7";
  uint32_t payloadSize = 1024;
  DataRate dataRate("10Mbps");

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(10.0),
                               "GridWidth", UintegerValue(3),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 9;

  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      UdpServerHelper server(port + i);
      serverApps.Add(server.Install(wifiStaNodes.Get(i)));
    }
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      UdpClientHelper client(staInterfaces.GetAddress(i), port + i);
      client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
      client.SetAttribute("Interval", TimeValue(Seconds(double(payloadSize * 8) / double(dataRate.GetBitRate()))));
      client.SetAttribute("PacketSize", UintegerValue(payloadSize));
      clientApps.Add(client.Install(wifiApNode.Get(0)));
    }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime + 1));

  phy.EnablePcap("wifi-ap", apDevice.Get(0));
  phy.EnablePcap("wifi-sta1", staDevices.Get(0));
  phy.EnablePcap("wifi-sta2", staDevices.Get(1));

  staStats.resize(nSta);
  for (uint32_t i = 0; i < nSta; ++i)
    {
      Ptr<Socket> recvSocket = Socket::CreateSocket(wifiStaNodes.Get(i), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port + i);
      recvSocket->Bind(local);
      recvSocket->SetRecvCallback(MakeBoundCallback(&ReceivePacket, i));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(simulationTime + 1));
  Simulator::Run();

  for (uint32_t i = 0; i < nSta; ++i)
    {
      double throughput = 0.0;
      if (staStats[i].lastRx > staStats[i].firstRx && staStats[i].packetsReceived > 0)
        {
          throughput = (staStats[i].bytesReceived * 8.0) / (staStats[i].lastRx.GetSeconds() - staStats[i].firstRx.GetSeconds()) / 1e6;
        }
      std::cout << "STA" << i + 1 << ": Packets Received: " << staStats[i].packetsReceived
                << ", Bytes Received: " << staStats[i].bytesReceived
                << ", Throughput: " << throughput << " Mbps" << std::endl;
    }

  Simulator::Destroy();
  return 0;
}