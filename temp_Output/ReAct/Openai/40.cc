#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiFixedRssExample");

int
main (int argc, char *argv[])
{
  // Simulation parameters with defaults
  double rss = -80.0; // dBm
  double rxThreshold = -81.0; // dBm
  uint32_t packetSize = 1000;
  bool verbose = false;
  std::string dataRate = "1Mbps";
  double startTime = 1.0; // seconds
  double stopTime = 5.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("rss", "Fixed RSS value in dBm", rss);
  cmd.AddValue ("rxThreshold", "Receiver sensitivity threshold in dBm", rxThreshold);
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("verbose", "Enable verbose logging", verbose);
  cmd.AddValue ("dataRate", "ConstantDataRate Application data rate", dataRate);
  cmd.AddValue ("startTime", "Application start time (s)", startTime);
  cmd.AddValue ("stopTime", "Simulation stop time (s)", stopTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("AdhocWifiFixedRssExample", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("YansWifiPhy", LOG_LEVEL_INFO);
    }

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set mobility (fixed positions)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Configure WiFi - Ad-hoc, 802.11b
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                               "DataMode", StringValue ("DsssRate1Mbps"),
                               "ControlMode", StringValue ("DsssRate1Mbps"));

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  // Fixed RSS model setup
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set ("RxGain", DoubleValue (0.0));
  phy.Set ("RxSensitivity", DoubleValue (rxThreshold));
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  Ptr<FixedRssLossModel> rssLossModel = CreateObject<FixedRssLossModel> ();
  rssLossModel->SetRss (rss);

  Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel> ();
  channel->SetPropagationLossModel (rssLossModel);
  channel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());

  phy.SetChannel (channel);

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // Internet stack and IP assignment
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP echo server and client
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApp = echoServer.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (stopTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = echoClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (startTime));
  clientApp.Stop (Seconds (stopTime));

  // Enable pcap tracing
  phy.EnablePcap ("adhoc-wifi-fixedrss", devices);

  // Print simulation settings
  NS_LOG_INFO ("Simulation settings:");
  NS_LOG_INFO ("  RSS = " << rss << " dBm");
  NS_LOG_INFO ("  Rx SNR Threshold = " << rxThreshold << " dBm");
  NS_LOG_INFO ("  Packet size = " << packetSize << " bytes");

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}