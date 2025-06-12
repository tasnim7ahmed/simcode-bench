#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include <iostream>
#include <iomanip>

using namespace ns3;

struct StaStats {
  uint32_t packetsTx = 0;
  uint32_t bytesRx = 0;
};

StaStats g_staStats[2];

void
TxTrace(uint32_t staIdx, Ptr<const Packet> packet)
{
  g_staStats[staIdx].packetsTx++;
}

void
RxTrace(uint32_t staIdx, Ptr<const Packet> packet, const Address &from)
{
  g_staStats[staIdx].bytesRx += packet->GetSize();
}

int
main(int argc, char *argv[])
{
  double simulationTime = 10.0;

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-80211g");

  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(10.0),
                               "GridWidth", UintegerValue(3),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiApNode);
  stack.Install(wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 9;
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < 2; ++i)
  {
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(staInterfaces.GetAddress(i), port));
    ApplicationContainer app = sinkHelper.Install(wifiStaNodes.Get(i));
    app.Start(Seconds(0.0));
    app.Stop(Seconds(simulationTime));
    sinkApps.Add(app);
  }

  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < 2; ++i)
  {
    AddressValue remoteAddress(
      InetSocketAddress(staInterfaces.GetAddress(i), port));
    onoff.SetAttribute("Remote", remoteAddress);
    ApplicationContainer app = onoff.Install(wifiApNode.Get(0));
    app.Start(Seconds(1.0 + i));
    app.Stop(Seconds(simulationTime));
    clientApps.Add(app);

    Ptr<NetDevice> dev = staDevices.Get(i);
    dev->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&TxTrace, i));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(i));
    sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTrace, i));
  }

  phy.EnablePcap("wifi-ap", apDevice);
  phy.EnablePcap("wifi-sta1", staDevices.Get(0));
  phy.EnablePcap("wifi-sta2", staDevices.Get(1));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  std::cout << std::fixed << std::setprecision(2);
  for (uint32_t i = 0; i < 2; ++i)
  {
    double throughput = g_staStats[i].bytesRx * 8.0 / simulationTime / 1e6; // Mbps
    std::cout << "Station " << i+1 << ":" << std::endl;
    std::cout << "  Packets transmitted: " << g_staStats[i].packetsTx << std::endl;
    std::cout << "  Total received bytes: " << g_staStats[i].bytesRx << std::endl;
    std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
  }

  Simulator::Destroy();
  return 0;
}