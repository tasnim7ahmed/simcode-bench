#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateControlComparison");

// Helper function to get current transmit power of STA
double
GetStaTxPower (Ptr<WifiNetDevice> staDevice)
{
  Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy> (staDevice->GetPhy ());
  if (phy)
    {
      return phy->GetTxPowerStart ();
    }
  return 0.0;
}

// Helper function to get current MCS/rate
std::string
GetStaTxMode (Ptr<WifiNetDevice> staDevice)
{
  Ptr<WifiMac> mac = staDevice->GetMac ();
  if (mac)
    {
      Ptr<RegularWifiMac> rmac = DynamicCast<RegularWifiMac> (mac);
      if (rmac)
        {
          // Modulation mode is not directly accessible. We can try to print out the current rate control algorithm's info.
          // Output as placeholder: actual implementation may require patching ns-3.
          // Here we'll just return an empty string.
        }
    }
  return "";
}

int
main (int argc, char *argv[])
{
  std::string rateManager = "ns3::ParfWifiManager";
  uint32_t rtsThreshold = 2347;
  std::string throughputOutput = "throughput.plt";
  std::string txpowerOutput = "txpower.plt";
  std::string logOutput = "rate_txpower.log";
  double simTime = 60.0; // seconds (node will reach 60m distance)
  double dataRateMbps = 54.0;
  uint32_t packetSize = 1472; // Bytes

  CommandLine cmd;
  cmd.AddValue ("rateManager", "Wifi rate control manager: ns3::ParfWifiManager, ns3::AparfWifiManager, or ns3::RrpaaWifiManager", rateManager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold (bytes)", rtsThreshold);
  cmd.AddValue ("throughputOutput", "Throughput gnuplot output filename", throughputOutput);
  cmd.AddValue ("txpowerOutput", "Tx power gnuplot output filename", txpowerOutput);
  cmd.AddValue ("logOutput", "Detailed log of rate/txpower", logOutput);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);
  Ptr<Node> apNode = wifiNodes.Get (0);
  Ptr<Node> staNode = wifiNodes.Get (1);

  // Wifi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (16.0206)); // 40 mW = 16.02 dBm
  phy.Set ("TxPowerEnd", DoubleValue (16.0206));
  phy.Set ("TxPowerLevels", UintegerValue (1)); // Single power level by default

  // Wifi MAC and devices
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager (rateManager, "RtsCtsThreshold", UintegerValue (rtsThreshold));

  Ssid ssid = Ssid ("wifi-ssid");

  WifiMacHelper mac;
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDeviceContainer = wifi.Install (phy, mac, staNode);

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDeviceContainer = wifi.Install (phy, mac, apNode);

  // Mobility: AP fixed, STA moves away by 1m per second
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP at origin
  positionAlloc->Add (Vector (1.0, 0.0, 0.0)); // Initial STA position
  mobility.SetPositionAllocator (positionAlloc);

  // AP is stationary
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  // STA with ConstantVelocity
  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (staNode);

  Ptr<ConstantVelocityMobilityModel> staMob = staNode->GetObject<ConstantVelocityMobilityModel> ();
  staMob->SetVelocity (Vector (1.0, 0.0, 0.0)); // 1 m/s, in x direction

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (apDeviceContainer, staDeviceContainer));

  // Install UDP CBR server on STA, client on AP
  uint16_t port = 50000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (staNode);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (0)); // unlimited
  client.SetAttribute ("Interval", TimeValue (Seconds (packetSize * 8.0 / (dataRateMbps * 1e6)))); // cbr at 54 Mbps
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (apNode);
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simTime));

  // Data collectors for plotting
  std::vector<double> distances;
  std::vector<double> throughputKbps;
  std::vector<double> txPowerDbm;

  // Open log file
  std::ofstream logFile;
  logFile.open (logOutput, std::ios::out | std::ios::trunc);
  logFile << "Time(s)\tDistance(m)\tTxPower(dBm)\tPacketsReceived\tThroughput(Kbps)\n";

  // Calculate and log throughput & txpower at each second
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
  uint64_t lastRx = 0;

  Ptr<NetDevice> staDev = staDeviceContainer.Get (0);
  Ptr<WifiNetDevice> wifiStaDev = DynamicCast<WifiNetDevice> (staDev);

  for (uint32_t t = 1; t <= uint32_t(simTime); ++t)
    {
      Simulator::Schedule (Seconds (t), [&, t, udpServer, wifiStaDev, staMob, &logFile, &distances, &throughputKbps, &txPowerDbm, &lastRx]()
      {
        Vector apPos = Vector (0.0, 0.0, 0.0);
        Vector staPos = staMob->GetPosition ();
        double distance = std::sqrt (std::pow (staPos.x - apPos.x, 2) + std::pow (staPos.y - apPos.y, 2));
        uint64_t totalRx = udpServer->GetReceived ();
        uint64_t pktsRx = totalRx - lastRx;
        lastRx = totalRx;
        double throughput = (pktsRx * 8.0 * 1472) / 1000.0; // kbps over last 1s

        double txPower = GetStaTxPower (wifiStaDev);
        //std::string rateStr = GetStaTxMode (wifiStaDev);

        distances.push_back (distance);
        throughputKbps.push_back (throughput);
        txPowerDbm.push_back (txPower);

        logFile << t << "\t" << distance << "\t" << txPower << "\t"
                << pktsRx << "\t" << throughput << "\n";
      });
    }

  Simulator::Stop (Seconds (simTime + 0.1));
  Simulator::Run ();
  Simulator::Destroy ();

  logFile.close ();

  // Output Gnuplot files
  Gnuplot throughputPlot (throughputOutput, "Throughput vs. Distance");
  throughputPlot.SetTerminal ("png");
  throughputPlot.SetLegend ("Distance (m)", "Throughput (kbps)");
  Gnuplot2dDataset thrData;
  thrData.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  for (size_t i = 0; i < distances.size (); ++i)
    {
      thrData.Add (distances[i], throughputKbps[i]);
    }
  throughputPlot.AddDataset (thrData);
  std::ofstream thrPlotStream (throughputOutput);
  throughputPlot.GenerateOutput (thrPlotStream);
  thrPlotStream.close ();

  Gnuplot txPowerPlot (txpowerOutput, "Transmit Power vs. Distance");
  txPowerPlot.SetTerminal ("png");
  txPowerPlot.SetLegend ("Distance (m)", "TxPower (dBm)");
  Gnuplot2dDataset txpData;
  txpData.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  for (size_t i = 0; i < distances.size (); ++i)
    {
      txpData.Add (distances[i], txPowerDbm[i]);
    }
  txPowerPlot.AddDataset (txpData);
  std::ofstream txpPlotStream (txpowerOutput);
  txPowerPlot.GenerateOutput (txpPlotStream);
  txpPlotStream.close ();

  return 0;
}