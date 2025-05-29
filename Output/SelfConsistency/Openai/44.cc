#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/propagation-loss-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FixedRssWifiExample");

int
main (int argc, char *argv[])
{
  double rss = -80; // dBm
  uint32_t packetSize = 1024; // bytes
  uint32_t numPackets = 10;
  double interval = 1.0; // seconds
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("rss", "Received signal strength in dBm", rss);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
      LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("FixedRssWifiExample", LOG_LEVEL_INFO);
      LogComponentEnable ("InterferenceHelper", LOG_LEVEL_ALL);
    }

  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Set up Wi-Fi PHY and MAC
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  // Apply FixedRssLossModel
  Ptr<FixedRssLossModel> fixedRss = CreateObject<FixedRssLossModel> ();
  fixedRss->SetRss (rss);
  channel.SetPropagationLossModel (fixedRss);
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;

  Ssid ssid = Ssid ("fixed-rss-ssid");

  // Configure AP
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (wifiPhy, wifiMac, wifiNodes.Get (1));

  // Configure STA
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (wifiPhy, wifiMac, wifiNodes.Get (0));

  // Set mobility (nodes do not move)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // Install the internet stack
  InternetStackHelper internet;
  internet.Install (wifiNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  NetDeviceContainer allDevices;
  allDevices.Add (staDevice);
  allDevices.Add (apDevice);
  interfaces = address.Assign (allDevices);

  // Install applications: Station sends to the AP
  uint16_t sinkPort = 50000;

  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (wifiNodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds ((numPackets + 1) * interval));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  onoff.SetAttribute ("MaxBytes", UintegerValue (packetSize * numPackets));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app = onoff.Install (wifiNodes.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds ((numPackets + 1) * interval));

  // Schedule the sending of packets at the specified interval
  Ptr<Application> appPtr = app.Get (0);
  Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication> (appPtr);
  if (onoffApp)
    {
      onoffApp->SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoffApp->SetAttribute ("StopTime", TimeValue (Seconds ((numPackets + 1) * interval)));
    }

  // Enable pcap tracing
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap ("fixed-rss-sta", staDevice.Get (0), true, true);
  wifiPhy.EnablePcap ("fixed-rss-ap", apDevice.Get (0), true, true);

  // Print info
  NS_LOG_INFO ("RSS set to " << rss << " dBm");
  NS_LOG_INFO ("Sending " << numPackets << " packets of " << packetSize << " bytes each, interval " << interval << "s");

  Simulator::Stop (Seconds ((numPackets + 2) * interval));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}