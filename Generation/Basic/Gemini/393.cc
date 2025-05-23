#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

// NS_LOG_COMPONENT_DEFINE ("TwoNodeWifiUdp"); // Optional: for logging

int main (int argc, char *argv[])
{
  // Set default values for simulation parameters
  // Time simulation runs (seconds)
  double simulationTime = 10.0;
  // UDP packet size (bytes)
  uint32_t packetSize = 1024;
  // Number of UDP packets to send
  uint32_t numPackets = 10;
  // Time interval between packets (seconds)
  double interPacketInterval = 1.0;
  // Port for UDP applications
  uint16_t port = 9;

  // Optional: Allow command line arguments to override defaults
  // CommandLine cmd (__FILE__);
  // cmd.AddValue ("simulationTime", "Total duration of the simulation", simulationTime);
  // cmd.AddValue ("packetSize", "Size of UDP packets", packetSize);
  // cmd.AddValue ("numPackets", "Number of UDP packets to send", numPackets);
  // cmd.AddValue ("interPacketInterval", "Time interval between packets", interPacketInterval);
  // cmd.Parse (argc, argv);

  // NS_LOG_INFO ("Creating two nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  // Configure Mobility Model
  // NS_LOG_INFO ("Setting up mobility.");
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set positions for the nodes
  Ptr<ConstantPositionMobilityModel> pos0 = nodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  pos0->SetPosition (Vector (0.0, 0.0, 0.0));
  Ptr<ConstantPositionMobilityModel> pos1 = nodes.Get (1)->GetObject<ConstantPositionMobilityModel> ();
  pos1->SetPosition (Vector (10.0, 0.0, 0.0)); // Node 1 is 10m away from Node 0

  // Configure Wi-Fi devices
  // NS_LOG_INFO ("Setting up Wi-Fi devices.");
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n); // Using 802.11n standard

  YansWifiPhyHelper phy;
  phy.SetChannel (YansWifiChannelHelper::Default ().Create ());
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (Ssid ("my-ssid"))); // Ad-hoc (IBSS) mode

  NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  // Install Internet stack
  // NS_LOG_INFO ("Installing Internet stack.");
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  // NS_LOG_INFO ("Assigning IP addresses.");
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up UDP applications
  // NS_LOG_INFO ("Setting up UDP applications.");

  // Node 1 (Receiver): Packet Sink application
  // The PacketSinkHelper is a generic application that receives traffic.
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  // Node 0 (Sender): UDP Client application
  // The UdpClientHelper sends UDP packets.
  UdpClientHelper clientHelper (interfaces.GetAddress (1), port); // Send to Node 1's IP address
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0)); // Start sending after 1 second
  clientApp.Stop (Seconds (simulationTime));

  // Run simulation
  // NS_LOG_INFO ("Running simulation.");
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  // NS_LOG_INFO ("Done.");

  return 0;
}