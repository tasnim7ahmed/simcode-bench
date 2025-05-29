#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-echo-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiStaticPosition");

int
main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  // Set up logging verbosity
  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  // Create nodes
  NodeContainer apNode;
  apNode.Create (1);
  NodeContainer staNode;
  staNode.Create (1);

  // Configure Wifi PHY
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Configure Wifi MAC
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-wifi");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  // Configure Mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::ListPositionAllocator",
                                 "List", AttributeValue (StringValue (
                                                          "0 0 0|5.0 0 0"))); // AP at (0,0,0), STA at (5,0,0)
  mobility.Install (NodeContainer (apNode, staNode));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (NodeContainer (apNode, staNode));

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  // Create applications
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (apInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (staNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));


  // Enable Tracing
  if (tracing)
    {
      phy.EnablePcap ("wifi-static-position", apDevice);
      phy.EnablePcap ("wifi-static-position", staDevice);
    }

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}