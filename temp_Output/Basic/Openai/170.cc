#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

static uint32_t g_rxBytes = 0;
static uint32_t g_rxPackets = 0;

void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv()))
    {
      g_rxBytes += packet->GetSize();
      g_rxPackets++;
    }
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("DsssRate11Mbps"),
                              "ControlMode", StringValue("DsssRate11Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(0.0),
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
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  phy.EnablePcap("wifi-ap", apDevice.Get(0));
  phy.EnablePcap("wifi-sta1", staDevices.Get(0));
  phy.EnablePcap("wifi-sta2", staDevices.Get(1));

  uint16_t port = 9;
  // STA 1 is the server; STA 0 is the client
  Address serverAddress(InetSocketAddress(staInterfaces.GetAddress(1), port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<Socket> sinkSocket = Socket::CreateSocket(wifiStaNodes.Get(1), TcpSocketFactory::GetTypeId());
  sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
  sinkSocket->Listen();
  sinkSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

  OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
  ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  double throughput = (g_rxBytes * 8.0) / 8 / 1024 / (10.0 - 1.0);
  std::cout << "Server received " << g_rxPackets << " packets." << std::endl;
  std::cout << "Server received " << g_rxBytes << " bytes." << std::endl;
  std::cout << "Average throughput at server: " << throughput << " KB/s" << std::endl;

  Simulator::Destroy();
  return 0;
}