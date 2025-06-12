#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool enableFlowMonitor = false;
  std::string protocol = "AODV";
  uint32_t numNodes = 50;
  double simulationTime = 200;
  double maxSpeed = 20;
  double areaWidth = 300;
  double areaHeight = 1500;
  double txPowerDbm = 7.5;
  bool enableNetAnim = false;

  CommandLine cmd;
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("flowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("protocol", "Routing protocol (DSDV, AODV, OLSR, DSR)", protocol);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("maxSpeed", "Maximum speed of nodes in m/s", maxSpeed);
  cmd.AddValue ("areaWidth", "Width of the simulation area", areaWidth);
  cmd.AddValue ("areaHeight", "Height of the simulation area", areaHeight);
  cmd.AddValue ("txPowerDbm", "WiFi transmission power in dBm", txPowerDbm);
  cmd.AddValue ("netanim", "Enable NetAnim visualization", enableNetAnim);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaWidth) + "]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaHeight) + "]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(maxSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]");
  mobility.Install (nodes);

  InternetStackHelper internet;
  if (protocol == "OLSR") {
    OlsrHelper olsr;
    internet.SetRoutingHelper (olsr);
  } else if (protocol == "DSDV") {
    DsdvHelper dsdv;
    internet.SetRoutingHelper (dsdv);
  } else if (protocol == "AODV") {
    AodvHelper aodv;
    internet.SetRoutingHelper (aodv);
  } else if (protocol == "DSR") {
    DsrHelper dsr;
    DsrMainRoutingHelper dsrRouting;
    internet.SetRoutingHelper (dsrRouting);
  } else {
    std::cerr << "Error: Unknown routing protocol " << protocol << std::endl;
    return 1;
  }
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  uint16_t port = 9;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < 10; ++i) {
    UdpServerHelper echoServer (port);
    serverApps.Add (echoServer.Install (nodes.Get (i)));

    UdpClientHelper echoClient (i.GetAddress (0), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    clientApps.Add (echoClient.Install (nodes.Get (i + 10)));
    port++;
  }

  serverApps.Start (Seconds (1.0));
  clientApps.Start (Seconds (50 + (double)rand() / RAND_MAX));

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("manet");
    }

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
    {
      flowmon.InstallAll ();
    }

  if (enableNetAnim) {
      AnimationInterface anim ("manet-animation.xml");
      anim.SetMaxPktsPerTraceFile(100000000);
  }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  if (enableFlowMonitor)
    {
      flowmon.SerializeToXmlFile ("manet.flowmon", false, true);
    }

  uint64_t totalBytesReceived = 0;
  for (uint32_t i = 0; i < 10; ++i) {
      Ptr<Application> serverApp = serverApps.Get(i);
      Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp);
      totalBytesReceived += udpServer->GetReceived();
  }

  double throughput = (totalBytesReceived * 8.0) / (simulationTime * 1000000.0);

  std::ofstream resultsFile;
  resultsFile.open ("results.csv", std::ios::app);
  resultsFile << protocol << ","
              << numNodes << ","
              << maxSpeed << ","
              << txPowerDbm << ","
              << throughput << std::endl;
  resultsFile.close ();

  Simulator::Destroy ();
  return 0;
}