#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInfraUdpThroughput");

void
CalculateThroughput (Ptr<Application> app, uint64_t *lastTotalRx, Time interval)
{
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
  uint64_t curRx = sink->GetTotalRx ();
  double throughput = ((curRx - *lastTotalRx) * 8.0) / (interval.GetSeconds () * 1e6); // Mbps
  std::cout << Simulator::Now ().GetSeconds ()
            << "s: Throughput: " << throughput << " Mbps" << std::endl;
  *lastTotalRx = curRx;
  Simulator::Schedule (interval, &CalculateThroughput, app, lastTotalRx, interval);
}

int
main (int argc, char *argv[])
{
  uint32_t nSta = 4;
  double simulationTime = 10.0; // seconds
  uint32_t packetSize = 1024; // bytes
  uint32_t nPackets = 10000;
  double interval = 0.001; // seconds, ~8.192 Mbps stream

  CommandLine cmd;
  cmd.AddValue ("nSta", "Number of wifi STA devices", nSta);
  cmd.Parse (argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  channel.Set ("Delay", TimeValue (MilliSeconds (2)));

  Ptr<YansWifiChannel> phyChannel = channel.Create ();

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (phyChannel);
  phy.Set ("TxGain", DoubleValue (0));
  phy.Set ("RxGain", DoubleValue (0));
  phy.SetErrorRateModel ("ns3::NistErrorRateModel");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("ErpOfdmRate54Mbps"),
                               "ControlMode", StringValue ("ErpOfdmRate6Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-infra");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // UDP server on STA 0
  uint16_t serverPort = 9;
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install (wifiStaNodes.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simulationTime));

  // UDP client on STA 1
  UdpClientHelper udpClient (staInterfaces.GetAddress (0), serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (1));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simulationTime));

  phy.EnablePcap ("wifi-infra", apDevice.Get (0));
  phy.EnablePcap ("wifi-infra-sta0", staDevices.Get (0));
  phy.EnablePcap ("wifi-infra-sta1", staDevices.Get (1));
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-infra.tr"));

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
  uint64_t lastTotalRx = 0;
  Simulator::Schedule (Seconds (2.1), &CalculateThroughput, serverApp.Get (0), &lastTotalRx, Seconds (1.0));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}