#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/propagation-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMpduAggregationHiddenNode");

uint64_t g_rxBytes = 0;

void RxCallback (Ptr<const Packet> packet, const Address &address)
{
  g_rxBytes += packet->GetSize ();
}

int main (int argc, char *argv[])
{
  // Command line arguments
  bool enableRtsCts = false;
  uint32_t maxAmpdu = 32;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double expectedThroughputMbps = 0.0;
  double throughputTolerance = 5.0;

  CommandLine cmd;
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS (default: false)", enableRtsCts);
  cmd.AddValue ("maxAmpdu", "Number of aggregated MPDUs per AMSDU (default: 32)", maxAmpdu);
  cmd.AddValue ("payloadSize", "Application payload size in bytes (default: 1472)", payloadSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds (default: 10)", simulationTime);
  cmd.AddValue ("expectedThroughputMbps", "Expected throughput in Mbps (default: 0 = skip check)", expectedThroughputMbps);
  cmd.AddValue ("throughputTolerance", "Throughput check tolerance in percent (default: 5)", throughputTolerance);
  cmd.Parse (argc, argv);

  // Wifi physical & standard limits, 5m range
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  // Set up to limit comm range ~5m due to path loss
  Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
  lossModel->SetReference (1.0, 0.0); // at 1m, 0 dB
  lossModel->SetPathLossExponent (10.0); // Very high exponent
  wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                                  "ReferenceDistance", DoubleValue (1.0),
                                  "ReferenceLoss", DoubleValue (0.0),
                                  "Exponent", DoubleValue (10.0));
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Enable 802.11n
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("hidden-nodes-ssid");

  // Setup MPDU Aggregation
  // Use a QosWifiMac to support A-MPDU.
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    "DataMode", StringValue ("HtMcs7"),
    "ControlMode", StringValue ("HtMcs0"),
    "RtsCtsThreshold", UintegerValue (enableRtsCts ? 0 : 9999)
  );
  wifiPhy.Set ("ShortGuardEnabled", BooleanValue (true)); // for highest throughput

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create (2);
  wifiApNode.Create (1);

  // Station MACs
  wifiMac.SetType ("ns3::StaWifiMac",
      "Ssid", SsidValue (ssid),
      "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (wifiPhy, wifiMac, wifiStaNodes);

  // AP MAC
  wifiMac.SetType ("ns3::ApWifiMac",
      "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (wifiPhy, wifiMac, wifiApNode);

  // Configure A-MPDU aggregation
  for (uint32_t i = 0; i < staDevices.GetN (); ++i)
    {
      Ptr<NetDevice> dev = staDevices.Get (i);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (dev);
      Ptr<WifiMac> mac = wifiDev->GetMac ();
      mac->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxAmpdu * payloadSize));
    }
  Ptr<WifiNetDevice> apWifiDev = apDevice.Get (0)->GetObject<WifiNetDevice> ();
  apWifiDev->GetMac ()->SetAttribute ("BE_MaxAmpduSize", UintegerValue (maxAmpdu * payloadSize));

  // Mobility: Arrange AP at (0,0,0), sta1 at (2,0,0), sta2 at (8,0,0)
  // This way, AP is within 5m of both STAs, while STAs are 6m from each other (hidden node)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (2, 0, 0)); // STA 1
  posAlloc->Add (Vector (8, 0, 0)); // STA 2
  wifiStaNodes.Get (0)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  wifiStaNodes.Get (1)->AggregateObject (CreateObject<ConstantPositionMobilityModel> ());
  mobility.SetPositionAllocator (posAlloc);
  mobility.Install (wifiStaNodes);

  posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0, 0, 0)); // AP at (0,0,0)
  mobility.SetPositionAllocator (posAlloc);
  mobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);

  // UDP Applications: saturated UDP from both STAs to AP
  uint16_t port = 5000;
  ApplicationContainer serverApps, clientApps;

  // UDP server on AP
  UdpServerHelper udpServer (port);
  serverApps = udpServer.Install (wifiApNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  // UDP client on STA1
  UdpClientHelper udpClient1 (apInterface.GetAddress (0), port);
  udpClient1.SetAttribute ("MaxPackets", UintegerValue (0)); // Unlimited
  // Generate saturated traffic, set high DataRate, small interval
  udpClient1.SetAttribute ("Interval", TimeValue (Seconds (payloadSize * 8.0 / (150 * 1e6)))); // Assume 150 Mbps
  udpClient1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientApps.Add (udpClient1.Install (wifiStaNodes.Get (0)));

  // UDP client on STA2
  UdpClientHelper udpClient2 (apInterface.GetAddress (0), port);
  udpClient2.SetAttribute ("MaxPackets", UintegerValue (0)); // Unlimited
  udpClient2.SetAttribute ("Interval", TimeValue (Seconds (payloadSize * 8.0 / (150 * 1e6))));
  udpClient2.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientApps.Add (udpClient2.Install (wifiStaNodes.Get (1)));

  clientApps.Start (Seconds (0.1));
  clientApps.Stop (Seconds (simulationTime + 1));

  // Tracing
  wifiPhy.EnablePcap ("wifi-mpdu-aggregation", apDevice.Get (0));
  wifiPhy.EnablePcap ("wifi-mpdu-aggregation-sta1", staDevices.Get (0));
  wifiPhy.EnablePcap ("wifi-mpdu-aggregation-sta2", staDevices.Get (1));

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("wifi-mpdu-aggregation.tr"));

  // Add a packet reception counter on the AP
  Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get (0));
  Config::ConnectWithoutContext ("/NodeList/2/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback (&RxCallback));

  // Run simulation
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Compute throughput
  double throughputMbps = (g_rxBytes * 8.0) / (simulationTime * 1e6);
  std::cout << "==== Throughput statistics ====" << std::endl;
  std::cout << "Simulation time: " << simulationTime << " s" << std::endl;
  std::cout << "Total received bytes: " << g_rxBytes << std::endl;
  std::cout << "Throughput: " << std::fixed << std::setprecision (3) << throughputMbps << " Mbps" << std::endl;

  if (expectedThroughputMbps > 0.0)
    {
      double lower = expectedThroughputMbps * (1.0 - throughputTolerance / 100.0);
      double upper = expectedThroughputMbps * (1.0 + throughputTolerance / 100.0);
      std::cout << "Expected throughput: " << expectedThroughputMbps
                << " Mbps (tolerance: " << throughputTolerance << "%)"
                << std::endl;
      if (throughputMbps < lower || throughputMbps > upper)
        {
          std::cout << "FAILURE: Measured throughput not within expected bounds: ["
                    << lower << ", " << upper << "] Mbps" << std::endl;
          Simulator::Destroy ();
          return 1;
        }
      else
        {
          std::cout << "SUCCESS: Measured throughput within expected bounds." << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}