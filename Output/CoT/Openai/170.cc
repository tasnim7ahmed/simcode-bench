#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

uint32_t g_rxPackets = 0;
uint64_t g_rxBytes = 0;

void ReceivePacket(Ptr<const Packet> packet, const Address &address)
{
  g_rxPackets++;
  g_rxBytes += packet->GetSize();
}

void PrintResults()
{
  std::cout << "Total received packets: " << g_rxPackets << std::endl;
  std::cout << "Throughput (Mbps): " << (g_rxBytes * 8.0 / 1e6 / 10.0) << std::endl;
}

int main(int argc, char *argv[])
{
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  // Wifi config
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("DsssRate11Mbps"),
                              "ControlMode", StringValue("DsssRate11Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-80211b");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  // Mobility
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

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  // TCP server (packet sink) on STA 1
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(staInterfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install(wifiStaNodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  // Trace packet receptions
  serverApp.Get(0)->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacket));

  // TCP client on STA 0
  OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute("DataRate", StringValue("11Mbps"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
  clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
  ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(0));

  // Enable pcap tracing
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
  phy.EnablePcap("ap", apDevice.Get(0));
  phy.EnablePcap("sta1", staDevices.Get(0));
  phy.EnablePcap("sta2", staDevices.Get(1));

  Simulator::Schedule(Seconds(10.0), &PrintResults);
  Simulator::Stop(Seconds(10.01));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}