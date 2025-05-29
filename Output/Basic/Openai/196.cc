#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpStaApExample");

struct StaStats
{
  uint32_t packetsReceived = 0;
  uint64_t bytesReceived = 0;
  Time    startTime;
  Time    endTime;
};

std::map<uint32_t, StaStats> stationStats;

void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &from, uint32_t staId)
{
  stationStats[staId].packetsReceived++;
  stationStats[staId].bytesReceived += packet->GetSize ();
}

void
AppStartTrace (ApplicationContainer apps, uint32_t staId)
{
  stationStats[staId].startTime = Simulator::Now ();
}

void
AppStopTrace (ApplicationContainer apps, uint32_t staId)
{
  stationStats[staId].endTime = Simulator::Now ();
}

int
main (int argc, char *argv[])
{
  uint32_t numSta = 2;
  double simulationTime = 10.0; // seconds
  uint32_t packetSize = 1024;   // bytes
  uint32_t maxPacketCount = 10000; // large enough so app runs full simulation
  double interPacketInterval = 0.01; // seconds

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // WiFi network and node creation
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (numSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // WiFi channel and PHY
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // MAC and SSID
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  Ssid ssid = Ssid ("ns3-80211-example");

  WifiMacHelper mac;

  // Configure stations
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Configure AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility for all nodes
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // UDP Traffic: Each STA sends to AP, AP runs two UDP servers (1 per STA)
  uint16_t portBase = 4000;
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < numSta; ++i)
    {
      UdpServerHelper server (portBase + i);
      ApplicationContainer serverApp = server.Install (wifiApNode.Get (0));
      serverApp.Start (Seconds (1.0));
      serverApp.Stop (Seconds (simulationTime));

      UdpClientHelper client (apInterface.GetAddress (0), portBase + i);
      client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (i));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (simulationTime));

      serverApps.Add (serverApp);
      clientApps.Add (clientApp);

      // Connect receive trace for stats
      Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (serverApp.Get (0));
      uint32_t staId = i;
      udpServer->TraceConnectWithoutContext (
        "Rx",
        MakeBoundCallback (&PacketReceiveCallback, staId));

      Simulator::Schedule (Seconds (2.0), &AppStartTrace, clientApp, staId);
      Simulator::Schedule (Seconds (simulationTime), &AppStopTrace, clientApp, staId);
    }

  // Enable pcap
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-udp-ap", apDevice.Get (0));
  for (uint32_t i = 0; i < numSta; ++i)
    {
      phy.EnablePcap ("wifi-udp-sta" + std::to_string (i), staDevices.Get (i));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  // Display per-station results
  std::cout << "==== Station Statistics ====" << std::endl;
  for (uint32_t i = 0; i < numSta; ++i)
    {
      StaStats &stats = stationStats[i];
      double duration = (stats.endTime - stats.startTime).GetSeconds ();
      double throughput = duration > 0 ? (stats.bytesReceived * 8.0 / duration / 1e6) : 0; // Mbps
      std::cout << "STA " << i
                << " | Received Packets: " << stats.packetsReceived
                << " | Received Bytes: " << stats.bytesReceived
                << " | Throughput: " << throughput << " Mbps"
                << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}