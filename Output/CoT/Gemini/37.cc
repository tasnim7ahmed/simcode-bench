#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/stats-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateComparison");

int main (int argc, char *argv[])
{
  bool verbose = false;
  std::string rateControl = "ParfWifiManager";
  std::string rtsThreshold = "2347";
  std::string throughputFilename = "throughput.dat";
  std::string powerFilename = "power.dat";
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("rateControl", "Rate control algorithm [ParfWifiManager, AparfWifiManager, RrpaaWifiManager]", rateControl);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
  cmd.AddValue ("throughputFilename", "Filename for throughput data", throughputFilename);
  cmd.AddValue ("powerFilename", "Filename for transmit power data", powerFilename);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiRateComparison", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer apNode = NodeContainer (nodes.Get (0));
  NodeContainer staNode = NodeContainer (nodes.Get (1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  std::string rateManagerString = "ns3::" + rateControl;
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (rtsThreshold));
  wifi.SetRemoteStationManager (rateManagerString, "DataMode", StringValue ("OfdmRate54Mbps"), "ControlMode", StringValue ("OfdmRate54Mbps"));

  Ssid ssid = Ssid ("ns3-wifi");

  WifiMacHelper mac;

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, staNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (staNode);
  Ptr<ConstantVelocityMobilityModel> staMob = staNode.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  staMob->SetVelocity (Vector (1, 0, 0));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (apDevice);
  interfaces = address.Assign (staDevice);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (apNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (0xffffffff));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.00002))); // CBR 54Mbps
  echoClient.SetAttribute ("PacketSize", UintegerValue (1140));

  ApplicationContainer clientApps = echoClient.Install (staNode);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Schedule (Seconds (0.1), [&](){
    std::ofstream powerStream;
    powerStream.open (powerFilename.c_str());
    powerStream << "#Time\tDistance\tTxPower\tRate\n";

    std::ofstream throughputStream;
    throughputStream.open (throughputFilename.c_str());
    throughputStream << "#Time\tDistance\tThroughput\n";

    Simulator::Schedule (Seconds (0.2), [&,powerStream,throughputStream](){

      for (double time = 0.2; time <= simulationTime; time += 0.1) {
        Simulator::Schedule (Seconds (time), [&,time,powerStream,throughputStream]() {
          Ptr<MobilityModel> mobilityModel = staNode.Get (0)->GetObject<MobilityModel> ();
          double distance = mobilityModel->GetDistanceFromReference ();
          Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(staDevice.Get(0));
          WifiMode currentMode = wifiNetDevice->GetRemoteStationManager()->GetDataMode();
          double txPower = wifiNetDevice->GetPhy()->GetTxPowerLevel();

          throughputStream << time << "\t" << distance << "\t" << 0 << "\n";
          powerStream << time << "\t" << distance << "\t" << txPower << "\t" << currentMode.GetName() << "\n";
        });
      }
    });
  });

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress == "10.1.1.2" && t.destinationAddress == "10.1.1.1"))
        {
		  totalThroughput = (i->second.rxBytes * 8.0) / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          NS_LOG_UNCOND ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n");
          NS_LOG_UNCOND ("  Tx Packets: " << i->second.txPackets);
          NS_LOG_UNCOND ("  Rx Packets: " << i->second.rxPackets);
          NS_LOG_UNCOND ("  Lost Packets: " << i->second.lostPackets);
          NS_LOG_UNCOND ("  Throughput: " << totalThroughput << " Mbps");
        }
    }

  monitor->SerializeToXmlFile("wifi_rate_comparison.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}